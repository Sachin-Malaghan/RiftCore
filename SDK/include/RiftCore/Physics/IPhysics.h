// ── SDK/include/RiftCore/Physics/IPhysics.h ──────────────────
#pragma once
#include "../Common/Platform.h"
#include "../Common/Types.h"
#include "../Common/Result.h"
#include "../ECS/IECS.h"

namespace RiftCore {

    enum class ColliderShape : u8 {
        Box = 0, Sphere, Capsule, Mesh, ConvexHull
    };

    struct RigidBodyDesc {
        f32           mass        = 1.0f;
        bool          isStatic    = false;
        bool          isKinematic = false;
        Vec3          linearVelocity  = Vec3::Zero();
        Vec3          angularVelocity = Vec3::Zero();
        ColliderShape shape = ColliderShape::Box;
        Vec3          shapeExtents   = Vec3::One();
        f32           restitution    = 0.5f;
        f32           friction       = 0.5f;
    };

    struct RaycastHit {
        bool      hit         = false;
        EntityID  entity      = INVALID_ENTITY;
        Vec3      point       = Vec3::Zero();
        Vec3      normal      = Vec3::Up();
        f32       distance    = 0.0f;
    };

    class IPhysics {
    public:
        virtual ~IPhysics() = default;

        virtual VoidResult  Initialize()                              = 0;
        virtual void        Shutdown()                                = 0;
        virtual void        StepSimulation(f32 deltaTime)            = 0;
        virtual void        SetGravity(const Vec3& gravity)          = 0;
        virtual Vec3        GetGravity()                       const  = 0;
        virtual void        AddRigidBody(EntityID e, const RigidBodyDesc& desc) = 0;
        virtual void        RemoveRigidBody(EntityID entity)         = 0;
        virtual void        SetVelocity(EntityID e, const Vec3& v)   = 0;
        virtual Vec3        GetVelocity(EntityID e)            const  = 0;
        virtual void        ApplyForce(EntityID e, const Vec3& f)    = 0;
        virtual void        ApplyImpulse(EntityID e, const Vec3& i)  = 0;
        virtual RaycastHit  Raycast(const Vec3& origin,
                                    const Vec3& direction,
                                    f32         maxDist = 1000.0f)const = 0;
    };

} // namespace RiftCore