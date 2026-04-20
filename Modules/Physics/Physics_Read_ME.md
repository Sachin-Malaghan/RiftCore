# VS Code — Install the Markdown Preview Mermaid Support extension, then open any .md file containing the block above and press Ctrl+Shift+V. #

# Online — Paste into mermaid.live to get an interactive SVG you can export as PNG. #

# GitHub — Any .md file with a ```mermaid block renders automatically. #



classDiagram
    direction TB

    %% ═══════════════════════ SDK LAYER ═══════════════════════

    class IPhysics {
        <<interface>>
        +Initialize() VoidResult
        +Shutdown() void
        +StepSimulation(dt: f32) void
        +SetGravity(g: Vec3) void
        +GetGravity() Vec3
        +AddRigidBody(e: EntityID, desc: RigidBodyDesc) void
        +RemoveRigidBody(e: EntityID) void
        +SetVelocity(e: EntityID, v: Vec3) void
        +GetVelocity(e: EntityID) Vec3
        +ApplyForce(e: EntityID, f: Vec3) void
        +ApplyImpulse(e: EntityID, impulse: Vec3) void
        +Raycast(origin: Vec3, dir: Vec3, maxDist: f32) RaycastHit
    }

    class IModule {
        <<interface>>
        +Initialize(params: ModuleInitParams) VoidResult
        +OnUpdate(dt: f32) void
        +OnFixedUpdate(fdt: f32) void
        +Shutdown() void
        +GetDescriptor() ModuleDescriptor
    }

    %% ═══════════════════ SDK DATA TYPES ═══════════════════

    class ColliderDesc {
        +shape: ColliderShape
        +halfExtents: Vec3
        +radius: f32
        +planeNormal: Vec3
        +planeOffset: f32
        +restitution: f32
        +friction: f32
    }

    class RigidBodyDesc {
        +position: Vec3
        +velocity: Vec3
        +angularVelocity: Vec3
        +mass: f32
        +isStatic: bool
        +isKinematic: bool
        +useGravity: bool
        +collider: ColliderDesc
        +linearDamping: f32
        +angularDamping: f32
    }

    class RaycastHit {
        +hit: bool
        +distance: f32
        +point: Vec3
        +normal: Vec3
        +bodyIndex: u32
    }

    class ColliderShape {
        <<enumeration>>
        Sphere
        Box
        Plane
        Capsule
    }

    %% ═══════════════════ DLL INTERNALS ═══════════════════

    class PhysicsModule {
        -physics_: unique_ptr~PhysicsSystemImpl~
        -accumulator_: f32
        -fixedStep_: f32
        +Initialize(params) VoidResult
        +OnUpdate(dt) void
        +OnFixedUpdate(fdt) void
        +Shutdown() void
        +GetDescriptor() ModuleDescriptor
        +GetPhysics() PhysicsSystemImpl*
    }

    class PhysicsSystemImpl {
        -world_: unique_ptr~PhysicsWorld~
        -entityToBody_: map~EntityID → u32~
        -bodyToEntity_: map~u32 → EntityID~
        +Initialize() VoidResult
        +Shutdown() void
        +StepSimulation(dt) void
        +SetGravity(g) void
        +GetGravity() Vec3
        +AddRigidBody(e, desc) void
        +RemoveRigidBody(e) void
        +SetVelocity(e, v) void
        +GetVelocity(e) Vec3
        +ApplyForce(e, f) void
        +ApplyImpulse(e, impulse) void
        +Raycast(o, d, dist) RaycastHit
        +GetWorld() PhysicsWorld*
        +AddBody(d) u32
        +RemoveBody(id) void
        +ClearAllBodies() void
        +GetBody(id) RigidBody*
        +GetBodyPosition(id) Vec3
    }

    class PhysicsWorld {
        -gravity_: Vec3
        -bodies_: vector~RigidBody~
        -idToIndex_: map~u32 → u32~
        -nextID_: u32
        -subSteps_: u32
        -solverIter_: u32
        -spatialHash_: SpatialHash
        -constraints_: vector~Constraint~
        -manifoldCache_: map~u64 → ContactManifold~
        -collisionCallback_: CollisionCallback
        -stats_: PhysicsStats
        +SetGravity(g) void
        +GetGravity() Vec3
        +AddBody(desc) u32
        +RemoveBody(id) void
        +ClearAllBodies() void
        +GetBody(id) RigidBody*
        +AddGroundPlane(y, rest, fric) u32
        +Step(dt) void
        +Raycast(origin, dir, maxDist) RaycastResult
        +AddConstraint(desc) u32
        +RemoveConstraint(id) void
        +SetCollisionCallback(cb) void
        +SetSubSteps(s) void
        +SetSolverIterations(n) void
        +SetBroadphaseCellSize(size) void
        -BroadPhase(pairs) void
        -NarrowPhase(pairs, manifolds) void
        -PreSolve(manifolds, dt) void
        -SolveVelocities(manifolds) void
        -SolvePositions(manifolds) void
        -SolveConstraints(dt) void
        -DispatchNarrow(a, b, iA, iB, c) bool
        -TestSphereSphere() bool
        -TestSphereBox() bool
        -TestBoxBox() bool
        -TestSpherePlane() bool
        -TestBoxPlane() bool
        -TestCapsuleSphere() bool
        -TestCapsuleBox() bool
        -TestCapsulePlane() bool
        -TestCapsuleCapsule() bool
        -RayVsSphere() bool
        -RayVsBox() bool
        -RayVsCapsule() bool
        -RayVsPlane() bool
    }

    class RigidBody {
        -id_: u32
        -position_: Vec3
        -orientation_: Quat
        -velocity_: Vec3
        -angularVelocity_: Vec3
        -accumulatedForce_: Vec3
        -accumulatedTorque_: Vec3
        -mass_: f32
        -invMass_: f32
        -localInertia_: Mat3
        -localInvInertia_: Mat3
        -worldInvInertia_: Mat3
        -collider_: ColliderDesc
        -material_: PhysicsMaterial
        -layer_: CollisionLayer
        -mask_: CollisionMask
        -isStatic_: bool
        -isAwake_: bool
        -ccdEnabled_: bool
        +GetID() u32
        +IsStatic() bool
        +IsAwake() bool
        +GetPosition() Vec3
        +GetOrientation() Quat
        +GetVelocity() Vec3
        +GetAngularVel() Vec3
        +GetMass() f32
        +GetInvMass() f32
        +GetWorldInvInertia() Mat3
        +SetPosition(pos) void
        +SetOrientation(q) void
        +SetVelocity(vel) void
        +SetAngularVelocity(av) void
        +ApplyForce(f) void
        +ApplyForceAtPoint(f, point) void
        +ApplyImpulse(impulse) void
        +ApplyImpulseAtPoint(impulse, point) void
        +ApplyTorque(torque) void
        +ApplyTorqueImpulse(impulse) void
        +GetAABB() AABB
        +GetCollider() ColliderDesc
        +Integrate(dt, gravity) void
        +Wake() void
        +Sleep() void
        -ComputeInertia() void
        -UpdateWorldInertia() void
    }

    %% ═══════════════ INTERNAL DATA TYPES ═══════════════

    class Mat3 {
        +m: f32[3][3]
        +Zero() Mat3$
        +Identity() Mat3$
        +Diagonal(a, b, c) Mat3$
        +operator*(Mat3) Mat3
        +operator*(Vec3) Vec3
        +Transposed() Mat3
        +Inverse() Mat3
    }

    class Quat {
        +w: f32
        +x: f32
        +y: f32
        +z: f32
        +Identity() Quat$
        +Normalized() Quat
        +Conjugate() Quat
        +operator*(Quat) Quat
        +Rotate(v: Vec3) Vec3
        +ToMat3() Mat3
        +FromAxisAngle(axis, angle) Quat$
        +IntegrateAngularVelocity(omega, dt) void
    }

    class AABB {
        +min: Vec3
        +max: Vec3
        +Overlaps(other) bool
        +Center() Vec3
        +HalfExtents() Vec3
        +SurfaceArea() f32
        +Merged(other) AABB
        +Fattened(margin) AABB
        +Contains(p) bool
        +RayIntersect(origin, invDir, maxDist, tMin) bool
    }

    class ContactPoint {
        +point: Vec3
        +normal: Vec3
        +penetration: f32
        +bodyA: u32
        +bodyB: u32
        +normalImpulseAccum: f32
        +tangentImpulseAccum: f32
        +rA: Vec3
        +rB: Vec3
        +normalMass: f32
        +bias: f32
        +tangent: Vec3
        +bitangent: Vec3
    }

    class ContactManifold {
        +bodyA: u32
        +bodyB: u32
        +numContacts: u32
        +contacts: ContactPoint[4]
        +Key() u64
    }

    class PhysicsMaterial {
        +restitution: f32
        +staticFriction: f32
        +dynamicFriction: f32
        +density: f32
        +frictionCombine: CombineMode
        +Combine(a, b, mode) f32$
    }

    class SpatialHash {
        -cellSize_: f32
        -cells_: map~u64 → vector~u32~~
        +Clear() void
        +SetCellSize(size) void
        +Insert(index, aabb) void
        +Query(aabb, results) void
        -HashKey(x, y, z) u64
    }

    class Constraint {
        +desc: ConstraintDesc
        +id: u32
    }

    class ConstraintDesc {
        +type: ConstraintType
        +bodyA: u32
        +bodyB: u32
        +anchorA: Vec3
        +anchorB: Vec3
        +axis: Vec3
        +distance: f32
        +stiffness: f32
        +damping: f32
    }

    class ConstraintType {
        <<enumeration>>
        Distance
        Hinge
        BallSocket
        Fixed
        Slider
        Spring
    }

    class PhysicsStats {
        +bodyCount: u32
        +activeCount: u32
        +contactCount: u32
        +manifoldCount: u32
        +collisionPairs: u32
        +broadPhaseMs: f32
        +narrowPhaseMs: f32
        +solverMs: f32
        +stepTimeMs: f32
    }

    %% ═══════════════ RELATIONSHIPS ═══════════════

    IPhysics <|.. PhysicsSystemImpl : implements
    IModule <|.. PhysicsModule : implements

    PhysicsModule *-- PhysicsSystemImpl : owns
    PhysicsSystemImpl *-- PhysicsWorld : owns

    PhysicsWorld *-- "0..*" RigidBody : stores
    PhysicsWorld *-- SpatialHash : broadphase
    PhysicsWorld *-- "0..*" Constraint : joints
    PhysicsWorld o-- "0..*" ContactManifold : cache

    RigidBody *-- ColliderDesc : shape
    RigidBody *-- PhysicsMaterial : surface
    RigidBody *-- Quat : orientation
    RigidBody *-- Mat3 : inertia tensors
    RigidBody ..> AABB : computes

    ContactManifold *-- "1..4" ContactPoint : contains
    Constraint *-- ConstraintDesc : config

    ColliderDesc --> ColliderShape : uses
    ConstraintDesc --> ConstraintType : uses
    RigidBodyDesc *-- ColliderDesc : contains
