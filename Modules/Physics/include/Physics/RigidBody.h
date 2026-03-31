#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <Physics/PhysicsTypes.h>
#include <cmath>

#ifdef PHYSICS_EXPORTS
    #define PHYSICS_API RIFTCORE_EXPORT
#else
    #define PHYSICS_API RIFTCORE_IMPORT
#endif

namespace RiftCore {

    class PHYSICS_API RigidBody {
    public:
        RigidBody() = default;
        RigidBody(u32 id, const RigidBodyDesc& desc);

        // Allow move (needed for vector storage)
        RigidBody(RigidBody&&)            = default;
        RigidBody& operator=(RigidBody&&) = default;

        // No copy
        RigidBody(const RigidBody&)            = delete;
        RigidBody& operator=(const RigidBody&) = delete;

        u32  GetID()        const { return id_;         }
        bool IsStatic()     const { return isStatic_;   }
        bool IsKinematic()  const { return isKinematic_;}
        bool IsAwake()      const { return isAwake_;    }
        bool UsesGravity()  const { return useGravity_; }

        const Vec3& GetPosition()   const { return position_;        }
        const Vec3& GetVelocity()   const { return velocity_;        }
        const Vec3& GetAngularVel() const { return angularVelocity_; }
        const Vec3& GetRotation()   const { return rotation_;        }
        f32         GetMass()       const { return mass_;            }
        f32         GetInvMass()    const { return invMass_;         }

        void SetPosition(const Vec3& pos);
        void SetVelocity(const Vec3& vel);
        void SetAngularVelocity(const Vec3& av);

        void ApplyForce  (const Vec3& force);
        void ApplyImpulse(const Vec3& impulse);
        void ApplyTorque (const Vec3& torque);
        void ClearForces ();

        const ColliderDesc& GetCollider() const {
            return collider_;
        }

        AABB GetAABB() const;

        void Wake()  { isAwake_ = true;  sleepTimer_ = 0; }
        void Sleep() { isAwake_ = false; }

        void Integrate(f32 dt, const Vec3& gravity);

    private:
        u32          id_               = 0;
        Vec3         position_         = Vec3::Zero();
        Vec3         velocity_         = Vec3::Zero();
        Vec3         angularVelocity_  = Vec3::Zero();
        Vec3         rotation_         = Vec3::Zero();
        Vec3         accumulatedForce_ = Vec3::Zero();
        Vec3         accumulatedTorque_= Vec3::Zero();

        f32          mass_             = 1.0f;
        f32          invMass_          = 1.0f;
        f32          linearDamping_    = 0.01f;
        f32          angularDamping_   = 0.05f;

        bool         isStatic_         = false;
        bool         isKinematic_      = false;
        bool         useGravity_       = true;
        bool         isAwake_          = true;

        f32          sleepTimer_       = 0.0f;
        f32          sleepThreshold_   = 0.02f;
        f32          sleepDelay_       = 2.0f;

        ColliderDesc collider_;
    };

} // namespace RiftCore

#pragma warning(pop)

