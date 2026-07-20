/*──────────────────────────────────────────────────────────────────────────
 *  PhysicsWorld.h  –  Core simulation world, system-impl, and module
 *
 *  Preserved public API (method names & signatures) so the rest of
 *  the engine / DLL boundary is not affected.
 *
 *  What changed internally:
 *    • Spatial-hash broadphase (replaces O(n²) brute-force).
 *    • Sequential-Impulse (SI) velocity solver with warm-starting.
 *    • Separate broadPhase / narrowPhase / solver timing stats.
 *    • Capsule ↔ everything collision dispatch.
 *    • Constraint / joint system (distance, hinge, ball-socket, spring).
 *    • Manifold caching across frames for warm-starting.
 *    • Per-contact Baumgarte bias + restitution slop.
 *    • Collision-layer filtering.
 *    • Box-raycast + capsule-raycast.
 *──────────────────────────────────────────────────────────────────────────*/
#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>
#include <RiftCore/Core/IModule.h>
#include <RiftCore/Physics/IPhysics.h>

#include <Physics/PhysicsTypes.h>
#include <Physics/RigidBody.h>

#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <functional>

#ifdef PHYSICS_EXPORTS
    #define PHYSICS_API RIFTCORE_EXPORT
#else
    #define PHYSICS_API RIFTCORE_IMPORT
#endif





namespace RiftCore {

    /* ──────────────────────────────────────────────────────────
     *  Collision callback — unchanged signature.
     * ────────────────────────────────────────────────────────── */
    using CollisionCallback = std::function<void(
        u32 bodyA_ID, u32 bodyB_ID, const ContactPoint& cp)>;

    /* ══════════════════════════════════════════════════════════
     *  PhysicsWorld  –  the simulation kernel
     * ══════════════════════════════════════════════════════════*/


    class PHYSICS_API PhysicsWorld {
    public:
        PhysicsWorld();
        ~PhysicsWorld();

        /* ── Gravity ─────────────────────────────────────────── */
        void SetGravity(const Vec3& g);
        Vec3 GetGravity() const { return gravity_; }

        /* ── Body management (same API) ──────────────────────── */
        u32         AddBody(const RigidBodyDesc& desc);
        void        RemoveBody(u32 id);
        void        ClearAllBodies();
        RigidBody*  GetBody(u32 id);

        u32 AddGroundPlane(f32 y          = 0.0f,
                           f32 restitution = 0.5f,
                           f32 friction    = 0.5f);

        /* ── Stepping ────────────────────────────────────────── */
        void Step(f32 dt);

        /* ── Raycast ─────────────────────────────────────────── */
        RaycastResult Raycast(const Vec3& origin,
                              const Vec3& direction,
                              f32 maxDistance = 1000.0f) const;

        /* ── Constraints / Joints ────────────────────────────── */
        u32  AddConstraint(const ConstraintDesc& desc);
        void RemoveConstraint(u32 id);

        /* ── Callbacks & config ──────────────────────────────── */
        void SetCollisionCallback(CollisionCallback cb) {
            collisionCallback_ = std::move(cb);
        }

        PhysicsStats GetStats()       const { return stats_;    }
        void SetSubSteps(u32 s)             { subSteps_ = s;    }
        u32  GetSubSteps()            const { return subSteps_; }
        void SetSolverIterations(u32 n)     { solverIter_ = n;  }
        u32  GetSolverIterations()    const { return solverIter_;}
        u32  GetBodyCount()           const {
            return static_cast<u32>(bodies_.size());
        }

        void SetBroadphaseCellSize(f32 size) {
            spatialHash_.SetCellSize(size);
        }

    private:
        /* ── Pipeline stages ─────────────────────────────────── */
        void BroadPhase (std::vector<std::pair<u32,u32>>& pairs);
        void NarrowPhase(const std::vector<std::pair<u32,u32>>& pairs,
                         std::vector<ContactManifold>& manifolds);
        void PreSolve   (std::vector<ContactManifold>& manifolds, f32 dt);
        void SolveVelocities(std::vector<ContactManifold>& manifolds);
        void SolvePositions (std::vector<ContactManifold>& manifolds);
        void SolveConstraints(f32 dt);

        /* ── Narrow-phase collision tests ────────────────────── */
        bool TestSphereSphere (RigidBody& a, RigidBody& b, ContactPoint& c);
        bool TestSphereBox    (RigidBody& sphere, RigidBody& box, ContactPoint& c);
        bool TestBoxBox       (RigidBody& a, RigidBody& b, ContactPoint& c);
        bool TestSpherePlane  (RigidBody& sphere, RigidBody& plane, ContactPoint& c);
        bool TestBoxPlane     (RigidBody& box, RigidBody& plane, ContactPoint& c);
        bool TestCapsuleSphere(RigidBody& capsule, RigidBody& sphere, ContactPoint& c);
        bool TestCapsuleBox   (RigidBody& capsule, RigidBody& box, ContactPoint& c);
        bool TestCapsulePlane (RigidBody& capsule, RigidBody& plane, ContactPoint& c);
        bool TestCapsuleCapsule(RigidBody& a, RigidBody& b, ContactPoint& c);

        bool DispatchNarrow(RigidBody& a, RigidBody& b, u32 iA, u32 iB,
                            ContactPoint& c);

        /* ── Raycast helpers ─────────────────────────────────── */
        bool RayVsSphere (const Vec3& o, const Vec3& d, const RigidBody& b,
                          f32& t, Vec3& n) const;
        bool RayVsBox    (const Vec3& o, const Vec3& d, const RigidBody& b,
                          f32& t, Vec3& n) const;
        bool RayVsCapsule(const Vec3& o, const Vec3& d, const RigidBody& b,
                          f32& t, Vec3& n) const;
        bool RayVsPlane  (const Vec3& o, const Vec3& d, const RigidBody& b,
                          f32& t, Vec3& n) const;

        /* ── Data ────────────────────────────────────────────── */
        Vec3                         gravity_  = {0, -9.81f, 0};
        std::vector<RigidBody>       bodies_;
        std::unordered_map<u32,u32>  idToIndex_;
        u32                          nextID_      = 1;

        // Solver tuning
        u32                          subSteps_    = 8;
        u32                          solverIter_  = 10;

        // Broadphase
        SpatialHash                  spatialHash_;

        // Constraints
        std::vector<Constraint>      constraints_;
        u32                          nextConstraintID_ = 1;

        // Manifold warm-start cache (key → manifold from previous step)
        std::unordered_map<u64, ContactManifold> manifoldCache_;

        // Callbacks & stats
        CollisionCallback            collisionCallback_;
        PhysicsStats                 stats_;
        mutable std::mutex           mutex_;
    };

    /* ══════════════════════════════════════════════════════════
     *  PhysicsSystemImpl  –  IPhysics implementation
     *
     *  This wraps PhysicsWorld and maps EntityID ↔ body-ID.
     *  Signatures match IPhysics exactly — DO NOT CHANGE.
     * ══════════════════════════════════════════════════════════*/
    class PHYSICS_API PhysicsSystemImpl : public IPhysics {
    public:
        PhysicsSystemImpl();
        ~PhysicsSystemImpl();

        VoidResult Initialize()                          override;
        void       Shutdown()                            override;
        void       StepSimulation(f32 dt)                override;
        void       SetGravity(const Vec3& g)             override;
        Vec3       GetGravity()                    const override;
        void       AddRigidBody(EntityID e,
                       const RigidBodyDesc& d)           override;
        void       RemoveRigidBody(EntityID e)           override;
        void       SetVelocity(EntityID e,
                       const Vec3& v)                    override;
        Vec3       GetVelocity(EntityID e)         const override;
        void       ApplyForce(EntityID e,
                       const Vec3& f)                    override;
        void       ApplyImpulse(EntityID e,
                       const Vec3& i)                    override;
        RaycastHit Raycast(const Vec3& o,
                       const Vec3& d, f32 dist)    const override;

        /* ── Extended API (used by other engine modules) ─────── */
        PhysicsWorld* GetWorld()  { return world_.get(); }
        u32           AddBody    (const RigidBodyDesc& d);
        void          RemoveBody (u32 id);
        void          ClearAllBodies();
        RigidBody*    GetBody    (u32 id);
        Vec3          GetBodyPosition(u32 id) const;

    private:
        std::unique_ptr<PhysicsWorld>     world_;
        std::unordered_map<EntityID,u32>  entityToBody_;
        std::unordered_map<u32,EntityID>  bodyToEntity_;
    };

    /* ══════════════════════════════════════════════════════════
     *  PhysicsModule  –  IModule plug-in entry point
     *
     *  The engine loads this DLL and calls Initialize / OnUpdate /
     *  Shutdown through the IModule interface.
     * ══════════════════════════════════════════════════════════*/
    class PHYSICS_API PhysicsModule : public IModule {
    public:
        PhysicsModule();
        ~PhysicsModule();

        VoidResult       Initialize(
            const ModuleInitParams& p) override;
        void             OnUpdate(f32 dt)  override;
        void             OnFixedUpdate(
            f32 fdt)                       override;
        void             Shutdown()        override;
        ModuleDescriptor GetDescriptor()
                                     const override;

        PhysicsSystemImpl* GetPhysics() {
            return physics_.get();
        }

    private:
        std::unique_ptr<PhysicsSystemImpl> physics_;
        f32 accumulator_ = 0.0f;
        f32 fixedStep_   = 1.0f / 120.0f;
    };

} // namespace RiftCore

#pragma warning(pop)
