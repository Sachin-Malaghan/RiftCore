#include <Physics/RigidBody.h>
#include <cmath>
#include <algorithm>

namespace RiftCore {

    RigidBody::RigidBody(u32 id, const RigidBodyDesc& desc)
        : id_(id)
        , position_(desc.position)
        , velocity_(desc.velocity)
        , angularVelocity_(desc.angularVelocity)
        , linearDamping_(desc.linearDamping)
        , angularDamping_(desc.angularDamping)
        , isStatic_(desc.isStatic)
        , isKinematic_(desc.isKinematic)
        , useGravity_(desc.useGravity)
        , collider_(desc.collider)
    {
        if (isStatic_) {
            mass_    = 0.0f;
            invMass_ = 0.0f;
            isAwake_ = true;  // static bodies always awake
        } else {
            mass_    = desc.mass > 0 ? desc.mass : 1.0f;
            invMass_ = 1.0f / mass_;
            isAwake_ = true;
        }
    }

    void RigidBody::SetPosition(const Vec3& pos) {
        position_ = pos;
        Wake();
    }

    void RigidBody::SetVelocity(const Vec3& vel) {
        velocity_ = vel;
        Wake();
    }

    void RigidBody::SetAngularVelocity(const Vec3& av) {
        angularVelocity_ = av;
    }

    void RigidBody::ApplyForce(const Vec3& force) {
        if (isStatic_ || isKinematic_) return;
        accumulatedForce_.x += force.x;
        accumulatedForce_.y += force.y;
        accumulatedForce_.z += force.z;
        Wake();
    }

    void RigidBody::ApplyImpulse(const Vec3& impulse) {
        if (isStatic_ || isKinematic_) return;
        velocity_.x += impulse.x * invMass_;
        velocity_.y += impulse.y * invMass_;
        velocity_.z += impulse.z * invMass_;
        Wake();
    }

    void RigidBody::ApplyTorque(const Vec3& torque) {
        if (isStatic_ || isKinematic_) return;
        accumulatedTorque_.x += torque.x;
        accumulatedTorque_.y += torque.y;
        accumulatedTorque_.z += torque.z;
    }

    void RigidBody::ClearForces() {
        accumulatedForce_  = Vec3::Zero();
        accumulatedTorque_ = Vec3::Zero();
    }

    void RigidBody::Integrate(f32 dt, const Vec3& gravity) {
        if (isStatic_ || isKinematic_ || !isAwake_) return;

        // Apply gravity
        if (useGravity_) {
            accumulatedForce_.x += gravity.x * mass_;
            accumulatedForce_.y += gravity.y * mass_;
            accumulatedForce_.z += gravity.z * mass_;
        }

        // Linear acceleration = F / m
        Vec3 acceleration = {
            accumulatedForce_.x * invMass_,
            accumulatedForce_.y * invMass_,
            accumulatedForce_.z * invMass_
        };

        // Semi-implicit Euler
        velocity_.x += acceleration.x * dt;
        velocity_.y += acceleration.y * dt;
        velocity_.z += acceleration.z * dt;

        // ── CCD velocity clamping ─────────────────────────
        // Limit max velocity per frame to prevent tunneling
        // Max distance per step = half of smallest collider
        // For radius 0.5 sphere: max = 0.5 / dt
        const f32 maxVelPerStep = 20.0f;
        f32 speedSq =
            velocity_.x * velocity_.x +
            velocity_.y * velocity_.y +
            velocity_.z * velocity_.z;
        if (speedSq > maxVelPerStep * maxVelPerStep) {
            f32 speed    = std::sqrt(speedSq);
            f32 scale    = maxVelPerStep / speed;
            velocity_.x *= scale;
            velocity_.y *= scale;
            velocity_.z *= scale;
        }

        // Apply linear damping
        f32 linDamp = 1.0f - linearDamping_ * dt;
        if (linDamp < 0.0f) linDamp = 0.0f;
        velocity_.x *= linDamp;
        velocity_.y *= linDamp;
        velocity_.z *= linDamp;

        // Update position
        position_.x += velocity_.x * dt;
        position_.y += velocity_.y * dt;
        position_.z += velocity_.z * dt;

        // Angular velocity from torque
        angularVelocity_.x += accumulatedTorque_.x
                               * invMass_ * dt;
        angularVelocity_.y += accumulatedTorque_.y
                               * invMass_ * dt;
        angularVelocity_.z += accumulatedTorque_.z
                               * invMass_ * dt;

        // Clamp angular velocity to prevent spinning chaos
        const f32 maxAngVel = 15.0f;
        f32 angSpeedSq =
            angularVelocity_.x * angularVelocity_.x +
            angularVelocity_.y * angularVelocity_.y +
            angularVelocity_.z * angularVelocity_.z;
        if (angSpeedSq > maxAngVel * maxAngVel) {
            f32 angSpeed = std::sqrt(angSpeedSq);
            f32 s        = maxAngVel / angSpeed;
            angularVelocity_.x *= s;
            angularVelocity_.y *= s;
            angularVelocity_.z *= s;
        }

        // Apply angular damping
        f32 angDamp = 1.0f - angularDamping_ * dt;
        if (angDamp < 0.0f) angDamp = 0.0f;
        angularVelocity_.x *= angDamp;
        angularVelocity_.y *= angDamp;
        angularVelocity_.z *= angDamp;

        // Update rotation
        rotation_.x += angularVelocity_.x * dt;
        rotation_.y += angularVelocity_.y * dt;
        rotation_.z += angularVelocity_.z * dt;

        ClearForces();

        // Sleep check - use speed squared
        if (speedSq < sleepThreshold_ * sleepThreshold_) {
            sleepTimer_ += dt;
            if (sleepTimer_ >= sleepDelay_) {
                // Only sleep if really not moving
                velocity_        = Vec3::Zero();
                angularVelocity_ = Vec3::Zero();
                Sleep();
            }
        } else {
            sleepTimer_ = 0;
        }
    }

    AABB RigidBody::GetAABB() const {
        AABB aabb;
        const auto& col = collider_;

        if (col.shape == ColliderShape::Sphere) {
            f32 r = col.radius;
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
            // Plane AABB: infinite in XZ, extends upward
            // planeOffset = Y position of plane
            f32 planeY = col.planeOffset;
            aabb.min = {-500.0f, planeY - 0.1f, -500.0f};
            aabb.max = { 500.0f, planeY + 500.0f, 500.0f};
        }
        return aabb;
    }

} // namespace RiftCore





