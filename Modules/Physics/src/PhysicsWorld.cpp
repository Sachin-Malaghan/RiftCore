#pragma warning(disable: 4190)
#include <Physics/PhysicsWorld.h>
#include <RiftCore/Common/EngineContext.h>
#include <RiftCore/Core/ILogger.h>

#include <algorithm>
#include <iostream>
#include <chrono>
#include <cmath>

namespace RiftCore {

    // -- Math helpers ------------------------------------------
    static f32 Dot(const Vec3& a, const Vec3& b) {
        return a.x*b.x + a.y*b.y + a.z*b.z;
    }

    static f32 Length(const Vec3& v) {
        return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    }

    static Vec3 Normalize(const Vec3& v) {
        f32 len = Length(v);
        if (len < 0.00001f) return {0,1,0};
        return {v.x/len, v.y/len, v.z/len};
    }

    // -- PhysicsWorld ------------------------------------------
    PhysicsWorld::PhysicsWorld()  = default;
    PhysicsWorld::~PhysicsWorld() = default;

    void PhysicsWorld::SetGravity(const Vec3& g) {
        gravity_ = g;
    }

    u32 PhysicsWorld::AddBody(const RigidBodyDesc& desc) {
        u32 id    = nextID_++;
        u32 index = static_cast<u32>(bodies_.size());
        bodies_.emplace_back(id, desc);
        idToIndex_[id] = index;
        return id;
    }

    u32 PhysicsWorld::AddGroundPlane(
        f32 y, f32 restitution, f32 friction
    ) {
        RigidBodyDesc desc;
        desc.isStatic         = true;
        desc.useGravity       = false;
        desc.position         = {0, y, 0};
        desc.collider.shape       = ColliderShape::Plane;
        desc.collider.planeNormal = {0, 1, 0};
        desc.collider.planeOffset = y;
        desc.collider.restitution = restitution;
        desc.collider.friction    = friction;
        return AddBody(desc);
    }

    void PhysicsWorld::RemoveBody(u32 id) {
        auto it = idToIndex_.find(id);
        if (it == idToIndex_.end()) return;

        u32 idx     = it->second;
        u32 lastIdx = static_cast<u32>(bodies_.size()-1);

        if (idx != lastIdx) {
            bodies_[idx] = std::move(bodies_[lastIdx]);
            idToIndex_[bodies_[idx].GetID()] = idx;
        }
        bodies_.pop_back();
        idToIndex_.erase(id);
    }

    void PhysicsWorld::ClearAllBodies() {
        std::lock_guard<std::mutex> lock(mutex_);
        bodies_.clear();
        idToIndex_.clear();
        nextID_ = 1;
        std::cout << "[PhysicsWorld] All bodies cleared.\n";
    }

    RigidBody* PhysicsWorld::GetBody(u32 id) {
        auto it = idToIndex_.find(id);
        if (it == idToIndex_.end()) return nullptr;
        return &bodies_[it->second];
    }

    // -- Main simulation step ----------------------------------
    void PhysicsWorld::Step(f32 dt) {
        auto t0 = std::chrono::high_resolution_clock::now();

        // Fixed substeps for stability
        f32  subDt       = dt / static_cast<f32>(subSteps_);
        u32  totalPairs  = 0;
        u32  totalContacts = 0;

        for (u32 s = 0; s < subSteps_; s++) {
            // 1. Integrate positions
            for (auto& body : bodies_) {
                body.Integrate(subDt, gravity_);
            }

            // 2. Find overlapping pairs
            std::vector<std::pair<u32,u32>> pairs;
            BroadPhase(pairs);
            totalPairs += static_cast<u32>(pairs.size());

            // 3. Generate contacts
            std::vector<ContactPoint> contacts;
            NarrowPhase(pairs, contacts);
            totalContacts += static_cast<u32>(
                contacts.size());

            // 4. Resolve contacts (multiple iterations)
            const u32 solverIter = 6;
            for (u32 iter = 0; iter < solverIter; iter++) {
                for (auto& c : contacts) {
                    ResolveVelocity(c);
                }
            }

            // 5. Positional correction
            for (auto& c : contacts) {
                PositionalCorrect(c);
            }

            // 6. Wake bodies that had contacts
            for (auto& c : contacts) {
                bodies_[c.bodyA].Wake();
                bodies_[c.bodyB].Wake();
            }
        }

        // Update stats
        u32 active = 0;
        for (auto& b : bodies_) {
            if (b.IsAwake() && !b.IsStatic()) active++;
        }
        stats_.bodyCount      = static_cast<u32>(
            bodies_.size());
        stats_.activeCount    = active;
        stats_.contactCount   = totalContacts;
        stats_.collisionPairs = totalPairs;

        auto t1 = std::chrono::high_resolution_clock::now();
        stats_.stepTimeMs = std::chrono::duration<f32,
            std::milli>(t1 - t0).count();

        // Debug every 120 frames
        static u32 dbgFrame = 0;
        if (++dbgFrame % 120 == 0) {
            f32 lowestY = 9999.0f;
            for (auto& b : bodies_) {
                if (!b.IsStatic()) {
                    lowestY = std::min(lowestY,
                        b.GetPosition().y);
                }
            }
            std::cout
                << "[Physics] bodies=" << stats_.bodyCount
                << " active=" << stats_.activeCount
                << " pairs="  << totalPairs
                << " contacts=" << totalContacts
                << " lowestY=" << lowestY
                << "\n";
        }
    }

    // -- BroadPhase --------------------------------------------
    void PhysicsWorld::BroadPhase(
        std::vector<std::pair<u32,u32>>& pairs
    ) {
        u32 n = static_cast<u32>(bodies_.size());
        for (u32 i = 0; i < n; i++) {
            for (u32 j = i+1; j < n; j++) {
                RigidBody& a = bodies_[i];
                RigidBody& b = bodies_[j];

                // Skip static vs static
                if (a.IsStatic() && b.IsStatic()) continue;

                // At least one must be active
                // Static bodies count as always active
                bool aOk = a.IsAwake() || a.IsStatic();
                bool bOk = b.IsAwake() || b.IsStatic();
                if (!aOk && !bOk) continue;

                // AABB test
                if (a.GetAABB().Overlaps(b.GetAABB())) {
                    pairs.push_back({i, j});
                }
            }
        }
    }

    // -- NarrowPhase -------------------------------------------
    void PhysicsWorld::NarrowPhase(
        const std::vector<std::pair<u32,u32>>& pairs,
        std::vector<ContactPoint>&              contacts
    ) {
        for (auto& [iA, iB] : pairs) {
            RigidBody& bodyA = bodies_[iA];
            RigidBody& bodyB = bodies_[iB];

            ColliderShape sA = bodyA.GetCollider().shape;
            ColliderShape sB = bodyB.GetCollider().shape;

            ContactPoint c;
            c.bodyA = iA;
            c.bodyB = iB;
            bool hit = false;

            // Dispatch to correct test
            if (sA == ColliderShape::Sphere &&
                sB == ColliderShape::Sphere) {
                hit = TestSphereSphere(bodyA,bodyB,c);
            }
            else if (sA == ColliderShape::Sphere &&
                     sB == ColliderShape::Box) {
                hit = TestSphereBox(bodyA,bodyB,c);
            }
            else if (sA == ColliderShape::Box &&
                     sB == ColliderShape::Sphere) {
                hit = TestSphereBox(bodyB,bodyA,c);
                if (hit) {
                    // Flip normal — sphere was B
                    c.normal.x = -c.normal.x;
                    c.normal.y = -c.normal.y;
                    c.normal.z = -c.normal.z;
                    std::swap(c.bodyA, c.bodyB);
                }
            }
            else if (sA == ColliderShape::Box &&
                     sB == ColliderShape::Box) {
                hit = TestBoxBox(bodyA,bodyB,c);
            }
            else if (sA == ColliderShape::Sphere &&
                     sB == ColliderShape::Plane) {
                hit = TestSpherePlane(bodyA,bodyB,c);
            }
            else if (sA == ColliderShape::Plane &&
                     sB == ColliderShape::Sphere) {
                // Swap so sphere=A, plane=B
                hit = TestSpherePlane(bodyB,bodyA,c);
                if (hit) {
                    std::swap(c.bodyA, c.bodyB);
                }
            }
            else if (sA == ColliderShape::Box &&
                     sB == ColliderShape::Plane) {
                hit = TestBoxPlane(bodyA,bodyB,c);
            }
            else if (sA == ColliderShape::Plane &&
                     sB == ColliderShape::Box) {
                // Swap so box=A, plane=B
                hit = TestBoxPlane(bodyB,bodyA,c);
                if (hit) {
                    std::swap(c.bodyA, c.bodyB);
                }
            }

            if (hit && c.penetration > 0.0f) {
                contacts.push_back(c);
                if (collisionCallback_) {
                    collisionCallback_(
                        bodyA.GetID(),
                        bodyB.GetID(), c);
                }
            }
        }
    }

    // -- Collision tests ---------------------------------------
    bool PhysicsWorld::TestSphereSphere(
        RigidBody& a, RigidBody& b, ContactPoint& c
    ) {
        Vec3 posA = a.GetPosition();
        Vec3 posB = b.GetPosition();
        f32  rA   = a.GetCollider().radius;
        f32  rB   = b.GetCollider().radius;
        f32  rSum = rA + rB;

        Vec3 diff = {posA.x-posB.x, posA.y-posB.y,
                     posA.z-posB.z};
        f32  dist = Length(diff);

        if (dist >= rSum || dist < 0.0001f) return false;

        c.normal      = Normalize(diff);
        c.penetration = rSum - dist;
        c.point       = {
            posB.x + c.normal.x * rB,
            posB.y + c.normal.y * rB,
            posB.z + c.normal.z * rB
        };
        return true;
    }

    bool PhysicsWorld::TestSphereBox(
        RigidBody& sphere, RigidBody& box, ContactPoint& c
    ) {
        Vec3 sp  = sphere.GetPosition();
        Vec3 bp  = box.GetPosition();
        f32  r   = sphere.GetCollider().radius;
        Vec3 he  = box.GetCollider().halfExtents;

        // Vector from box center to sphere center
        Vec3 local = {sp.x-bp.x, sp.y-bp.y, sp.z-bp.z};

        // Closest point on box surface to sphere center
        Vec3 closest = {
            std::max(-he.x, std::min(local.x, he.x)),
            std::max(-he.y, std::min(local.y, he.y)),
            std::max(-he.z, std::min(local.z, he.z))
        };

        Vec3 diff = {
            local.x-closest.x,
            local.y-closest.y,
            local.z-closest.z
        };
        f32 dist = Length(diff);

        if (dist >= r) return false;

        c.normal = (dist > 0.0001f)
            ? Normalize(diff)
            : Vec3{0,1,0};
        c.penetration = r - dist;
        c.point = {
            bp.x+closest.x,
            bp.y+closest.y,
            bp.z+closest.z
        };
        return true;
    }

    bool PhysicsWorld::TestBoxBox(
        RigidBody& a, RigidBody& b, ContactPoint& c
    ) {
        Vec3 posA = a.GetPosition();
        Vec3 posB = b.GetPosition();
        Vec3 heA  = a.GetCollider().halfExtents;
        Vec3 heB  = b.GetCollider().halfExtents;

        f32 dx = posA.x - posB.x;
        f32 dy = posA.y - posB.y;
        f32 dz = posA.z - posB.z;

        f32 ox = (heA.x + heB.x) - std::abs(dx);
        f32 oy = (heA.y + heB.y) - std::abs(dy);
        f32 oz = (heA.z + heB.z) - std::abs(dz);

        if (ox <= 0 || oy <= 0 || oz <= 0) return false;

        if (ox < oy && ox < oz) {
            c.normal      = {dx < 0 ? -1.0f : 1.0f, 0, 0};
            c.penetration = ox;
        } else if (oy < oz) {
            c.normal      = {0, dy < 0 ? -1.0f : 1.0f, 0};
            c.penetration = oy;
        } else {
            c.normal      = {0, 0, dz < 0 ? -1.0f : 1.0f};
            c.penetration = oz;
        }

        c.point = {
            (posA.x + posB.x) * 0.5f,
            (posA.y + posB.y) * 0.5f,
            (posA.z + posB.z) * 0.5f
        };
        return true;
    }

    bool PhysicsWorld::TestSpherePlane(
        RigidBody& sphere, RigidBody& plane,
        ContactPoint& c
    ) {
        Vec3 pos    = sphere.GetPosition();
        f32  r      = sphere.GetCollider().radius;
        Vec3 n      = plane.GetCollider().planeNormal;
        f32  offset = plane.GetCollider().planeOffset;

        // Signed distance from sphere center to plane
        // positive = sphere is on the normal side (above)
        f32 dist = Dot(pos, n) - offset;

        // No collision if sphere is entirely above plane
        if (dist > r) return false;

        // Collision — sphere penetrates plane
        c.normal      = n;            // push sphere upward
        c.penetration = r - dist;     // how deep
        c.point       = {             // contact on plane
            pos.x - n.x * r,
            pos.y - n.y * r,
            pos.z - n.z * r
        };
        return true;
    }

    bool PhysicsWorld::TestBoxPlane(
        RigidBody& box, RigidBody& plane,
        ContactPoint& c
    ) {
        Vec3 pos    = box.GetPosition();
        Vec3 he     = box.GetCollider().halfExtents;
        Vec3 n      = plane.GetCollider().planeNormal;
        f32  offset = plane.GetCollider().planeOffset;

        // For axis-aligned box:
        // effective radius = projection of half-extents
        // onto plane normal
        f32 proj = std::abs(he.x * n.x) +
                   std::abs(he.y * n.y) +
                   std::abs(he.z * n.z);

        // Signed distance from box center to plane
        f32 dist = Dot(pos, n) - offset;

        // No collision if box is entirely above plane
        if (dist > proj) return false;

        c.normal      = n;
        c.penetration = proj - dist;
        c.point = {
            pos.x - n.x * proj,
            pos.y - n.y * proj,
            pos.z - n.z * proj
        };
        return true;
    }

    // -- Velocity resolution -----------------------------------
    void PhysicsWorld::ResolveVelocity(ContactPoint& c) {
        RigidBody& bodyA = bodies_[c.bodyA];
        RigidBody& bodyB = bodies_[c.bodyB];

        Vec3 vA = bodyA.GetVelocity();
        Vec3 vB = bodyB.GetVelocity();

        // Relative velocity along contact normal
        Vec3 relVel = {vA.x-vB.x, vA.y-vB.y, vA.z-vB.z};
        f32  vn     = Dot(relVel, c.normal);

        // Only resolve if bodies are approaching
        if (vn > 0.0f) return;

        // Coefficient of restitution
        f32 e = std::min(
            bodyA.GetCollider().restitution,
            bodyB.GetCollider().restitution);

        f32 iMA = bodyA.GetInvMass();
        f32 iMB = bodyB.GetInvMass();
        f32 iMSum = iMA + iMB;

        if (iMSum < 0.00001f) return;

        // Impulse magnitude
        f32 j = -(1.0f + e) * vn / iMSum;

        // Apply impulse
        Vec3 impulse = {
            c.normal.x * j,
            c.normal.y * j,
            c.normal.z * j
        };
        bodyA.ApplyImpulse(impulse);
        Vec3 negI = {-impulse.x,-impulse.y,-impulse.z};
        bodyB.ApplyImpulse(negI);

        // Friction
        Vec3 vAn = bodyA.GetVelocity();
        Vec3 vBn = bodyB.GetVelocity();
        Vec3 rv2 = {vAn.x-vBn.x, vAn.y-vBn.y,
                    vAn.z-vBn.z};

        f32  rvn  = Dot(rv2, c.normal);
        Vec3 tang = {
            rv2.x - c.normal.x * rvn,
            rv2.y - c.normal.y * rvn,
            rv2.z - c.normal.z * rvn
        };

        f32 tLen = Length(tang);
        if (tLen < 0.0001f) return;

        tang.x /= tLen;
        tang.y /= tLen;
        tang.z /= tLen;

        f32 jt  = -Dot(rv2, tang) / iMSum;
        f32 mu  = (bodyA.GetCollider().friction +
                   bodyB.GetCollider().friction) * 0.5f;
        f32 jMax = std::abs(j) * mu;
        if (jt >  jMax) jt =  jMax;
        if (jt < -jMax) jt = -jMax;

        Vec3 fImpulse = {tang.x*jt, tang.y*jt, tang.z*jt};
        bodyA.ApplyImpulse(fImpulse);
        Vec3 nfI = {-fImpulse.x,-fImpulse.y,-fImpulse.z};
        bodyB.ApplyImpulse(nfI);
    }

    // -- Positional correction ---------------------------------
    void PhysicsWorld::PositionalCorrect(ContactPoint& c) {
        RigidBody& bodyA = bodies_[c.bodyA];
        RigidBody& bodyB = bodies_[c.bodyB];

        f32 iMA   = bodyA.GetInvMass();
        f32 iMB   = bodyB.GetInvMass();
        f32 iMSum = iMA + iMB;
        if (iMSum < 0.00001f) return;

        // Baumgarte stabilization
        const f32 percent  = 0.8f;   // correction strength
        const f32 slop     = 0.002f; // tiny gap tolerance

        f32 corr = std::max(c.penetration - slop, 0.0f)
                   / iMSum * percent;

        Vec3 correction = {
            c.normal.x * corr,
            c.normal.y * corr,
            c.normal.z * corr
        };

        Vec3 posA = bodyA.GetPosition();
        posA.x += correction.x * iMA;
        posA.y += correction.y * iMA;
        posA.z += correction.z * iMA;
        bodyA.SetPosition(posA);

        Vec3 posB = bodyB.GetPosition();
        posB.x -= correction.x * iMB;
        posB.y -= correction.y * iMB;
        posB.z -= correction.z * iMB;
        bodyB.SetPosition(posB);
    }

    // -- Raycast -----------------------------------------------
    RaycastResult PhysicsWorld::Raycast(
        const Vec3& origin,
        const Vec3& direction,
        f32         maxDistance
    ) const {
        RaycastResult best;
        best.distance = maxDistance;
        Vec3 dir      = Normalize(direction);

        for (u32 i = 0;
             i < static_cast<u32>(bodies_.size()); i++) {
            const RigidBody& body = bodies_[i];
            ColliderShape shape   =
                body.GetCollider().shape;

            if (shape == ColliderShape::Plane) continue;

            if (shape == ColliderShape::Sphere) {
                Vec3 pos = body.GetPosition();
                f32  r   = body.GetCollider().radius;
                Vec3 oc  = {origin.x-pos.x,
                             origin.y-pos.y,
                             origin.z-pos.z};
                f32 b2 = 2.0f * Dot(oc, dir);
                f32 c2 = Dot(oc,oc) - r*r;
                f32 disc = b2*b2 - 4.0f*c2;
                if (disc >= 0) {
                    f32 t = (-b2 - std::sqrt(disc))*0.5f;
                    if (t > 0 && t < best.distance) {
                        best.hit       = true;
                        best.distance  = t;
                        best.bodyIndex = i;
                        best.point = {
                            origin.x+dir.x*t,
                            origin.y+dir.y*t,
                            origin.z+dir.z*t
                        };
                        best.normal = Normalize({
                            best.point.x-pos.x,
                            best.point.y-pos.y,
                            best.point.z-pos.z
                        });
                    }
                }
            }
        }
        return best;
    }

    // -- PhysicsSystemImpl -------------------------------------
    PhysicsSystemImpl::PhysicsSystemImpl()  = default;
    PhysicsSystemImpl::~PhysicsSystemImpl() = default;

    VoidResult PhysicsSystemImpl::Initialize() {
        world_ = std::make_unique<PhysicsWorld>();
        world_->SetGravity({0, -9.81f, 0});
        std::cout
            << "[Physics] World ready. "
            << "Gravity=(0,-9.81,0)\n";
        return VoidResult::Ok();
    }

    void PhysicsSystemImpl::Shutdown() {
        world_.reset();
        std::cout << "[Physics] Shutdown.\n";
    }


    void PhysicsSystemImpl::StepSimulation(f32 dt) {
        if (world_) world_->Step(dt);
    }

    void PhysicsSystemImpl::SetGravity(const Vec3& g) {
        if (world_) world_->SetGravity(g);
    }

    Vec3 PhysicsSystemImpl::GetGravity() const {
        return world_ ? world_->GetGravity() : Vec3::Zero();
    }

    void PhysicsSystemImpl::AddRigidBody(
        EntityID e, const RigidBodyDesc& d
    ) {
        if (!world_) return;
        u32 id = world_->AddBody(d);
        entityToBody_[e]  = id;
        bodyToEntity_[id] = e;
    }

    void PhysicsSystemImpl::RemoveRigidBody(EntityID e) {
        auto it = entityToBody_.find(e);
        if (it == entityToBody_.end()) return;
        if (world_) world_->RemoveBody(it->second);
        bodyToEntity_.erase(it->second);
        entityToBody_.erase(it);
    }

    void PhysicsSystemImpl::SetVelocity(
        EntityID e, const Vec3& v
    ) {
        auto it = entityToBody_.find(e);
        if (it == entityToBody_.end()) return;
        auto* b = world_->GetBody(it->second);
        if (b) b->SetVelocity(v);
    }

    Vec3 PhysicsSystemImpl::GetVelocity(EntityID e) const {
        auto it = entityToBody_.find(e);
        if (it == entityToBody_.end()) return Vec3::Zero();
        auto* b = world_->GetBody(it->second);
        return b ? b->GetVelocity() : Vec3::Zero();
    }

    void PhysicsSystemImpl::ApplyForce(
        EntityID e, const Vec3& f
    ) {
        auto it = entityToBody_.find(e);
        if (it == entityToBody_.end()) return;
        auto* b = world_->GetBody(it->second);
        if (b) b->ApplyForce(f);
    }

    void PhysicsSystemImpl::ApplyImpulse(
        EntityID e, const Vec3& impulse
    ) {
        auto it = entityToBody_.find(e);
        if (it == entityToBody_.end()) return;
        auto* b = world_->GetBody(it->second);
        if (b) b->ApplyImpulse(impulse);
    }

    RaycastHit PhysicsSystemImpl::Raycast(
        const Vec3& o, const Vec3& d, f32 dist
    ) const {
        RaycastHit hit;
        if (!world_) return hit;
        auto r    = world_->Raycast(o, d, dist);
        hit.hit      = r.hit;
        hit.distance = r.distance;
        hit.point    = r.point;
        hit.normal   = r.normal;
        return hit;
    }

    void PhysicsSystemImpl::ClearAllBodies() {
        if (!world_) return;
        world_->ClearAllBodies();
        entityToBody_.clear();
        bodyToEntity_.clear();
        std::cout << "[Physics] All bodies cleared.\n";
    }

    u32 PhysicsSystemImpl::AddBody(const RigidBodyDesc& d) {
        return world_ ? world_->AddBody(d) : 0;
    }

    void PhysicsSystemImpl::RemoveBody(u32 id) {
        if (world_) world_->RemoveBody(id);
    }

    RigidBody* PhysicsSystemImpl::GetBody(u32 id) {
        return world_ ? world_->GetBody(id) : nullptr;
    }

    Vec3 PhysicsSystemImpl::GetBodyPosition(u32 id) const {
        if (!world_) return Vec3::Zero();
        RigidBody* b =
            const_cast<PhysicsWorld*>(world_.get())
            ->GetBody(id);
        return b ? b->GetPosition() : Vec3::Zero();
    }

    // -- PhysicsModule -----------------------------------------
    PhysicsModule::PhysicsModule()  = default;
    PhysicsModule::~PhysicsModule() = default;

    VoidResult PhysicsModule::Initialize(
        const ModuleInitParams& params
    ) {
        ILogger* log = nullptr;
        if (params.context) log = params.context->Logger();

        if (log) log->Info("Physics","Initializing...");

        physics_ = std::make_unique<PhysicsSystemImpl>();
        auto r   = physics_->Initialize();
        if (r.IsErr()) return r;

        if (params.context) {
            params.context->Register<IPhysics>(
                physics_.get());
        }

        if (log) log->Info("Physics",
            "Ready. Fixed step=1/120  SubSteps=8");

        return VoidResult::Ok();
    }

    void PhysicsModule::OnUpdate(f32 deltaTime) {
        if (!physics_) return;

        // Fixed timestep accumulator
        accumulator_ += deltaTime;

        // Clamp to avoid spiral of death
        if (accumulator_ > 0.1f) accumulator_ = 0.1f;

        while (accumulator_ >= fixedStep_) {
            physics_->StepSimulation(fixedStep_);
            accumulator_ -= fixedStep_;
        }
    }

    void PhysicsModule::OnFixedUpdate(f32 fixedDt) {
        RIFTCORE_UNUSED(fixedDt);
    }

    void PhysicsModule::Shutdown() {
        std::cout << "[Physics] Shutting down...\n";
        if (physics_) physics_->Shutdown();
        physics_.reset();
        std::cout << "[Physics] Shutdown complete.\n";
    }

    ModuleDescriptor PhysicsModule::GetDescriptor() const {
        ModuleDescriptor d;
        d.name        = "Physics";
        d.version     = "0.1.0";
        d.apiVersion  = RIFTCORE_API_VERSION;
        d.description = "Rigid body physics";
        return d;
    }

    RIFTCORE_IMPLEMENT_MODULE(PhysicsModule)

} // namespace RiftCore



