/*──────────────────────────────────────────────────────────────────────────
 *  IPhysics.h  –  Public SDK interface for the Physics system
 *
 *  This file lives in the engine SDK and defines the abstract contract
 *  that any Physics plug-in DLL must implement.  Game code and other
 *  engine modules include ONLY this header — never the internal
 *  PhysicsTypes.h / RigidBody.h / PhysicsWorld.h files.
 *
 *  ┌─────────────────────────────────────────────────────────────────┐
 *  │  SDK layer  (this file)       →  stable, versioned, minimal    │
 *  │  DLL layer  (Physics/*.h/cpp) →  internal, can change freely   │
 *  └─────────────────────────────────────────────────────────────────┘
 *──────────────────────────────────────────────────────────────────────────*/
#pragma once


#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>



#include <RiftCore/ECS/IECS.h>











namespace RiftCore {

    /* ══════════════════════════════════════════════════════════════
     *  ColliderShape
     *
     *  Enumerates the primitive shapes the physics engine can collide.
     *  Capsule was added in v2.0 for character-controller support.
     * ══════════════════════════════════════════════════════════════*/
    enum class ColliderShape : u8 {
        Sphere  = 0,   ///< Defined by `radius`.
        Box     = 1,   ///< Defined by `halfExtents` (x, y, z).
        Plane   = 2,   ///< Infinite plane: `planeNormal` · P = `planeOffset`.
        Capsule = 3    ///< Y-axis cylinder + hemispherical caps.
                       ///  `radius` + `halfExtents.y` (half-height of cylinder).
    };

    /* ══════════════════════════════════════════════════════════════
     *  ColliderDesc  –  Shape + surface properties
     *
     *  Attached to every RigidBodyDesc.  The physics DLL reads these
     *  to build internal collision geometry and material data.
     *
     *  Fields used per shape:
     *    Sphere  → radius
     *    Box     → halfExtents
     *    Plane   → planeNormal, planeOffset
     *    Capsule → radius, halfExtents.y  (half cylinder height)
     * ══════════════════════════════════════════════════════════════*/
    struct ColliderDesc {
        ColliderShape shape       = ColliderShape::Box;
        Vec3          halfExtents = {0.5f, 0.5f, 0.5f};
        f32           radius      = 0.5f;
        Vec3          planeNormal = {0, 1, 0};
        f32           planeOffset = 0.0f;
        f32           restitution = 0.4f;   ///< Bounciness  [0..1].
        f32           friction    = 0.5f;   ///< Surface grip [0..1+].
    };

    /* ══════════════════════════════════════════════════════════════
     *  RigidBodyDesc  –  Everything needed to spawn a body
     *
     *  Pass one of these to IPhysics::AddRigidBody().  The DLL will
     *  compute mass-derived quantities (inverse mass, inertia tensor)
     *  internally.
     * ══════════════════════════════════════════════════════════════*/
    struct RigidBodyDesc {
        Vec3         position        = Vec3::Zero();      ///< World-space spawn position.
        Vec3         velocity        = Vec3::Zero();      ///< Initial linear velocity.
        Vec3         angularVelocity = Vec3::Zero();      ///< Initial angular velocity (rad/s).
        f32          mass            = 1.0f;              ///< Kilograms.  0 or negative → 1 kg.
        bool         isStatic        = false;             ///< Immovable (infinite mass).
        bool         isKinematic     = false;             ///< Moved by code, not forces.
        bool         useGravity      = true;              ///< Subject to world gravity?
        ColliderDesc collider;                            ///< Shape + surface.
        f32          linearDamping   = 0.01f;             ///< Velocity decay per second [0..1].
        f32          angularDamping  = 0.05f;             ///< Spin decay per second    [0..1].
    };

    /* ══════════════════════════════════════════════════════════════
     *  RaycastHit  –  Result returned by IPhysics::Raycast()
     * ══════════════════════════════════════════════════════════════*/
    struct RaycastHit {
        bool  hit       = false;          ///< True if something was hit.
        f32   distance  = 0.0f;           ///< Distance along the ray.
        Vec3  point     = Vec3::Zero();   ///< World-space hit location.
        Vec3  normal    = Vec3::Up();     ///< Surface normal at hit.
        u32   bodyIndex = 0;              ///< Internal body index.
    };

    /* ══════════════════════════════════════════════════════════════
     *  IPhysics  –  Abstract interface (pure virtual)
     *
     *  The engine resolves this via EngineContext::Get<IPhysics>().
     *  The Physics DLL registers its PhysicsSystemImpl (which derives
     *  from IPhysics) during module initialisation.
     *
     *  ── Method reference ────────────────────────────────────────
     *
     *  Initialize()
     *      Create the internal PhysicsWorld, set default gravity,
     *      allocate solver buffers.  Called once at engine start-up.
     *      Returns VoidResult::Err() on failure.
     *
     *  Shutdown()
     *      Destroy the world and free all memory.
     *
     *  StepSimulation(dt)
     *      Advance the simulation by `dt` seconds.  Internally this
     *      is divided into fixed sub-steps for stability.
     *
     *  SetGravity(g) / GetGravity()
     *      World-wide gravitational acceleration (default 0, −9.81, 0).
     *
     *  AddRigidBody(entity, desc)
     *      Spawn a body tied to an ECS EntityID.
     *
     *  RemoveRigidBody(entity)
     *      Destroy the body associated with that entity.
     *
     *  SetVelocity(entity, v) / GetVelocity(entity)
     *      Direct velocity read / write.
     *
     *  ApplyForce(entity, f)
     *      Accumulate a continuous force (applied at centre of mass).
     *      Persists for one step, then cleared.
     *
     *  ApplyImpulse(entity, impulse)
     *      Instantaneous momentum change (mass-scaled internally).
     *
     *  Raycast(origin, direction, maxDist)
     *      Cast a ray and return the closest hit (if any).
     * ══════════════════════════════════════════════════════════════*/
    class IPhysics {
    public:
        virtual ~IPhysics() = default;

        virtual VoidResult Initialize()                        = 0;
        virtual void       Shutdown()                          = 0;
        virtual void       StepSimulation(f32 deltaTime)       = 0;
        virtual void       SetGravity(const Vec3& gravity)     = 0;
        virtual Vec3       GetGravity()                  const = 0;

        virtual void AddRigidBody(
            EntityID e, const RigidBodyDesc& desc)             = 0;
        virtual void RemoveRigidBody(EntityID entity)          = 0;
        virtual void SetVelocity(
            EntityID e, const Vec3& v)                         = 0;
        virtual Vec3 GetVelocity(EntityID e)             const = 0;
        virtual void ApplyForce(
            EntityID e, const Vec3& f)                         = 0;
        virtual void ApplyImpulse(
            EntityID e, const Vec3& impulse)                   = 0;
        virtual RaycastHit Raycast(
            const Vec3& origin,
            const Vec3& direction,
            f32 maxDist = 1000.0f)                       const = 0;
    };

} // namespace RiftCore
