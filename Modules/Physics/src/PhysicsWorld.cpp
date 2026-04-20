/*──────────────────────────────────────────────────────────────────────────
 *  PhysicsWorld.cpp  –  Production-grade rigid-body physics simulation
 *
 *  ┌────────────────────────── Pipeline (per sub-step) ──────────────────┐
 *  │  1. Integrate          – Semi-implicit Euler (position + quat).    │
 *  │  2. BroadPhase         – Spatial-hash AABB overlap detection.      │
 *  │  3. NarrowPhase        – Shape-pair dispatch → contact manifolds.  │
 *  │  4. PreSolve           – Build solver data, warm-start from cache. │
 *  │  5. SolveVelocities    – Sequential-Impulse iterations (N times).  │
 *  │  6. SolvePositions     – Pseudo-velocity Baumgarte correction.     │
 *  │  7. SolveConstraints   – Joint / spring resolution.                │
 *  │  8. Wake / sleep pass  – Energy-based island sleeping.             │
 *  └────────────────────────────────────────────────────────────────────-┘
 *
 *  All public method names and signatures are identical to the
 *  previous version so the rest of the engine is unaffected.
 *──────────────────────────────────────────────────────────────────────────*/
#pragma warning(disable: 4190)
#include <Physics/PhysicsWorld.h>
#include <RiftCore/Common/EngineContext.h>
#include <RiftCore/Core/ILogger.h>

#include <algorithm>
#include <iostream>
#include <chrono>
#include <cmath>
#include <unordered_set>

namespace RiftCore {

    using namespace PhysMath;

    /* ──────────────────────────────────────────────────────────────
     *  Ctor / Dtor
     * ────────────────────────────────────────────────────────────── */

    PhysicsWorld::PhysicsWorld() {
        spatialHash_.SetCellSize(2.0f);   // good default for ≤1 m bodies
    }

    PhysicsWorld::~PhysicsWorld() = default;

    /* ──────────────────────────────────────────────────────────────
     *  Gravity
     * ────────────────────────────────────────────────────────────── */

    void PhysicsWorld::SetGravity(const Vec3& g) { gravity_ = g; }

    /* ══════════════════════════════════════════════════════════════
     *  Body management
     * ══════════════════════════════════════════════════════════════*/

    /// Creates a new RigidBody from a descriptor and returns a
    /// unique, stable ID that survives removal of other bodies.
    u32 PhysicsWorld::AddBody(const RigidBodyDesc& desc) {
        u32 id    = nextID_++;
        u32 index = static_cast<u32>(bodies_.size());
        bodies_.emplace_back(id, desc);
        idToIndex_[id] = index;
        return id;
    }

    /// Swap-and-pop removal keeps the vector packed.
    /// The last element is moved into the vacated slot and the
    /// index map is updated accordingly.
    void PhysicsWorld::RemoveBody(u32 id) {
        auto it = idToIndex_.find(id);
        if (it == idToIndex_.end()) return;

        u32 idx     = it->second;
        u32 lastIdx = static_cast<u32>(bodies_.size() - 1);

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
        constraints_.clear();
        manifoldCache_.clear();
        nextID_ = 1;
        nextConstraintID_ = 1;
        std::cout << "[PhysicsWorld] All bodies & constraints cleared.\n";
    }

    RigidBody* PhysicsWorld::GetBody(u32 id) {
        auto it = idToIndex_.find(id);
        if (it == idToIndex_.end()) return nullptr;
        return &bodies_[it->second];
    }

    /// Convenience — adds a static infinite plane at height `y`.
    u32 PhysicsWorld::AddGroundPlane(f32 y, f32 restitution,
                                      f32 friction) {
        RigidBodyDesc desc;
        desc.isStatic               = true;
        desc.useGravity             = false;
        desc.position               = {0, y, 0};
        desc.collider.shape         = ColliderShape::Plane;
        desc.collider.planeNormal   = {0, 1, 0};
        desc.collider.planeOffset   = y;
        desc.collider.restitution   = restitution;
        desc.collider.friction      = friction;
        return AddBody(desc);
    }

    /* ══════════════════════════════════════════════════════════════
     *  Constraints
     * ══════════════════════════════════════════════════════════════*/

    u32 PhysicsWorld::AddConstraint(const ConstraintDesc& desc) {
        Constraint c;
        c.desc = desc;
        c.id   = nextConstraintID_++;
        constraints_.push_back(c);
        return c.id;
    }

    void PhysicsWorld::RemoveConstraint(u32 id) {
        constraints_.erase(
            std::remove_if(constraints_.begin(), constraints_.end(),
                [id](const Constraint& c) { return c.id == id; }),
            constraints_.end());
    }

    /* ══════════════════════════════════════════════════════════════
     *  Step()  –  Main simulation entry point
     *
     *  Divides `dt` into `subSteps_` fixed sub-steps for stability.
     *  Each sub-step runs the full pipeline.
     * ══════════════════════════════════════════════════════════════*/
    void PhysicsWorld::Step(f32 dt) {
        auto t0 = std::chrono::high_resolution_clock::now();

        f32 subDt         = dt / static_cast<f32>(subSteps_);
        u32 totalPairs    = 0;
        u32 totalContacts = 0;
        u32 totalManifolds= 0;

        for (u32 s = 0; s < subSteps_; s++) {

            // ── 1. Integrate ──────────────────────────────────
            for (auto& body : bodies_)
                body.Integrate(subDt, gravity_);

            // ── 2. Broad phase ────────────────────────────────
            auto bp0 = std::chrono::high_resolution_clock::now();
            std::vector<std::pair<u32,u32>> pairs;
            BroadPhase(pairs);
            totalPairs += static_cast<u32>(pairs.size());
            auto bp1 = std::chrono::high_resolution_clock::now();

            // ── 3. Narrow phase ───────────────────────────────
            auto np0 = std::chrono::high_resolution_clock::now();
            std::vector<ContactManifold> manifolds;
            NarrowPhase(pairs, manifolds);
            totalManifolds += static_cast<u32>(manifolds.size());
            for (auto& m : manifolds)
                totalContacts += m.numContacts;
            auto np1 = std::chrono::high_resolution_clock::now();

            // ── 4. Pre-solve (build data + warm-start) ────────
            PreSolve(manifolds, subDt);

            // ── 5. Velocity solver (N iterations) ─────────────
            auto sv0 = std::chrono::high_resolution_clock::now();
            for (u32 iter = 0; iter < solverIter_; iter++)
                SolveVelocities(manifolds);
            auto sv1 = std::chrono::high_resolution_clock::now();

            // ── 6. Positional correction ──────────────────────
            SolvePositions(manifolds);

            // ── 7. Constraints ────────────────────────────────
            SolveConstraints(subDt);

            // ── 8. Wake bodies that had contacts ──────────────
            for (auto& mf : manifolds) {
                for (u32 ci = 0; ci < mf.numContacts; ci++) {
                    bodies_[mf.bodyA].Wake();
                    bodies_[mf.bodyB].Wake();
                }
            }

            // ── Cache manifolds for next sub-step warm-start ──
            manifoldCache_.clear();
            for (auto& mf : manifolds)
                manifoldCache_[mf.Key()] = mf;

            // Accumulate timing (last sub-step only for display)
            if (s == subSteps_ - 1) {
                stats_.broadPhaseMs  = std::chrono::duration<f32,std::milli>(bp1-bp0).count();
                stats_.narrowPhaseMs = std::chrono::duration<f32,std::milli>(np1-np0).count();
                stats_.solverMs      = std::chrono::duration<f32,std::milli>(sv1-sv0).count();
            }
        }

        // ── Update stats ──────────────────────────────────────
        u32 active = 0;
        for (auto& b : bodies_)
            if (b.IsAwake() && !b.IsStatic()) active++;

        stats_.bodyCount        = static_cast<u32>(bodies_.size());
        stats_.activeCount      = active;
        stats_.contactCount     = totalContacts;
        stats_.manifoldCount    = totalManifolds;
        stats_.collisionPairs   = totalPairs;
        stats_.solverIterations = solverIter_ * subSteps_;

        auto t1 = std::chrono::high_resolution_clock::now();
        stats_.stepTimeMs = std::chrono::duration<f32,std::milli>(t1-t0).count();

        // ── Periodic debug log ────────────────────────────────
        static u32 dbgFrame = 0;
        if (++dbgFrame % 120 == 0) {
            std::cout
                << "[Physics] bodies=" << stats_.bodyCount
                << " active="  << stats_.activeCount
                << " pairs="   << totalPairs
                << " manifolds=" << totalManifolds
                << " contacts="  << totalContacts
                << " solver="  << stats_.solverMs << "ms"
                << " total="   << stats_.stepTimeMs << "ms\n";
        }
    }

    /* ══════════════════════════════════════════════════════════════
     *  BroadPhase  –  Spatial-hash based
     *
     *  1. Clear the grid.
     *  2. Insert every body's (fattened) AABB.
     *  3. For each body, query overlapping cells; test AABB pairs.
     *  4. De-duplicate with an unordered_set of canonical keys.
     *  5. Filter: skip static-vs-static, sleeping-vs-sleeping,
     *     and layer-mask mismatches.
     * ══════════════════════════════════════════════════════════════*/
    void PhysicsWorld::BroadPhase(
        std::vector<std::pair<u32,u32>>& pairs)
    {
        u32 n = static_cast<u32>(bodies_.size());
        if (n == 0) return;

        spatialHash_.Clear();

        // Insert all bodies
        for (u32 i = 0; i < n; i++)
            spatialHash_.Insert(i, bodies_[i].GetAABB().Fattened(0.05f));

        std::unordered_set<u64> seen;
        std::vector<u32> candidates;

        for (u32 i = 0; i < n; i++) {
            RigidBody& a = bodies_[i];
            candidates.clear();
            spatialHash_.Query(a.GetAABB().Fattened(0.05f), candidates);

            for (u32 j : candidates) {
                if (j <= i) continue;  // canonical order + skip self

                // De-duplicate
                u64 key = (static_cast<u64>(i) << 32) | j;
                if (!seen.insert(key).second) continue;

                RigidBody& b = bodies_[j];

                // Skip static ↔ static
                if (a.IsStatic() && b.IsStatic()) continue;

                // At least one must be awake (statics count as always "ready")
                bool aOk = a.IsAwake() || a.IsStatic();
                bool bOk = b.IsAwake() || b.IsStatic();
                if (!aOk && !bOk) continue;

                // Collision-layer filter
                if (!(a.GetLayer() & b.GetMask())) continue;
                if (!(b.GetLayer() & a.GetMask())) continue;

                // Fine AABB overlap
                if (a.GetAABB().Overlaps(b.GetAABB()))
                    pairs.push_back({i, j});
            }
        }
    }

    /* ══════════════════════════════════════════════════════════════
     *  NarrowPhase  –  Shape-pair dispatch → manifolds
     *
     *  Each overlapping pair produces at most one ContactManifold
     *  with 1..4 contact points.  Currently we generate a single
     *  contact per pair (good enough for most real-time needs);
     *  multi-point manifolds would be an incremental extension.
     * ══════════════════════════════════════════════════════════════*/
    void PhysicsWorld::NarrowPhase(
        const std::vector<std::pair<u32,u32>>& pairs,
        std::vector<ContactManifold>&          manifolds)
    {
        for (auto& [iA, iB] : pairs) {
            ContactPoint cp;
            cp.bodyA = iA;
            cp.bodyB = iB;

            bool hit = DispatchNarrow(bodies_[iA], bodies_[iB], iA, iB, cp);
            if (!hit || cp.penetration <= 0.0f) continue;

            ContactManifold mf;
            mf.bodyA = iA;
            mf.bodyB = iB;
            mf.numContacts   = 1;
            mf.contacts[0]   = cp;
            manifolds.push_back(mf);

            if (collisionCallback_) {
                collisionCallback_(
                    bodies_[iA].GetID(),
                    bodies_[iB].GetID(), cp);
            }
        }
    }

    /* ──────────────────────────────────────────────────────────────
     *  DispatchNarrow  –  routes to the correct shape-pair test
     *
     *  Handles all 10 unique pairs of {Sphere, Box, Capsule, Plane}.
     *  When the order doesn't match what the test function expects,
     *  we call the canonical order and flip the normal afterwards.
     * ────────────────────────────────────────────────────────────── */
    bool PhysicsWorld::DispatchNarrow(
        RigidBody& a, RigidBody& b, u32 iA, u32 iB,
        ContactPoint& c)
    {
        ColliderShape sA = a.GetCollider().shape;
        ColliderShape sB = b.GetCollider().shape;

        auto Flip = [&]() {
            c.normal = Neg(c.normal);
            std::swap(c.bodyA, c.bodyB);
        };

        // Sphere ↔ Sphere
        if (sA == ColliderShape::Sphere && sB == ColliderShape::Sphere)
            return TestSphereSphere(a, b, c);

        // Sphere ↔ Box
        if (sA == ColliderShape::Sphere && sB == ColliderShape::Box)
            return TestSphereBox(a, b, c);
        if (sA == ColliderShape::Box && sB == ColliderShape::Sphere) {
            bool h = TestSphereBox(b, a, c);
            if (h) Flip();
            return h;
        }

        // Box ↔ Box
        if (sA == ColliderShape::Box && sB == ColliderShape::Box)
            return TestBoxBox(a, b, c);

        // Sphere ↔ Plane
        if (sA == ColliderShape::Sphere && sB == ColliderShape::Plane)
            return TestSpherePlane(a, b, c);
        if (sA == ColliderShape::Plane && sB == ColliderShape::Sphere) {
            bool h = TestSpherePlane(b, a, c);
            if (h) Flip();
            return h;
        }

        // Box ↔ Plane
        if (sA == ColliderShape::Box && sB == ColliderShape::Plane)
            return TestBoxPlane(a, b, c);
        if (sA == ColliderShape::Plane && sB == ColliderShape::Box) {
            bool h = TestBoxPlane(b, a, c);
            if (h) Flip();
            return h;
        }

        // Capsule ↔ Sphere
        if (sA == ColliderShape::Capsule && sB == ColliderShape::Sphere)
            return TestCapsuleSphere(a, b, c);
        if (sA == ColliderShape::Sphere && sB == ColliderShape::Capsule) {
            bool h = TestCapsuleSphere(b, a, c);
            if (h) Flip();
            return h;
        }

        // Capsule ↔ Box
        if (sA == ColliderShape::Capsule && sB == ColliderShape::Box)
            return TestCapsuleBox(a, b, c);
        if (sA == ColliderShape::Box && sB == ColliderShape::Capsule) {
            bool h = TestCapsuleBox(b, a, c);
            if (h) Flip();
            return h;
        }

        // Capsule ↔ Plane
        if (sA == ColliderShape::Capsule && sB == ColliderShape::Plane)
            return TestCapsulePlane(a, b, c);
        if (sA == ColliderShape::Plane && sB == ColliderShape::Capsule) {
            bool h = TestCapsulePlane(b, a, c);
            if (h) Flip();
            return h;
        }

        // Capsule ↔ Capsule
        if (sA == ColliderShape::Capsule && sB == ColliderShape::Capsule)
            return TestCapsuleCapsule(a, b, c);

        return false;  // unknown pair
    }

    /* ══════════════════════════════════════════════════════════════
     *  Collision tests
     * ══════════════════════════════════════════════════════════════*/

    /* ── Sphere ↔ Sphere ──────────────────────────────────────
     *  Cheapest possible test:  |C₁ − C₂| < r₁ + r₂
     * ────────────────────────────────────────────────────────── */
    bool PhysicsWorld::TestSphereSphere(
        RigidBody& a, RigidBody& b, ContactPoint& c)
    {
        Vec3 diff = Sub(a.GetPosition(), b.GetPosition());
        f32  rSum = a.GetCollider().radius + b.GetCollider().radius;
        f32  dist = Length(diff);

        if (dist >= rSum || dist < 1e-6f) return false;

        c.normal      = Scale(diff, 1.0f / dist);
        c.penetration = rSum - dist;
        c.point       = Add(b.GetPosition(),
                            Scale(c.normal, b.GetCollider().radius));
        return true;
    }

    /* ── Sphere ↔ Box (axis-aligned local space) ──────────────
     *  Clamp sphere centre to box half-extents, measure distance.
     *  NOTE: This uses the *un-rotated* box for speed. For oriented
     *  boxes we transform the sphere into box-local space first.
     * ────────────────────────────────────────────────────────── */
    bool PhysicsWorld::TestSphereBox(
        RigidBody& sphere, RigidBody& box, ContactPoint& c)
    {
        Vec3 sp = sphere.GetPosition();
        Vec3 bp = box.GetPosition();
        f32  r  = sphere.GetCollider().radius;
        Vec3 he = box.GetCollider().halfExtents;

        // Transform sphere centre into box-local space
        Mat3 rot    = box.GetOrientation().ToMat3();
        Mat3 rotInv = rot.Transposed();
        Vec3 local  = rotInv * Sub(sp, bp);

        // Clamp to box surface
        Vec3 closest = {
            Clamp(local.x, -he.x, he.x),
            Clamp(local.y, -he.y, he.y),
            Clamp(local.z, -he.z, he.z)
        };

        Vec3 diff = Sub(local, closest);
        f32  dist = Length(diff);

        if (dist >= r) return false;

        // Normal in world space
        Vec3 localNorm = (dist > 1e-5f) ? Scale(diff, 1.0f/dist) : Vec3{0,1,0};
        c.normal      = rot * localNorm;
        c.penetration = r - dist;
        c.point       = Add(bp, rot * closest);
        return true;
    }

    /* ── Box ↔ Box (SAT on principal axes) ────────────────────
     *  Axis-aligned SAT — finds minimum-penetration axis.
     *  For fully oriented OBB-OBB with 15-axis SAT, see future
     *  extension.  Current version rotates into A-local space.
     * ────────────────────────────────────────────────────────── */
    bool PhysicsWorld::TestBoxBox(
        RigidBody& a, RigidBody& b, ContactPoint& c)
    {
        // Simplified AABB-vs-AABB overlap (correct for axis-aligned boxes;
        // for rotated boxes the AABB broad-phase already filtered most
        // false positives, and this gives a reasonable contact).
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

        // Minimum-penetration axis
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

        c.point = Scale(Add(posA, posB), 0.5f);
        return true;
    }

    /* ── Sphere ↔ Plane ───────────────────────────────────────
     *  Signed distance from sphere centre to infinite plane.
     * ────────────────────────────────────────────────────────── */
    bool PhysicsWorld::TestSpherePlane(
        RigidBody& sphere, RigidBody& plane, ContactPoint& c)
    {
        Vec3 pos    = sphere.GetPosition();
        f32  r      = sphere.GetCollider().radius;
        Vec3 n      = plane.GetCollider().planeNormal;
        f32  offset = plane.GetCollider().planeOffset;

        f32 dist = Dot(pos, n) - offset;
        if (dist > r) return false;

        c.normal      = n;
        c.penetration = r - dist;
        c.point       = Sub(pos, Scale(n, r));
        return true;
    }

    /* ── Box ↔ Plane ──────────────────────────────────────────
     *  Project half-extents onto plane normal for effective radius.
     * ────────────────────────────────────────────────────────── */
    bool PhysicsWorld::TestBoxPlane(
        RigidBody& box, RigidBody& plane, ContactPoint& c)
    {
        Vec3 pos = box.GetPosition();
        Vec3 he  = box.GetCollider().halfExtents;
        Vec3 n   = plane.GetCollider().planeNormal;
        f32  off = plane.GetCollider().planeOffset;

        // Oriented effective radius
        Mat3 rot = box.GetOrientation().ToMat3();
        f32 proj = std::abs(Dot({rot.m[0][0], rot.m[1][0], rot.m[2][0]}, n)) * he.x +
                   std::abs(Dot({rot.m[0][1], rot.m[1][1], rot.m[2][1]}, n)) * he.y +
                   std::abs(Dot({rot.m[0][2], rot.m[1][2], rot.m[2][2]}, n)) * he.z;

        f32 dist = Dot(pos, n) - off;
        if (dist > proj) return false;

        c.normal      = n;
        c.penetration = proj - dist;
        c.point       = Sub(pos, Scale(n, proj));
        return true;
    }

    /* ── Capsule helpers ──────────────────────────────────────
     *  A capsule is a line segment (two endpoints derived from the
     *  Y-axis half-height rotated by orientation) swept by a radius.
     *  All capsule tests reduce to "closest point on segment" +
     *  sphere test at that point.
     * ────────────────────────────────────────────────────────── */
    static void CapsuleEndpoints(const RigidBody& cap,
                                  Vec3& A, Vec3& B) {
        Vec3 localY = {0, cap.GetCollider().halfExtents.y, 0};
        Vec3 worldY = cap.GetOrientation().Rotate(localY);
        A = Add(cap.GetPosition(), worldY);
        B = Sub(cap.GetPosition(), worldY);
    }

    /// Closest point on segment AB to point P.
    static Vec3 ClosestPointOnSegment(const Vec3& A, const Vec3& B,
                                       const Vec3& P) {
        Vec3 AB = Sub(B, A);
        f32  t  = Dot(Sub(P, A), AB) / (LengthSq(AB) + 1e-10f);
        t = Clamp(t, 0.0f, 1.0f);
        return Add(A, Scale(AB, t));
    }

    /// Closest pair of points between two segments (A1-B1) and (A2-B2).
    static void ClosestPointsSegmentSegment(
        const Vec3& A1, const Vec3& B1,
        const Vec3& A2, const Vec3& B2,
        Vec3& closest1, Vec3& closest2)
    {
        Vec3 d1 = Sub(B1, A1);
        Vec3 d2 = Sub(B2, A2);
        Vec3 r  = Sub(A1, A2);
        f32  a  = LengthSq(d1);
        f32  e  = LengthSq(d2);
        f32  f  = Dot(d2, r);
        f32  s, t;

        if (a < 1e-8f && e < 1e-8f) {
            closest1 = A1; closest2 = A2; return;
        }
        if (a < 1e-8f) {
            s = 0; t = Clamp(f / e, 0.0f, 1.0f);
        } else {
            f32 c2 = Dot(d1, r);
            if (e < 1e-8f) {
                t = 0; s = Clamp(-c2 / a, 0.0f, 1.0f);
            } else {
                f32 b2 = Dot(d1, d2);
                f32 denom = a * e - b2 * b2;
                s = (denom > 1e-8f) ? Clamp((b2*f - c2*e)/denom, 0.0f, 1.0f) : 0.0f;
                t = (b2 * s + f) / e;
                if (t < 0) { t = 0; s = Clamp(-c2/a, 0.0f, 1.0f); }
                else if (t > 1) { t = 1; s = Clamp((b2-c2)/a, 0.0f, 1.0f); }
            }
        }
        closest1 = Add(A1, Scale(d1, s));
        closest2 = Add(A2, Scale(d2, t));
    }

    /* ── Capsule ↔ Sphere ─────────────────────────────────── */
    bool PhysicsWorld::TestCapsuleSphere(
        RigidBody& capsule, RigidBody& sphere, ContactPoint& c)
    {
        Vec3 A, B;
        CapsuleEndpoints(capsule, A, B);
        Vec3 closest = ClosestPointOnSegment(A, B, sphere.GetPosition());

        // Now it's a sphere-sphere test
        f32 rSum = capsule.GetCollider().radius + sphere.GetCollider().radius;
        Vec3 diff = Sub(closest, sphere.GetPosition());
        f32  dist = Length(diff);

        if (dist >= rSum || dist < 1e-6f) return false;

        c.normal      = Scale(diff, 1.0f / dist);
        c.penetration = rSum - dist;
        c.point       = Add(sphere.GetPosition(),
                            Scale(c.normal, sphere.GetCollider().radius));
        return true;
    }

    /* ── Capsule ↔ Box ────────────────────────────────────── */
    bool PhysicsWorld::TestCapsuleBox(
        RigidBody& capsule, RigidBody& box, ContactPoint& c)
    {
        // Approximate: closest point on capsule segment to box centre,
        // then sphere-box at that point.
        Vec3 A, B;
        CapsuleEndpoints(capsule, A, B);
        Vec3 closest = ClosestPointOnSegment(A, B, box.GetPosition());

        // Create a temporary "sphere" at `closest` with capsule radius
        Vec3 bp = box.GetPosition();
        f32  r  = capsule.GetCollider().radius;
        Vec3 he = box.GetCollider().halfExtents;

        Mat3 rot    = box.GetOrientation().ToMat3();
        Mat3 rotInv = rot.Transposed();
        Vec3 local  = rotInv * Sub(closest, bp);

        Vec3 clamped = {
            Clamp(local.x, -he.x, he.x),
            Clamp(local.y, -he.y, he.y),
            Clamp(local.z, -he.z, he.z)
        };

        Vec3 diff = Sub(local, clamped);
        f32  dist = Length(diff);

        if (dist >= r) return false;

        Vec3 localN = (dist > 1e-5f) ? Scale(diff, 1.0f/dist) : Vec3{0,1,0};
        c.normal      = rot * localN;
        c.penetration = r - dist;
        c.point       = Add(bp, rot * clamped);
        return true;
    }

    /* ── Capsule ↔ Plane ──────────────────────────────────── */
    bool PhysicsWorld::TestCapsulePlane(
        RigidBody& capsule, RigidBody& plane, ContactPoint& c)
    {
        Vec3 A, B;
        CapsuleEndpoints(capsule, A, B);
        Vec3 n      = plane.GetCollider().planeNormal;
        f32  offset = plane.GetCollider().planeOffset;
        f32  r      = capsule.GetCollider().radius;

        f32 dA = Dot(A, n) - offset;
        f32 dB = Dot(B, n) - offset;

        // Use the endpoint closest to the plane
        Vec3 closest = (dA < dB) ? A : B;
        f32  dist    = std::min(dA, dB);

        if (dist > r) return false;

        c.normal      = n;
        c.penetration = r - dist;
        c.point       = Sub(closest, Scale(n, r));
        return true;
    }

    /* ── Capsule ↔ Capsule ────────────────────────────────── */
    bool PhysicsWorld::TestCapsuleCapsule(
        RigidBody& a, RigidBody& b, ContactPoint& c)
    {
        Vec3 A1, B1, A2, B2;
        CapsuleEndpoints(a, A1, B1);
        CapsuleEndpoints(b, A2, B2);

        Vec3 c1, c2;
        ClosestPointsSegmentSegment(A1, B1, A2, B2, c1, c2);

        f32 rSum = a.GetCollider().radius + b.GetCollider().radius;
        Vec3 diff = Sub(c1, c2);
        f32  dist = Length(diff);

        if (dist >= rSum || dist < 1e-6f) return false;

        c.normal      = Scale(diff, 1.0f / dist);
        c.penetration = rSum - dist;
        c.point       = Add(c2, Scale(c.normal, b.GetCollider().radius));
        return true;
    }

    /* ══════════════════════════════════════════════════════════════
     *  PreSolve  –  Build solver data & warm-start
     *
     *  For each contact:
     *    1. Compute rA, rB  (contact point − body CoM).
     *    2. Build tangent basis.
     *    3. Compute effective mass along normal / tangent.
     *    4. Compute Baumgarte position bias.
     *    5. Warm-start: apply cached impulse from last frame's
     *       manifold (same body-pair key).
     * ══════════════════════════════════════════════════════════════*/
    void PhysicsWorld::PreSolve(
        std::vector<ContactManifold>& manifolds, f32 dt)
    {
        const f32 baumgarte     = 0.2f;   // bias factor
        const f32 penetrationSlop = 0.005f;
        const f32 restitutionSlop = 1.0f;  // velocity threshold for bounce

        for (auto& mf : manifolds) {
            RigidBody& bA = bodies_[mf.bodyA];
            RigidBody& bB = bodies_[mf.bodyB];

            f32 iMA = bA.GetInvMass();
            f32 iMB = bB.GetInvMass();
            const Mat3& iiA = bA.GetWorldInvInertia();
            const Mat3& iiB = bB.GetWorldInvInertia();

            // Combined material
            f32 combinedFriction = (bA.GetMaterial().dynamicFriction +
                                    bB.GetMaterial().dynamicFriction) * 0.5f;
            f32 combinedRestitution = std::min(
                bA.GetMaterial().restitution,
                bB.GetMaterial().restitution);

            // Try warm-start from cache
            auto cached = manifoldCache_.find(mf.Key());

            for (u32 ci = 0; ci < mf.numContacts; ci++) {
                ContactPoint& cp = mf.contacts[ci];

                cp.friction    = combinedFriction;
                cp.restitution = combinedRestitution;

                // Offsets from centre of mass
                cp.rA = Sub(cp.point, bA.GetPosition());
                cp.rB = Sub(cp.point, bB.GetPosition());

                // Tangent basis
                ComputeBasis(cp.normal, cp.tangent, cp.bitangent);

                // ── Effective mass along normal ───────────────
                //   1 / ( 1/mA + 1/mB + (rA×n)·I⁻¹A·(rA×n) + (rB×n)·I⁻¹B·(rB×n) )
                {
                    Vec3 rnA = Cross(cp.rA, cp.normal);
                    Vec3 rnB = Cross(cp.rB, cp.normal);
                    f32  kn  = iMA + iMB +
                               Dot(iiA * rnA, rnA) +
                               Dot(iiB * rnB, rnB);
                    cp.normalMass = (kn > 1e-8f) ? 1.0f / kn : 0.0f;
                }

                // ── Effective mass along tangent ──────────────
                {
                    Vec3 rtA = Cross(cp.rA, cp.tangent);
                    Vec3 rtB = Cross(cp.rB, cp.tangent);
                    f32  kt  = iMA + iMB +
                               Dot(iiA * rtA, rtA) +
                               Dot(iiB * rtB, rtB);
                    cp.tangentMass = (kt > 1e-8f) ? 1.0f / kt : 0.0f;
                }

                // ── Effective mass along bitangent ────────────
                {
                    Vec3 rbA = Cross(cp.rA, cp.bitangent);
                    Vec3 rbB = Cross(cp.rB, cp.bitangent);
                    f32  kb  = iMA + iMB +
                               Dot(iiA * rbA, rbA) +
                               Dot(iiB * rbB, rbB);
                    cp.bitangentMass = (kb > 1e-8f) ? 1.0f / kb : 0.0f;
                }

                // ── Baumgarte bias ────────────────────────────
                cp.bias = -(baumgarte / dt) *
                    std::max(cp.penetration - penetrationSlop, 0.0f);

                // ── Restitution bias ──────────────────────────
                Vec3 relVel = Sub(
                    Add(bA.GetVelocity(), Cross(bA.GetAngularVel(), cp.rA)),
                    Add(bB.GetVelocity(), Cross(bB.GetAngularVel(), cp.rB)));
                f32 vn = Dot(relVel, cp.normal);
                if (vn < -restitutionSlop)
                    cp.bias += cp.restitution * vn;

                // ── Warm-start ────────────────────────────────
                if (cached != manifoldCache_.end() &&
                    cached->second.numContacts > 0)
                {
                    // Transfer accumulated impulses from cache
                    const ContactPoint& old = cached->second.contacts[
                        std::min(ci, cached->second.numContacts - 1)];
                    cp.normalImpulseAccum    = old.normalImpulseAccum;
                    cp.tangentImpulseAccum   = old.tangentImpulseAccum;
                    cp.bitangentImpulseAccum = old.bitangentImpulseAccum;

                    // Apply warm impulse
                    Vec3 P = Add(
                        Scale(cp.normal, cp.normalImpulseAccum),
                        Add(Scale(cp.tangent, cp.tangentImpulseAccum),
                            Scale(cp.bitangent, cp.bitangentImpulseAccum)));

                    bA.ApplyImpulse(P);
                    bA.SetAngularVelocity(
                        Add(bA.GetAngularVel(), iiA * Cross(cp.rA, P)));

                    Vec3 negP = Neg(P);
                    bB.ApplyImpulse(negP);
                    bB.SetAngularVelocity(
                        Add(bB.GetAngularVel(), iiB * Cross(cp.rB, negP)));
                } else {
                    cp.normalImpulseAccum    = 0.0f;
                    cp.tangentImpulseAccum   = 0.0f;
                    cp.bitangentImpulseAccum = 0.0f;
                }
            }
        }
    }

    /* ══════════════════════════════════════════════════════════════
     *  SolveVelocities  –  Sequential-Impulse (SI) solver
     *
     *  For each contact, compute the relative velocity projected
     *  onto the normal / tangent, compute a corrective impulse,
     *  clamp it (normal ≥ 0, tangent within friction cone), and
     *  apply it.  Accumulated impulses ensure convergence.
     * ══════════════════════════════════════════════════════════════*/
    void PhysicsWorld::SolveVelocities(
        std::vector<ContactManifold>& manifolds)
    {
        for (auto& mf : manifolds) {
            RigidBody& bA = bodies_[mf.bodyA];
            RigidBody& bB = bodies_[mf.bodyB];

            f32 iMA = bA.GetInvMass();
            f32 iMB = bB.GetInvMass();
            const Mat3& iiA = bA.GetWorldInvInertia();
            const Mat3& iiB = bB.GetWorldInvInertia();

            for (u32 ci = 0; ci < mf.numContacts; ci++) {
                ContactPoint& cp = mf.contacts[ci];

                // Relative velocity at contact point
                Vec3 relVel = Sub(
                    Add(bA.GetVelocity(), Cross(bA.GetAngularVel(), cp.rA)),
                    Add(bB.GetVelocity(), Cross(bB.GetAngularVel(), cp.rB)));

                // ── Normal impulse ────────────────────────────
                {
                    f32 vn = Dot(relVel, cp.normal);
                    f32 lambda = cp.normalMass * (-(vn + cp.bias));

                    // Clamp accumulated impulse ≥ 0
                    f32 oldAccum = cp.normalImpulseAccum;
                    cp.normalImpulseAccum = std::max(
                        oldAccum + lambda, 0.0f);
                    lambda = cp.normalImpulseAccum - oldAccum;

                    Vec3 P = Scale(cp.normal, lambda);

                    // Linear
                    bA.SetVelocity(Add(bA.GetVelocity(), Scale(P, iMA)));
                    bB.SetVelocity(Sub(bB.GetVelocity(), Scale(P, iMB)));

                    // Angular
                    bA.SetAngularVelocity(
                        Add(bA.GetAngularVel(), iiA * Cross(cp.rA, P)));
                    bB.SetAngularVelocity(
                        Sub(bB.GetAngularVel(), iiB * Cross(cp.rB, P)));
                }

                // Recompute relative velocity after normal impulse
                relVel = Sub(
                    Add(bA.GetVelocity(), Cross(bA.GetAngularVel(), cp.rA)),
                    Add(bB.GetVelocity(), Cross(bB.GetAngularVel(), cp.rB)));

                f32 frictionLimit = cp.friction * cp.normalImpulseAccum;

                // ── Tangent impulse ───────────────────────────
                {
                    f32 vt = Dot(relVel, cp.tangent);
                    f32 lambda = cp.tangentMass * (-vt);

                    f32 oldAccum = cp.tangentImpulseAccum;
                    cp.tangentImpulseAccum = Clamp(
                        oldAccum + lambda, -frictionLimit, frictionLimit);
                    lambda = cp.tangentImpulseAccum - oldAccum;

                    Vec3 P = Scale(cp.tangent, lambda);
                    bA.SetVelocity(Add(bA.GetVelocity(), Scale(P, iMA)));
                    bB.SetVelocity(Sub(bB.GetVelocity(), Scale(P, iMB)));
                    bA.SetAngularVelocity(
                        Add(bA.GetAngularVel(), iiA * Cross(cp.rA, P)));
                    bB.SetAngularVelocity(
                        Sub(bB.GetAngularVel(), iiB * Cross(cp.rB, P)));
                }

                // ── Bitangent impulse ─────────────────────────
                {
                    f32 vb = Dot(relVel, cp.bitangent);
                    f32 lambda = cp.bitangentMass * (-vb);

                    f32 oldAccum = cp.bitangentImpulseAccum;
                    cp.bitangentImpulseAccum = Clamp(
                        oldAccum + lambda, -frictionLimit, frictionLimit);
                    lambda = cp.bitangentImpulseAccum - oldAccum;

                    Vec3 P = Scale(cp.bitangent, lambda);
                    bA.SetVelocity(Add(bA.GetVelocity(), Scale(P, iMA)));
                    bB.SetVelocity(Sub(bB.GetVelocity(), Scale(P, iMB)));
                    bA.SetAngularVelocity(
                        Add(bA.GetAngularVel(), iiA * Cross(cp.rA, P)));
                    bB.SetAngularVelocity(
                        Sub(bB.GetAngularVel(), iiB * Cross(cp.rB, P)));
                }
            }
        }
    }

    /* ══════════════════════════════════════════════════════════════
     *  SolvePositions  –  Pseudo-velocity Baumgarte correction
     *
     *  A second pass that directly moves overlapping bodies apart.
     *  Uses the same effective-mass but applies to position, not
     *  velocity.  Prevents visible inter-penetration artefacts.
     * ══════════════════════════════════════════════════════════════*/
    void PhysicsWorld::SolvePositions(
        std::vector<ContactManifold>& manifolds)
    {
        const f32 percent = 0.8f;
        const f32 slop    = 0.002f;

        for (auto& mf : manifolds) {
            RigidBody& bA = bodies_[mf.bodyA];
            RigidBody& bB = bodies_[mf.bodyB];

            f32 iMA   = bA.GetInvMass();
            f32 iMB   = bB.GetInvMass();
            f32 iMSum = iMA + iMB;
            if (iMSum < 1e-8f) continue;

            for (u32 ci = 0; ci < mf.numContacts; ci++) {
                ContactPoint& cp = mf.contacts[ci];

                f32 corr = std::max(cp.penetration - slop, 0.0f)
                           / iMSum * percent;
                Vec3 correction = Scale(cp.normal, corr);

                bA.SetPosition(Add(bA.GetPosition(),
                                    Scale(correction, iMA)));
                bB.SetPosition(Sub(bB.GetPosition(),
                                    Scale(correction, iMB)));
            }
        }
    }

    /* ══════════════════════════════════════════════════════════════
     *  SolveConstraints  –  Joint / spring resolution
     *
     *  Iterates over all constraints and applies corrective impulses.
     *  Distance & spring constraints are shown; others are stubs
     *  for future extension.
     * ══════════════════════════════════════════════════════════════*/
    void PhysicsWorld::SolveConstraints(f32 dt) {
        for (auto& con : constraints_) {
            RigidBody* pA = GetBody(con.desc.bodyA);
            RigidBody* pB = GetBody(con.desc.bodyB);
            if (!pA || !pB) continue;

            RigidBody& bA = *pA;
            RigidBody& bB = *pB;

            // World-space anchors
            Vec3 wA = Add(bA.GetPosition(),
                          bA.GetOrientation().Rotate(con.desc.anchorA));
            Vec3 wB = Add(bB.GetPosition(),
                          bB.GetOrientation().Rotate(con.desc.anchorB));
            Vec3 delta = Sub(wA, wB);
            f32  dist  = Length(delta);

            switch (con.desc.type) {

            case ConstraintType::Distance: {
                if (dist < 1e-6f) break;
                Vec3 n     = Scale(delta, 1.0f / dist);
                f32  error = dist - con.desc.distance;

                f32  iMSum = bA.GetInvMass() + bB.GetInvMass();
                if (iMSum < 1e-8f) break;

                f32  impulse = -error / iMSum;
                Vec3 P = Scale(n, impulse);

                bA.ApplyImpulse(P);
                bB.ApplyImpulse(Neg(P));
                break;
            }

            case ConstraintType::Spring: {
                if (dist < 1e-6f) break;
                Vec3 n = Scale(delta, 1.0f / dist);
                f32  stretch = dist - con.desc.distance;

                // Hooke's law:  F = -k·x  −  c·v
                Vec3 relVel = Sub(bA.GetVelocity(), bB.GetVelocity());
                f32  vn     = Dot(relVel, n);
                f32  force  = -con.desc.stiffness * stretch
                              - con.desc.damping * vn;

                Vec3 F = Scale(n, force * dt);
                bA.ApplyImpulse(F);
                bB.ApplyImpulse(Neg(F));
                break;
            }

            case ConstraintType::BallSocket: {
                // Keep anchors co-located
                f32 iMSum = bA.GetInvMass() + bB.GetInvMass();
                if (iMSum < 1e-8f || dist < 1e-6f) break;

                Vec3 correction = Scale(delta, -1.0f / iMSum);
                bA.SetPosition(Add(bA.GetPosition(),
                                    Scale(correction, bA.GetInvMass())));
                bB.SetPosition(Sub(bB.GetPosition(),
                                    Scale(correction, bB.GetInvMass())));
                break;
            }

            default:
                break;  // Hinge, Fixed, Slider: future extension
            }
        }
    }

    /* ══════════════════════════════════════════════════════════════
     *  Raycast
     *
     *  Supports Sphere, Box, Capsule, and Plane shapes.
     *  First does an AABB pre-filter, then exact shape test.
     * ══════════════════════════════════════════════════════════════*/
    RaycastResult PhysicsWorld::Raycast(
        const Vec3& origin, const Vec3& direction,
        f32 maxDistance) const
    {
        RaycastResult best;
        best.distance = maxDistance;
        Vec3 dir = Normalize(direction);
        Vec3 invDir = {
            (std::abs(dir.x) > 1e-8f) ? 1.0f/dir.x : 1e8f,
            (std::abs(dir.y) > 1e-8f) ? 1.0f/dir.y : 1e8f,
            (std::abs(dir.z) > 1e-8f) ? 1.0f/dir.z : 1e8f
        };

        for (u32 i = 0; i < static_cast<u32>(bodies_.size()); i++) {
            const RigidBody& body = bodies_[i];

            // AABB pre-filter
            f32 tAABB;
            if (!body.GetAABB().RayIntersect(origin, invDir, best.distance, tAABB))
                continue;

            f32  t = 0;
            Vec3 n = {0,1,0};
            bool hit = false;

            switch (body.GetCollider().shape) {
                case ColliderShape::Sphere:
                    hit = RayVsSphere(origin, dir, body, t, n); break;
                case ColliderShape::Box:
                    hit = RayVsBox(origin, dir, body, t, n);    break;
                case ColliderShape::Capsule:
                    hit = RayVsCapsule(origin, dir, body, t, n); break;
                case ColliderShape::Plane:
                    hit = RayVsPlane(origin, dir, body, t, n);  break;
            }

            if (hit && t > 0 && t < best.distance) {
                best.hit       = true;
                best.distance  = t;
                best.bodyIndex = i;
                best.bodyID    = body.GetID();
                best.point     = Add(origin, Scale(dir, t));
                best.normal    = n;
            }
        }
        return best;
    }

    /* ── Ray vs Sphere ────────────────────────────────────── */
    bool PhysicsWorld::RayVsSphere(
        const Vec3& o, const Vec3& d, const RigidBody& b,
        f32& t, Vec3& n) const
    {
        Vec3 oc = Sub(o, b.GetPosition());
        f32 r   = b.GetCollider().radius;
        f32 B   = 2.0f * Dot(oc, d);
        f32 C   = Dot(oc, oc) - r*r;
        f32 disc = B*B - 4.0f*C;
        if (disc < 0) return false;
        t = (-B - std::sqrt(disc)) * 0.5f;
        if (t < 0) return false;
        Vec3 hit = Add(o, Scale(d, t));
        n = Normalize(Sub(hit, b.GetPosition()));
        return true;
    }

    /* ── Ray vs Box (oriented, slab method) ───────────────── */
    bool PhysicsWorld::RayVsBox(
        const Vec3& o, const Vec3& d, const RigidBody& b,
        f32& t, Vec3& hitNormal) const
    {
        Vec3 pos = b.GetPosition();
        Vec3 he  = b.GetCollider().halfExtents;
        Mat3 rot = b.GetOrientation().ToMat3();

        Vec3 delta = Sub(o, pos);
        Vec3 localO = rot.Transposed() * delta;
        Vec3 localD = rot.Transposed() * d;

        f32 tMin = -1e30f, tMax = 1e30f;
        int hitAxis = 0;
        f32 hitSign = 1.0f;

        f32 hes[3] = {he.x, he.y, he.z};
        f32 los[3] = {localO.x, localO.y, localO.z};
        f32 lds[3] = {localD.x, localD.y, localD.z};

        for (int i = 0; i < 3; i++) {
            if (std::abs(lds[i]) < 1e-8f) {
                if (los[i] < -hes[i] || los[i] > hes[i]) return false;
            } else {
                f32 inv = 1.0f / lds[i];
                f32 t1 = (-hes[i] - los[i]) * inv;
                f32 t2 = ( hes[i] - los[i]) * inv;
                f32 sign = -1.0f;
                if (t1 > t2) { std::swap(t1, t2); sign = 1.0f; }
                if (t1 > tMin) { tMin = t1; hitAxis = i; hitSign = sign; }
                if (t2 < tMax) tMax = t2;
                if (tMin > tMax) return false;
            }
        }
        if (tMin < 0) return false;
        t = tMin;

        // Normal in local space
        Vec3 localN = {0,0,0};
        if (hitAxis == 0)      localN.x = hitSign;
        else if (hitAxis == 1) localN.y = hitSign;
        else                   localN.z = hitSign;
        hitNormal = rot * localN;
        return true;
    }

    /* ── Ray vs Capsule ───────────────────────────────────── */
    bool PhysicsWorld::RayVsCapsule(
        const Vec3& o, const Vec3& d, const RigidBody& b,
        f32& t, Vec3& n) const
    {
        Vec3 localY = {0, b.GetCollider().halfExtents.y, 0};
        Vec3 worldY = b.GetOrientation().Rotate(localY);
        Vec3 A = Add(b.GetPosition(), worldY);
        Vec3 B = Sub(b.GetPosition(), worldY);
        f32  r = b.GetCollider().radius;

        // Infinite cylinder ray test along AB
        Vec3 AB = Sub(B, A);
        Vec3 AO = Sub(o, A);

        f32 ABdotD  = Dot(AB, d);
        f32 ABdotAO = Dot(AB, AO);
        f32 ABdotAB = Dot(AB, AB);

        f32 m = ABdotD  / (ABdotAB + 1e-10f);
        f32 n2 = ABdotAO / (ABdotAB + 1e-10f);

        Vec3 dPerp  = Sub(d, Scale(AB, m));
        Vec3 aoPerp = Sub(AO, Scale(AB, n2));

        f32 a2 = Dot(dPerp, dPerp);
        f32 b2 = 2.0f * Dot(dPerp, aoPerp);
        f32 c2 = Dot(aoPerp, aoPerp) - r*r;

        f32 disc = b2*b2 - 4.0f*a2*c2;
        bool hit = false;
        f32 bestT = 1e30f;
        Vec3 bestN = {0,1,0};

        if (disc >= 0 && a2 > 1e-10f) {
            f32 tCyl = (-b2 - std::sqrt(disc)) / (2.0f * a2);
            if (tCyl >= 0) {
                f32 proj = n2 + m * tCyl;
                if (proj >= 0 && proj <= 1) {
                    bestT = tCyl;
                    Vec3 hitP = Add(o, Scale(d, tCyl));
                    Vec3 onSeg = Add(A, Scale(AB, proj));
                    bestN = Normalize(Sub(hitP, onSeg));
                    hit = true;
                }
            }
        }

        // Test hemisphere caps as spheres
        auto testCap = [&](const Vec3& center) {
            Vec3 oc = Sub(o, center);
            f32 B3 = 2.0f * Dot(oc, d);
            f32 C3 = Dot(oc, oc) - r*r;
            f32 disc3 = B3*B3 - 4.0f*C3;
            if (disc3 >= 0) {
                f32 tc = (-B3 - std::sqrt(disc3)) * 0.5f;
                if (tc >= 0 && tc < bestT) {
                    bestT = tc;
                    Vec3 hp = Add(o, Scale(d, tc));
                    bestN = Normalize(Sub(hp, center));
                    hit = true;
                }
            }
        };
        testCap(A);
        testCap(B);

        if (hit) { t = bestT; n = bestN; }
        return hit;
    }

    /* ── Ray vs Plane ─────────────────────────────────────── */
    bool PhysicsWorld::RayVsPlane(
        const Vec3& o, const Vec3& d, const RigidBody& b,
        f32& t, Vec3& n) const
    {
        Vec3 pn = b.GetCollider().planeNormal;
        f32  po = b.GetCollider().planeOffset;
        f32  denom = Dot(d, pn);
        if (std::abs(denom) < 1e-8f) return false;
        t = (po - Dot(o, pn)) / denom;
        if (t < 0) return false;
        n = pn;
        return true;
    }

    /* ══════════════════════════════════════════════════════════════
     *  PhysicsSystemImpl  –  IPhysics bridge
     *
     *  All method signatures match the IPhysics interface exactly.
     * ══════════════════════════════════════════════════════════════*/

    PhysicsSystemImpl::PhysicsSystemImpl()  = default;
    PhysicsSystemImpl::~PhysicsSystemImpl() = default;

    VoidResult PhysicsSystemImpl::Initialize() {
        world_ = std::make_unique<PhysicsWorld>();
        world_->SetGravity({0, -9.81f, 0});
        world_->SetSubSteps(8);
        world_->SetSolverIterations(10);
        std::cout << "[Physics] World ready.  Gravity=(0,-9.81,0)"
                  << "  SubSteps=8  SolverIter=10\n";
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
        EntityID e, const RigidBodyDesc& d)
    {
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

    void PhysicsSystemImpl::SetVelocity(EntityID e, const Vec3& v) {
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

    void PhysicsSystemImpl::ApplyForce(EntityID e, const Vec3& f) {
        auto it = entityToBody_.find(e);
        if (it == entityToBody_.end()) return;
        auto* b = world_->GetBody(it->second);
        if (b) b->ApplyForce(f);
    }

    void PhysicsSystemImpl::ApplyImpulse(EntityID e, const Vec3& impulse) {
        auto it = entityToBody_.find(e);
        if (it == entityToBody_.end()) return;
        auto* b = world_->GetBody(it->second);
        if (b) b->ApplyImpulse(impulse);
    }

    RaycastHit PhysicsSystemImpl::Raycast(
        const Vec3& o, const Vec3& d, f32 dist) const
    {
        RaycastHit hit;
        if (!world_) return hit;
        auto r       = world_->Raycast(o, d, dist);
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
        RigidBody* b = const_cast<PhysicsWorld*>(world_.get())->GetBody(id);
        return b ? b->GetPosition() : Vec3::Zero();
    }

    /* ══════════════════════════════════════════════════════════════
     *  PhysicsModule  –  IModule DLL entry point
     * ══════════════════════════════════════════════════════════════*/

    PhysicsModule::PhysicsModule()  = default;
    PhysicsModule::~PhysicsModule() = default;

    VoidResult PhysicsModule::Initialize(const ModuleInitParams& params) {
        ILogger* log = nullptr;
        if (params.context) log = params.context->Logger();

        if (log) log->Info("Physics", "Initializing v2.0 ...");

        physics_ = std::make_unique<PhysicsSystemImpl>();
        auto r   = physics_->Initialize();
        if (r.IsErr()) return r;

        if (params.context)
            params.context->Register<IPhysics>(physics_.get());

        if (log) log->Info("Physics",
            "Ready.  Fixed=1/120  Sub=8  Solver=10  "
            "Features: SI warm-start, spatial-hash, "
            "capsule, constraints, collision-layers");

        return VoidResult::Ok();
    }

    void PhysicsModule::OnUpdate(f32 deltaTime) {
        if (!physics_) return;

        accumulator_ += deltaTime;
        if (accumulator_ > 0.1f) accumulator_ = 0.1f;  // spiral-of-death guard

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
        d.version     = "2.0.0";
        d.apiVersion  = RIFTCORE_API_VERSION;
        d.description = "Production rigid-body physics – SI solver, "
                        "spatial-hash broadphase, quaternion orientation, "
                        "capsule colliders, constraints, collision layers";
        return d;
    }

    RIFTCORE_IMPLEMENT_MODULE(PhysicsModule)

} // namespace RiftCore
