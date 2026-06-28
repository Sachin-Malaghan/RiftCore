/*──────────────────────────────────────────────────────────────────────────
 *  RigidBody.cpp  –  Implementation of the RigidBody class
 *
 *  Key improvements:
 *    • Proper inertia-tensor computation for Sphere, Box, Capsule, Plane.
 *    • World-space inertia updated every integration step via R·I⁻¹·Rᵀ.
 *    • Quaternion integration for orientation (no Euler angles).
 *    • Force-at-point generates correct torque (r × F).
 *    • Oriented AABB for rotated boxes and capsules.
 *    • Configurable velocity caps prevent energy explosion.
 *    • Energy-based auto-sleep with configurable threshold & delay.
 *──────────────────────────────────────────────────────────────────────────*/
#include <Physics/RigidBody.h>
#include <cmath>
#include <algorithm>




namespace RiftCore {

    using namespace PhysMath;

    /* ──────────────────────────────────────────────────────────────
     *  Constructor
     *
     *  Copies every field from the SDK RigidBodyDesc, then computes
     *  mass, inverse mass, local inertia tensor, its inverse, and
     *  the initial world-space inverse inertia.  Static / kinematic
     *  bodies get zero inverse mass and zero inverse inertia so
     *  impulses never move them.
     * ────────────────────────────────────────────────────────────── */
    RigidBody::RigidBody(u32 id, const RigidBodyDesc& desc)
        : id_              (id)
        , position_        (desc.position)
        , orientation_     (Quat::Identity())
        , velocity_        (desc.velocity)
        , angularVelocity_ (desc.angularVelocity)
        , linearDamping_   (desc.linearDamping)
        , angularDamping_  (desc.angularDamping)
        , isStatic_        (desc.isStatic)
        , isKinematic_     (desc.isKinematic)
        , useGravity_      (desc.useGravity)
        , collider_        (desc.collider)
        , isAwake_         (true)
    {
        // ── Mass ──────────────────────────────────────────────
        if (isStatic_ || isKinematic_) {
            mass_    = 0.0f;
            invMass_ = 0.0f;
        } else {
            mass_    = (desc.mass > 0.0f) ? desc.mass : 1.0f;
            invMass_ = 1.0f / mass_;
        }

        // ── Material defaults from collider desc ──────────────
        material_.restitution     = desc.collider.restitution;
        material_.staticFriction  = desc.collider.friction;
        material_.dynamicFriction = desc.collider.friction * 0.8f;

        // ── Collision layer auto-assignment ───────────────────
        if (isStatic_)         layer_ = CollisionLayers::Static;
        else if (isKinematic_) layer_ = CollisionLayers::Kinematic;
        else                   layer_ = CollisionLayers::Dynamic;

        // ── Inertia tensor ────────────────────────────────────
        ComputeInertia();
        UpdateWorldInertia();
    }

    /* ──────────────────────────────────────────────────────────────
     *  ComputeInertia()
     *
     *  Builds the local-space inertia tensor from the collider shape
     *  and the body mass.  Also stores the inverse.
     *
     *  Formulae:
     *    Sphere  :  I = diag( 2/5 · m · r² )
     *    Box     :  I_x = m/12·(h²+d²),  I_y = m/12·(w²+d²), …
     *    Capsule :  Cylinder + hemisphere decomposition.
     *    Plane   :  Infinite mass → zero inverse (immovable).
     * ────────────────────────────────────────────────────────────── */
    void RigidBody::ComputeInertia() {
        if (isStatic_ || isKinematic_) {
            localInertia_    = Mat3::Zero();
            localInvInertia_ = Mat3::Zero();
            return;
        }

        switch (collider_.shape) {
            case ColliderShape::Sphere:
                localInertia_ = PhysMath::SphereInertia(mass_, collider_.radius);
                break;

            case ColliderShape::Box:
                localInertia_ = PhysMath::BoxInertia(mass_, collider_.halfExtents);
                break;

            case ColliderShape::Capsule:
                localInertia_ = PhysMath::CapsuleInertia(
                    mass_, collider_.radius, collider_.halfExtents.y);
                break;

            case ColliderShape::Plane:
            default:
                localInertia_    = Mat3::Zero();
                localInvInertia_ = Mat3::Zero();
                return;
        }

        localInvInertia_ = localInertia_.Inverse();
    }

    /* ──────────────────────────────────────────────────────────────
     *  UpdateWorldInertia()
     *
     *  Transforms the local inverse inertia into world space:
     *      I⁻¹_world  =  R · I⁻¹_local · Rᵀ
     *
     *  Must be called whenever the orientation changes (i.e. every
     *  integration sub-step).
     * ────────────────────────────────────────────────────────────── */
    void RigidBody::UpdateWorldInertia() {
        if (isStatic_ || isKinematic_) {
            worldInvInertia_ = Mat3::Zero();
            return;
        }
        Mat3 R  = QuatToMat3(orientation_);
        Mat3 Rt = R.Transposed();
        worldInvInertia_ = R * localInvInertia_ * Rt;
    }

    /* ──────────────────────────────────────────────────────────────
     *  Euler-angle helper (read-only convenience for the renderer)
     * ────────────────────────────────────────────────────────────── */
    Vec3 RigidBody::GetRotationEuler() const {
        // Extract YXZ Euler from quaternion
        f32 sinP = 2.0f * (orientation_.w * orientation_.x -
                           orientation_.y * orientation_.z);
        sinP = Clamp(sinP, -1.0f, 1.0f);
        f32 pitch = std::asin(sinP);

        f32 sinY = 2.0f * (orientation_.w * orientation_.y +
                           orientation_.x * orientation_.z);
        f32 cosY = 1.0f - 2.0f * (orientation_.x * orientation_.x +
                                    orientation_.y * orientation_.y);
        f32 yaw  = std::atan2(sinY, cosY);

        f32 sinR = 2.0f * (orientation_.w * orientation_.z +
                           orientation_.x * orientation_.y);
        f32 cosR = 1.0f - 2.0f * (orientation_.x * orientation_.x +
                                    orientation_.z * orientation_.z);
        f32 roll = std::atan2(sinR, cosR);

        return {pitch, yaw, roll};
    }

    /* ──────────────────────────────────────────────────────────────
     *  Setters
     * ────────────────────────────────────────────────────────────── */

    void RigidBody::SetPosition(const Vec3& pos) {
        position_ = pos;
    }

    void RigidBody::SetOrientation(const Quat& q) {
        orientation_ = q.Normalized();
        UpdateWorldInertia();
    }

    void RigidBody::SetVelocity(const Vec3& vel) {
        velocity_ = vel;
        if (LengthSq(vel) > 0.0001f) Wake();
    }

    void RigidBody::SetAngularVelocity(const Vec3& av) {
        angularVelocity_ = av;
        if (LengthSq(av) > 0.0001f) Wake();
    }

    /* ──────────────────────────────────────────────────────────────
     *  Force / Impulse application
     *
     *  Forces are accumulated over a sub-step and converted to
     *  acceleration during Integrate().  Impulses change velocity
     *  immediately (they represent an instantaneous momentum change).
     * ────────────────────────────────────────────────────────────── */

    void RigidBody::ApplyForce(const Vec3& force) {
        if (isStatic_ || isKinematic_) return;
        accumulatedForce_ = Add(accumulatedForce_, force);
        Wake();
    }

    /// Force at a world point:
    ///   linear  +=  F
    ///   torque  +=  (point − CoM) × F
    void RigidBody::ApplyForceAtPoint(const Vec3& force,
                                       const Vec3& worldPoint) {
        if (isStatic_ || isKinematic_) return;
        accumulatedForce_ = Add(accumulatedForce_, force);
        Vec3 r = Sub(worldPoint, position_);
        accumulatedTorque_ = Add(accumulatedTorque_, Cross(r, force));
        Wake();
    }

    void RigidBody::ApplyImpulse(const Vec3& impulse) {
        if (isStatic_ || isKinematic_) return;
        velocity_ = Add(velocity_, Scale(impulse, invMass_));
        Wake();
    }

    /// Impulse at a world point:
    ///   Δv  +=  impulse / m
    ///   Δω  +=  I⁻¹_world · (r × impulse)
    void RigidBody::ApplyImpulseAtPoint(const Vec3& impulse,
                                         const Vec3& worldPoint) {
        if (isStatic_ || isKinematic_) return;
        velocity_ = Add(velocity_, Scale(impulse, invMass_));
        Vec3 r     = Sub(worldPoint, position_);
        Vec3 dOmeg = worldInvInertia_ * Cross(r, impulse);
        angularVelocity_ = Add(angularVelocity_, dOmeg);
        Wake();
    }

    void RigidBody::ApplyTorque(const Vec3& torque) {
        if (isStatic_ || isKinematic_) return;
        accumulatedTorque_ = Add(accumulatedTorque_, torque);
    }

    void RigidBody::ApplyTorqueImpulse(const Vec3& impulse) {
        if (isStatic_ || isKinematic_) return;
        Vec3 dOmeg = worldInvInertia_ * impulse;
        angularVelocity_ = Add(angularVelocity_, dOmeg);
        Wake();
    }

    void RigidBody::ClearForces() {
        accumulatedForce_  = Vec3::Zero();
        accumulatedTorque_ = Vec3::Zero();
    }

    /* ──────────────────────────────────────────────────────────────
     *  Integrate()  –  Semi-implicit Euler
     *
     *  Order matters for stability:
     *    1.  Accumulate gravity as a force (F += m·g).
     *    2.  Compute linear acceleration  a = F / m.
     *    3.  Update velocity:  v += a·dt        (semi-implicit: new v
     *        is used for the position step).
     *    4.  Damping:  v *= (1 − λ·dt).
     *    5.  Cap to maxLinearSpeed_.
     *    6.  Position:  x += v·dt.
     *    7.  Angular acceleration:  α = I⁻¹_world · τ.
     *    8.  Angular velocity:  ω += α·dt,  damped & capped.
     *    9.  Quaternion:  q += ½·(0,ω)·q · dt,  renormalise.
     *   10.  Recompute world-space inertia.
     *   11.  Clear force accumulators.
     *   12.  Auto-sleep energy check.
     * ────────────────────────────────────────────────────────────── */
    void RigidBody::Integrate(f32 dt, const Vec3& gravity) {
        if (isStatic_ || isKinematic_ || !isAwake_) return;

        // 1. Gravity
        if (useGravity_) {
            accumulatedForce_ = Add(accumulatedForce_,
                                    Scale(gravity, mass_));
        }

        // 2-3. Linear acceleration → velocity
        Vec3 accel = Scale(accumulatedForce_, invMass_);
        velocity_  = Add(velocity_, Scale(accel, dt));

        // 4. Linear damping  (exponential decay model)
        f32 ld = std::max(0.0f, 1.0f - linearDamping_ * dt);
        velocity_ = Scale(velocity_, ld);

        // 5. Speed cap
        f32 speed = Length(velocity_);
        if (speed > maxLinearSpeed_)
            velocity_ = Scale(velocity_, maxLinearSpeed_ / speed);

        // 6. Position
        position_ = Add(position_, Scale(velocity_, dt));

        // 7. Angular acceleration  α = I⁻¹_world · τ
        Vec3 angAccel = worldInvInertia_ * accumulatedTorque_;

        // 8. Angular velocity
        angularVelocity_ = Add(angularVelocity_, Scale(angAccel, dt));
        f32 ad = std::max(0.0f, 1.0f - angularDamping_ * dt);
        angularVelocity_ = Scale(angularVelocity_, ad);

        f32 angSpeed = Length(angularVelocity_);
        if (angSpeed > maxAngularSpeed_)
            angularVelocity_ = Scale(angularVelocity_,
                                      maxAngularSpeed_ / angSpeed);

        // 9. Quaternion integration
        orientation_.IntegrateAngularVelocity(angularVelocity_, dt);

        // 10. World inertia
        UpdateWorldInertia();

        // 11. Clear accumulators
        ClearForces();

        // 12. Auto-sleep  (based on total kinetic energy)
        f32 linKE = LengthSq(velocity_);
        f32 angKE = LengthSq(angularVelocity_);
        f32 totalKE = linKE + angKE;

        if (totalKE < sleepThreshold_) {
            sleepTimer_ += dt;
            if (sleepTimer_ >= sleepDelay_) {
                velocity_        = Vec3::Zero();
                angularVelocity_ = Vec3::Zero();
                isAwake_ = false;
            }
        } else {
            sleepTimer_ = 0.0f;
        }
    }

    /* ──────────────────────────────────────────────────────────────
     *  Sleep / Wake
     * ────────────────────────────────────────────────────────────── */

    void RigidBody::Wake() {
        isAwake_    = true;
        sleepTimer_ = 0.0f;
    }

    void RigidBody::Sleep() {
        isAwake_         = false;
        velocity_        = Vec3::Zero();
        angularVelocity_ = Vec3::Zero();
    }

    /* ──────────────────────────────────────────────────────────────
     *  GetAABB()  –  Oriented bounding box
     *
     *  For spheres the AABB is trivial (position ± radius).
     *
     *  For boxes we rotate each local half-extent axis by the current
     *  quaternion, then take the absolute-value to find the world-
     *  axis-aligned extent — this is the standard "OBB → AABB"
     *  transformation.
     *
     *  For capsules we treat it as a cylinder (rotated Y-axis) plus
     *  a sphere sweep of the radius.
     *
     *  For planes we return a very large AABB (broadphase will still
     *  test only overlapping pairs).
     * ────────────────────────────────────────────────────────────── */
    AABB RigidBody::GetAABB() const {
        AABB aabb;
        const ColliderDesc& col = collider_;

        switch (col.shape) {

        case ColliderShape::Sphere: {
            f32 r = col.radius;
            aabb.min = {position_.x - r, position_.y - r, position_.z - r};
            aabb.max = {position_.x + r, position_.y + r, position_.z + r};
            break;
        }

        case ColliderShape::Box: {
            // Rotate each axis of the half-extents and take abs
            Mat3 rot = QuatToMat3(orientation_);
            Vec3 he  = col.halfExtents;
            f32 ex = std::abs(rot.m[0][0])*he.x + std::abs(rot.m[0][1])*he.y + std::abs(rot.m[0][2])*he.z;
            f32 ey = std::abs(rot.m[1][0])*he.x + std::abs(rot.m[1][1])*he.y + std::abs(rot.m[1][2])*he.z;
            f32 ez = std::abs(rot.m[2][0])*he.x + std::abs(rot.m[2][1])*he.y + std::abs(rot.m[2][2])*he.z;
            aabb.min = {position_.x - ex, position_.y - ey, position_.z - ez};
            aabb.max = {position_.x + ex, position_.y + ey, position_.z + ez};
            break;
        }

        case ColliderShape::Capsule: {
            // Capsule is Y-aligned in local space; rotate the Y-axis
            Vec3 localTop = {0, col.halfExtents.y, 0};
            Vec3 worldTop = orientation_.Rotate(localTop);
            f32 r = col.radius;
            // Endpoints of the capsule line segment
            Vec3 a = Add(position_, worldTop);
            Vec3 b = Sub(position_, worldTop);
            aabb.min = {std::min(a.x, b.x) - r, std::min(a.y, b.y) - r, std::min(a.z, b.z) - r};
            aabb.max = {std::max(a.x, b.x) + r, std::max(a.y, b.y) + r, std::max(a.z, b.z) + r};
            break;
        }

        case ColliderShape::Plane:
        default: {
            f32 y = col.planeOffset;
            aabb.min = {-10000.0f, y - 1.0f,  -10000.0f};
            aabb.max = { 10000.0f, y + 10000.0f, 10000.0f};
            break;
        }
        }

        return aabb;
    }

} // namespace RiftCore
