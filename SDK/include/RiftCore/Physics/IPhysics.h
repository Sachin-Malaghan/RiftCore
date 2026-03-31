#pragma once

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>
#include <RiftCore/ECS/IECS.h>

namespace RiftCore {

    enum class ColliderShape : u8 {
        Sphere  = 0,
        Box     = 1,
        Plane   = 2,
        Capsule = 3
    };

    struct ColliderDesc {
        ColliderShape shape       = ColliderShape::Box;
        Vec3          halfExtents = {0.5f,0.5f,0.5f};
        f32           radius      = 0.5f;
        Vec3          planeNormal = {0,1,0};
        f32           planeOffset = 0.0f;
        f32           restitution = 0.4f;
        f32           friction    = 0.5f;
    };

    struct RigidBodyDesc {
        Vec3         position        = Vec3::Zero();
        Vec3         velocity        = Vec3::Zero();
        Vec3         angularVelocity = Vec3::Zero();
        f32          mass            = 1.0f;
        bool         isStatic        = false;
        bool         isKinematic     = false;
        bool         useGravity      = true;
        ColliderDesc collider;
        f32          linearDamping   = 0.01f;
        f32          angularDamping  = 0.05f;
    };

    struct RaycastHit {
        bool  hit       = false;
        f32   distance  = 0.0f;
        Vec3  point     = Vec3::Zero();
        Vec3  normal    = Vec3::Up();
        u32   bodyIndex = 0;
    };

    class IPhysics {
    public:
        virtual ~IPhysics() = default;

        virtual VoidResult Initialize()                      = 0;
        virtual void       Shutdown()                        = 0;
        virtual void       StepSimulation(f32 deltaTime)     = 0;
        virtual void       SetGravity(const Vec3& gravity)   = 0;
        virtual Vec3       GetGravity()                const  = 0;

        virtual void AddRigidBody(
            EntityID e, const RigidBodyDesc& desc)           = 0;
        virtual void RemoveRigidBody(EntityID entity)         = 0;
        virtual void SetVelocity(
            EntityID e, const Vec3& v)                       = 0;
        virtual Vec3 GetVelocity(EntityID e)           const  = 0;
        virtual void ApplyForce(
            EntityID e, const Vec3& f)                       = 0;
        virtual void ApplyImpulse(
            EntityID e, const Vec3& impulse)                 = 0;
        virtual RaycastHit Raycast(
            const Vec3& origin,
            const Vec3& direction,
            f32 maxDist = 1000.0f)                     const  = 0;
    };

} // namespace RiftCore
