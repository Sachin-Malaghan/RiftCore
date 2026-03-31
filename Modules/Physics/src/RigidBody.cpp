#include <Physics/RigidBody.h>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace RiftCore {

    RigidBody::RigidBody(u32 id, const RigidBodyDesc& desc)
        : id_             (id)
        , position_       (desc.position)
        , velocity_       (desc.velocity)
        , angularVelocity_(desc.angularVelocity)
        , linearDamping_  (desc.linearDamping)
        , angularDamping_ (desc.angularDamping)
        , isStatic_       (desc.isStatic)
        , isKinematic_    (desc.isKinematic)
        , useGravity_     (desc.useGravity)
        , collider_       (desc.collider)
        , isAwake_        (true)
    {
        if (isStatic_) {
            mass_    = 0.0f;
            invMass_ = 0.0f;
        } else {
            mass_    = (desc.mass > 0) ? desc.mass : 1.0f;
            invMass_ = 1.0f / mass_;
        }
    }

    void RigidBody::SetPosition(const Vec3& pos) {
        position_ = pos;
    }

    void RigidBody::SetVelocity(const Vec3& vel) {
        velocity_ = vel;
        isAwake_  = true;
    }

    void RigidBody::SetAngularVelocity(const Vec3& av) {
        angularVelocity_ = av;
    }

    void RigidBody::ApplyForce(const Vec3& force) {
        if (isStatic_ || isKinematic_) return;
        accumulatedForce_.x += force.x;
        accumulatedForce_.y += force.y;
        accumulatedForce_.z += force.z;
        isAwake_ = true;
    }

    void RigidBody::ApplyImpulse(const Vec3& impulse) {
        if (isStatic_ || isKinematic_) return;
        velocity_.x += impulse.x * invMass_;
        velocity_.y += impulse.y * invMass_;
        velocity_.z += impulse.z * invMass_;
        isAwake_ = true;
    }

    void RigidBody::ApplyTorque(const Vec3& torque) {
        if (isStatic_ || isKinematic_) return;
        accumulatedTorque_.x += torque.x;
        accumulatedTorque_.y += torque.y;
        accumulatedTorque_.z += torque.z;
    }

    void RigidBody::ClearForces() {
        accumulatedForce_   = Vec3::Zero();
        accumulatedTorque_  = Vec3::Zero();
    }

    void RigidBody::Integrate(f32 dt, const Vec3& gravity) {
        if (isStatic_ || isKinematic_ || !isAwake_) return;

        // Apply gravity force
        if (useGravity_) {
            accumulatedForce_.x += gravity.x * mass_;
            accumulatedForce_.y += gravity.y * mass_;
            accumulatedForce_.z += gravity.z * mass_;
        }

        // Compute acceleration
        Vec3 accel = {
            accumulatedForce_.x * invMass_,
            accumulatedForce_.y * invMass_,
            accumulatedForce_.z * invMass_
        };

        // Semi-implicit Euler
        velocity_.x += accel.x * dt;
        velocity_.y += accel.y * dt;
        velocity_.z += accel.z * dt;

        // Velocity cap to prevent tunneling
        const f32 maxSpeed = 30.0f;
        f32 speed = std::sqrt(
            velocity_.x * velocity_.x +
            velocity_.y * velocity_.y +
            velocity_.z * velocity_.z);
        if (speed > maxSpeed) {
            f32 s    = maxSpeed / speed;
            velocity_.x *= s;
            velocity_.y *= s;
            velocity_.z *= s;
        }

        // Linear damping
        f32 ld = std::max(0.0f, 1.0f - linearDamping_ * dt);
        velocity_.x *= ld;
        velocity_.y *= ld;
        velocity_.z *= ld;

        // Integrate position
        position_.x += velocity_.x * dt;
        position_.y += velocity_.y * dt;
        position_.z += velocity_.z * dt;

        // Angular velocity from torque (simplified)
        angularVelocity_.x +=
            accumulatedTorque_.x * invMass_ * dt;
        angularVelocity_.y +=
            accumulatedTorque_.y * invMass_ * dt;
        angularVelocity_.z +=
            accumulatedTorque_.z * invMass_ * dt;

        // Angular speed cap
        const f32 maxAngSpeed = 10.0f;
        f32 angSpeed = std::sqrt(
            angularVelocity_.x * angularVelocity_.x +
            angularVelocity_.y * angularVelocity_.y +
            angularVelocity_.z * angularVelocity_.z);
        if (angSpeed > maxAngSpeed) {
            f32 s = maxAngSpeed / angSpeed;
            angularVelocity_.x *= s;
            angularVelocity_.y *= s;
            angularVelocity_.z *= s;
        }

        // Angular damping
        f32 ad = std::max(0.0f, 1.0f - angularDamping_ * dt);
        angularVelocity_.x *= ad;
        angularVelocity_.y *= ad;
        angularVelocity_.z *= ad;

        // Integrate rotation
        rotation_.x += angularVelocity_.x * dt;
        rotation_.y += angularVelocity_.y * dt;
        rotation_.z += angularVelocity_.z * dt;

        ClearForces();

        // Sleep check
        f32 kineticEnergy =
            velocity_.x * velocity_.x +
            velocity_.y * velocity_.y +
            velocity_.z * velocity_.z;

        if (kineticEnergy < 0.01f) {
            sleepTimer_ += dt;
            if (sleepTimer_ > 2.0f) {
                velocity_        = Vec3::Zero();
                angularVelocity_ = Vec3::Zero();
                isAwake_         = false;
            }
        } else {
            sleepTimer_ = 0.0f;
        }
    }

    AABB RigidBody::GetAABB() const {
        AABB aabb;
        const ColliderDesc& col = collider_;

        if (col.shape == ColliderShape::Sphere) {
            f32 r    = col.radius;
            aabb.min = {
                position_.x - r,
                position_.y - r,
                position_.z - r
            };
            aabb.max = {
                position_.x + r,
                position_.y + r,
                position_.z + r
            };
        }
        else if (col.shape == ColliderShape::Box) {
            aabb.min = {
                position_.x - col.halfExtents.x,
                position_.y - col.halfExtents.y,
                position_.z - col.halfExtents.z
            };
            aabb.max = {
                position_.x + col.halfExtents.x,
                position_.y + col.halfExtents.y,
                position_.z + col.halfExtents.z
            };
        }
        else if (col.shape == ColliderShape::Plane) {
            // Plane at planeOffset along planeNormal
            // Give it a large AABB above the plane
            f32 y    = col.planeOffset;
            aabb.min = {-1000.0f, y - 1.0f, -1000.0f};
            aabb.max = { 1000.0f, y + 1000.0f,  1000.0f};
        }

        return aabb;
    }

} // namespace RiftCore
