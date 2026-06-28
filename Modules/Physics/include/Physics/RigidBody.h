/*──────────────────────────────────────────────────────────────────────────
 *  RigidBody.h  –  Single rigid-body representation
 *
 *  Upgrades over the previous revision:
 *    • Orientation stored as a Quat (no gimbal lock).
 *    • Full 3×3 inertia tensor computed from shape + mass, transformed
 *      to world space every integration step.
 *    • Per-body PhysicsMaterial for fine-grained surface control.
 *    • Collision-layer + mask for filtering.
 *    • Oriented AABB that correctly encloses rotated boxes / capsules.
 *    • Configurable velocity caps, sleep thresholds, CCD toggle.
 *    • Torque impulse support (ApplyTorqueImpulse).
 *    • Force-at-point for realistic torque generation.
 *──────────────────────────────────────────────────────────────────────────*/
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

        /// Construct from the SDK-level descriptor.
        /// Computes mass, inverse mass, local inertia tensor and its
        /// inverse from the collider shape automatically.
        RigidBody(u32 id, const RigidBodyDesc& desc);

        /* ── move-only (stored in std::vector) ───────────────── */
        RigidBody(RigidBody&&)            = default;
        RigidBody& operator=(RigidBody&&) = default;
        RigidBody(const RigidBody&)            = delete;
        RigidBody& operator=(const RigidBody&) = delete;

        /* ════════════════════════════════════════════════════════
         *  Accessors  –  read-only queries
         * ════════════════════════════════════════════════════════*/

        u32  GetID()        const { return id_;          }
        bool IsStatic()     const { return isStatic_;    }
        bool IsKinematic()  const { return isKinematic_; }
        bool IsAwake()      const { return isAwake_;     }
        bool UsesGravity()  const { return useGravity_;  }
        bool IsCCDEnabled() const { return ccdEnabled_;  }

        const Vec3& GetPosition()   const { return position_;        }
        const Quat& GetOrientation()const { return orientation_;     }
        const Vec3& GetVelocity()   const { return velocity_;        }
        const Vec3& GetAngularVel() const { return angularVelocity_; }

        /// Euler-angle rotation (legacy, derived from quaternion).
        Vec3 GetRotationEuler() const;

        f32  GetMass()       const { return mass_;    }
        f32  GetInvMass()    const { return invMass_; }

        /// World-space inverse inertia tensor (updated every Integrate).
        const Mat3& GetWorldInvInertia() const { return worldInvInertia_; }

        /// Local-space inverse inertia tensor (constant for life of body).
        const Mat3& GetLocalInvInertia() const { return localInvInertia_; }

        CollisionLayer GetLayer() const { return layer_; }
        CollisionMask  GetMask()  const { return mask_;  }

        const PhysicsMaterial& GetMaterial() const { return material_; }

        /* ════════════════════════════════════════════════════════
         *  Mutators
         * ════════════════════════════════════════════════════════*/

        void SetPosition(const Vec3& pos);
        void SetOrientation(const Quat& q);
        void SetVelocity(const Vec3& vel);
        void SetAngularVelocity(const Vec3& av);

        void SetLayer(CollisionLayer l) { layer_ = l; }
        void SetMask(CollisionMask  m)  { mask_  = m; }
        void SetMaterial(const PhysicsMaterial& mat) { material_ = mat; }
        void EnableCCD(bool on) { ccdEnabled_ = on; }

        /* ════════════════════════════════════════════════════════
         *  Force / impulse API
         * ════════════════════════════════════════════════════════*/

        /// Accumulate a force (applied at centre-of-mass).
        void ApplyForce(const Vec3& force);

        /// Accumulate a force applied at a world-space point.
        /// Generates both linear force and torque.
        void ApplyForceAtPoint(const Vec3& force, const Vec3& worldPoint);

        /// Instantaneous change in linear momentum (mass-scaled internally).
        void ApplyImpulse(const Vec3& impulse);

        /// Impulse applied at a world-space point (generates angular impulse too).
        void ApplyImpulseAtPoint(const Vec3& impulse, const Vec3& worldPoint);

        /// Pure torque (no linear effect).
        void ApplyTorque(const Vec3& torque);

        /// Instantaneous angular impulse (bypasses mass, directly changes ω).
        void ApplyTorqueImpulse(const Vec3& impulse);

        void ClearForces();

        /* ════════════════════════════════════════════════════════
         *  Collider & AABB
         * ════════════════════════════════════════════════════════*/

        const ColliderDesc& GetCollider() const { return collider_; }

        /// Compute an oriented AABB that tightly encloses the rotated
        /// collider shape.
        AABB GetAABB() const;

        /* ════════════════════════════════════════════════════════
         *  Sleep system
         * ════════════════════════════════════════════════════════*/

        void Wake();
        void Sleep();

        /* ════════════════════════════════════════════════════════
         *  Integration  –  called by PhysicsWorld::Step()
         * ════════════════════════════════════════════════════════*/

        /// Semi-implicit Euler integration:
        ///   1. Accumulate gravity.
        ///   2. Compute linear + angular acceleration.
        ///   3. Update velocity, apply damping & caps.
        ///   4. Update position and quaternion orientation.
        ///   5. Recompute world-space inertia tensor.
        ///   6. Auto-sleep check.
        void Integrate(f32 dt, const Vec3& gravity);

    private:
        /* ── identity ─────────────────────────────────────────── */
        u32          id_               = 0;

        /* ── spatial state ────────────────────────────────────── */
        Vec3         position_         = Vec3::Zero();
        Quat         orientation_      = Quat::Identity();
        Vec3         velocity_         = Vec3::Zero();
        Vec3         angularVelocity_  = Vec3::Zero();

        /* ── force accumulators (cleared each sub-step) ───────── */
        Vec3         accumulatedForce_ = Vec3::Zero();
        Vec3         accumulatedTorque_= Vec3::Zero();

        /* ── mass properties ──────────────────────────────────── */
        f32          mass_             = 1.0f;
        f32          invMass_          = 1.0f;
        Mat3         localInertia_     = Mat3::Identity();
        Mat3         localInvInertia_  = Mat3::Identity();
        Mat3         worldInvInertia_  = Mat3::Identity();

        /* ── damping ──────────────────────────────────────────── */
        f32          linearDamping_    = 0.01f;
        f32          angularDamping_   = 0.05f;

        /* ── caps ─────────────────────────────────────────────── */
        f32          maxLinearSpeed_   = 100.0f;
        f32          maxAngularSpeed_  = 50.0f;

        /* ── flags ────────────────────────────────────────────── */
        bool         isStatic_         = false;
        bool         isKinematic_      = false;
        bool         useGravity_       = true;
        bool         isAwake_          = true;
        bool         ccdEnabled_       = false;

        /* ── sleep system ─────────────────────────────────────── */
        f32          sleepTimer_       = 0.0f;
        f32          sleepThreshold_   = 0.005f;   ///< Kinetic energy threshold.
        f32          sleepDelay_       = 1.0f;     ///< Seconds below threshold.

        /* ── collision ────────────────────────────────────────── */
        ColliderDesc     collider_;
        PhysicsMaterial  material_;
        CollisionLayer   layer_ = CollisionLayers::All;
        CollisionMask    mask_  = CollisionLayers::All;

        /* ── helpers ──────────────────────────────────────────── */
        void ComputeInertia();
        void UpdateWorldInertia();
    };

} // namespace RiftCore

#pragma warning(pop)
