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

    using CollisionCallback = std::function<void(
        u32 bodyA, u32 bodyB,
        const ContactPoint& contact
    )>;

    class PHYSICS_API PhysicsWorld {
    public:
        PhysicsWorld();
        ~PhysicsWorld();

        void SetGravity(const Vec3& gravity);
        Vec3 GetGravity() const { return gravity_; }

        u32  AddBody   (const RigidBodyDesc& desc);
        void RemoveBody(u32 id);
        RigidBody* GetBody(u32 id);

        u32 AddGroundPlane(f32 y = 0.0f,
                           f32 restitution = 0.4f,
                           f32 friction    = 0.6f);

        void Step(f32 dt);

        RaycastResult Raycast(
            const Vec3& origin,
            const Vec3& direction,
            f32 maxDistance = 1000.0f
        ) const;

        void SetCollisionCallback(CollisionCallback cb) {
            collisionCallback_ = std::move(cb);
        }

        PhysicsStats GetStats()    const { return stats_;  }
        void SetSubSteps(u32 s)          { subSteps_ = s;  }
        u32  GetSubSteps()         const { return subSteps_;}
        u32  GetBodyCount()        const {
            return static_cast<u32>(bodies_.size());
        }

    private:
        void IntegrateBodies(f32 dt);
        void BroadPhase(
            std::vector<std::pair<u32,u32>>& pairs);
        void NarrowPhase(
            const std::vector<std::pair<u32,u32>>& pairs,
            std::vector<ContactPoint>& contacts);
        void ResolveContacts(
            std::vector<ContactPoint>& contacts);

        bool TestSphereSphere (RigidBody& a, RigidBody& b,
                               ContactPoint& c);
        bool TestSphereBox    (RigidBody& sphere,
                               RigidBody& box,
                               ContactPoint& c);
        bool TestBoxBox       (RigidBody& a, RigidBody& b,
                               ContactPoint& c);
        bool TestSpherePlane  (RigidBody& sphere,
                               RigidBody& plane,
                               ContactPoint& c);
        bool TestBoxPlane     (RigidBody& box,
                               RigidBody& plane,
                               ContactPoint& c);
        void ResolveContact   (ContactPoint& c);
        void PositionalCorrect(ContactPoint& c);

        static f32  Dot      (const Vec3& a, const Vec3& b){
            return a.x*b.x + a.y*b.y + a.z*b.z;
        }
        static f32  Length   (const Vec3& v) {
            return std::sqrt(v.x*v.x+v.y*v.y+v.z*v.z);
        }
        static Vec3 Normalize(const Vec3& v) {
            f32 len = Length(v);
            if (len < 0.0001f) return {0,1,0};
            return {v.x/len, v.y/len, v.z/len};
        }

        Vec3                              gravity_  = {0,-9.81f,0};
        std::vector<RigidBody>            bodies_;
        std::unordered_map<u32, u32>      idToIndex_;
        u32                               nextID_   = 1;
        u32                               subSteps_ = 4;

        CollisionCallback                 collisionCallback_;
        PhysicsStats                      stats_;
        mutable std::mutex                mutex_;
    };

    // ── IPhysics implementation ───────────────────────────────
    class PHYSICS_API PhysicsSystemImpl : public IPhysics {
    public:
        PhysicsSystemImpl();
        ~PhysicsSystemImpl();

        VoidResult Initialize()                           override;
        void       Shutdown()                             override;
        void       StepSimulation(f32 dt)                 override;
        void       SetGravity(const Vec3& g)              override;
        Vec3       GetGravity()                     const override;
        void       AddRigidBody(EntityID e,
                       const RigidBodyDesc& d)            override;
        void       RemoveRigidBody(EntityID e)            override;
        void       SetVelocity(EntityID e, const Vec3& v) override;
        Vec3       GetVelocity(EntityID e)          const override;
        void       ApplyForce(EntityID e, const Vec3& f)  override;
        void       ApplyImpulse(EntityID e,
                       const Vec3& i)                     override;
        RaycastHit Raycast(const Vec3& origin,
                           const Vec3& dir,
                           f32 maxDist)             const override;

        PhysicsWorld* GetWorld() { return world_.get(); }

        u32        AddBody          (const RigidBodyDesc& d);
        void       RemoveBody       (u32 id);
        RigidBody* GetBody          (u32 id);
        Vec3       GetBodyPosition  (u32 id) const;

    private:
        std::unique_ptr<PhysicsWorld>     world_;
        std::unordered_map<EntityID, u32> entityToBody_;
        std::unordered_map<u32, EntityID> bodyToEntity_;
    };

    // ── IModule wrapper ───────────────────────────────────────
    class PHYSICS_API PhysicsModule : public IModule {
    public:
        PhysicsModule();
        ~PhysicsModule();

        VoidResult       Initialize(
            const ModuleInitParams& params) override;
        void             OnUpdate(f32 dt)   override;
        void             OnFixedUpdate(
            f32 fixedDt)                    override;
        void             Shutdown()         override;
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

