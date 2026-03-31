#include <Physics/PhysicsWorld.h>
#include <RiftCore/Common/EngineContext.h>
#include <RiftCore/Core/ILogger.h>

#include <algorithm>
#include <iostream>
#include <chrono>
#include <cmath>
#include <cstring>

namespace RiftCore {

    // ── PhysicsWorld ──────────────────────────────────────────
    PhysicsWorld::PhysicsWorld()  = default;
    PhysicsWorld::~PhysicsWorld() = default;

    void PhysicsWorld::SetGravity(const Vec3& gravity) {
        gravity_ = gravity;
    }

    u32 PhysicsWorld::AddBody(const RigidBodyDesc& desc) {
        std::lock_guard<std::mutex> lock(mutex_);
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
        desc.isStatic    = true;
        desc.useGravity  = false;
        desc.position    = {0, y, 0};
        desc.collider.shape       = ColliderShape::Plane;
        desc.collider.planeNormal = {0, 1, 0};
        desc.collider.planeOffset = y;
        desc.collider.restitution = restitution;
        desc.collider.friction    = friction;
        return AddBody(desc);
    }

    void PhysicsWorld::RemoveBody(u32 id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = idToIndex_.find(id);
        if (it == idToIndex_.end()) return;

        u32 index   = it->second;
        u32 lastIdx = static_cast<u32>(bodies_.size()-1);

        if (index != lastIdx) {
            bodies_[index] = std::move(bodies_[lastIdx]);
            idToIndex_[bodies_[index].GetID()] = index;
        }

        bodies_.pop_back();
        idToIndex_.erase(id);
    }

    RigidBody* PhysicsWorld::GetBody(u32 id) {
        auto it = idToIndex_.find(id);
        if (it == idToIndex_.end()) return nullptr;
        return &bodies_[it->second];
    }

    void PhysicsWorld::Step(f32 dt) {
        auto start = std::chrono::high_resolution_clock::now();

        f32 subDt = dt / static_cast<f32>(subSteps_);

        for (u32 step = 0; step < subSteps_; step++) {
            IntegrateBodies(subDt);

            std::vector<std::pair<u32,u32>> pairs;
            BroadPhase(pairs);

            std::vector<ContactPoint> contacts;
            NarrowPhase(pairs, contacts);

            ResolveContacts(contacts);

            stats_.contactCount   = static_cast<u32>(
                contacts.size());
            stats_.collisionPairs = static_cast<u32>(
                pairs.size());
        }

        // Count active bodies
        u32 active = 0;
        for (auto& b : bodies_) {
            if (b.IsAwake() && !b.IsStatic()) active++;
        }
        stats_.bodyCount  = static_cast<u32>(bodies_.size());
        stats_.activeCount= active;

        auto end = std::chrono::high_resolution_clock::now();
        stats_.stepTimeMs = std::chrono::duration<f32,
            std::milli>(end - start).count();

        // Debug: print contact count every 120 frames
        static u32 debugFrame = 0;
        debugFrame++;
        if (debugFrame % 120 == 0 &&
            stats_.contactCount > 0) {
            // Has contacts - collision working
        }
        if (debugFrame % 120 == 0 &&
            stats_.bodyCount > 2 &&
            stats_.contactCount == 0 &&
            stats_.activeCount > 0) {
            std::cout << "[Physics] WARNING: " <<
                stats_.activeCount <<
                " active bodies, 0 contacts detected. "
                "Check collider setup.\n";
        }
    }

    void PhysicsWorld::IntegrateBodies(f32 dt) {
        for (auto& body : bodies_) {
            body.Integrate(dt, gravity_);
        }
    }

    void PhysicsWorld::BroadPhase(
        std::vector<std::pair<u32,u32>>& pairs
    ) {
        u32 count = static_cast<u32>(bodies_.size());
        for (u32 i = 0; i < count; i++) {
            for (u32 j = i+1; j < count; j++) {
                RigidBody& a = bodies_[i];
                RigidBody& b = bodies_[j];

                // Skip static-static pairs
                if (a.IsStatic() && b.IsStatic()) continue;

                // Skip sleeping pairs
                // Skip only if BOTH are sleeping AND neither is static
                // Static bodies can wake sleeping objects
                if (!a.IsAwake() && !b.IsAwake() &&
                    !a.IsStatic() && !b.IsStatic()) continue;

                AABB aabbA = a.GetAABB();
                AABB aabbB = b.GetAABB();

                if (aabbA.Overlaps(aabbB)) {
                    pairs.push_back({i, j});
                }
            }
        }
    }

    void PhysicsWorld::NarrowPhase(
        const std::vector<std::pair<u32,u32>>& pairs,
        std::vector<ContactPoint>&              contacts
    ) {
        for (auto& [iA, iB] : pairs) {
            auto& bodyA = bodies_[iA];
            auto& bodyB = bodies_[iB];

            ContactPoint contact;
            contact.bodyA = iA;
            contact.bodyB = iB;

            ColliderShape shapeA = bodyA.GetCollider().shape;
            ColliderShape shapeB = bodyB.GetCollider().shape;

            bool hit = false;

            // Sphere vs Sphere
            if (shapeA == ColliderShape::Sphere &&
                shapeB == ColliderShape::Sphere) {
                hit = TestSphereSphere(bodyA, bodyB, contact);
            }
            // Sphere vs Box
            else if (shapeA == ColliderShape::Sphere &&
                     shapeB == ColliderShape::Box) {
                hit = TestSphereBox(bodyA, bodyB, contact);
            }
            // Box vs Sphere
            else if (shapeA == ColliderShape::Box &&
                     shapeB == ColliderShape::Sphere) {
                hit = TestSphereBox(bodyB, bodyA, contact);
                if (hit) {
                    // Flip normal
                    contact.normal.x = -contact.normal.x;
                    contact.normal.y = -contact.normal.y;
                    contact.normal.z = -contact.normal.z;
                    std::swap(contact.bodyA, contact.bodyB);
                }
            }
            // Box vs Box
            else if (shapeA == ColliderShape::Box &&
                     shapeB == ColliderShape::Box) {
                hit = TestBoxBox(bodyA, bodyB, contact);
            }
            // Sphere vs Plane
            else if (shapeA == ColliderShape::Sphere &&
                     shapeB == ColliderShape::Plane) {
                hit = TestSpherePlane(bodyA, bodyB, contact);
            }
            // Plane vs Sphere - swap so sphere=first
            else if (shapeA == ColliderShape::Plane &&
                     shapeB == ColliderShape::Sphere) {
                hit = TestSpherePlane(bodyB, bodyA, contact);
                if (hit) {
                    // Flip because we swapped bodies
                    contact.normal.x = -contact.normal.x;
                    contact.normal.y = -contact.normal.y;
                    contact.normal.z = -contact.normal.z;
                    std::swap(contact.bodyA, contact.bodyB);
                }
            }
            // Box vs Plane
            else if (shapeA == ColliderShape::Box &&
                     shapeB == ColliderShape::Plane) {
                hit = TestBoxPlane(bodyA, bodyB, contact);
            }
            // Plane vs Box - swap so box=first
            else if (shapeA == ColliderShape::Plane &&
                     shapeB == ColliderShape::Box) {
                hit = TestBoxPlane(bodyB, bodyA, contact);
                if (hit) {
                    contact.normal.x = -contact.normal.x;
                    contact.normal.y = -contact.normal.y;
                    contact.normal.z = -contact.normal.z;
                    std::swap(contact.bodyA, contact.bodyB);
                }
            }

            if (hit) {
                contacts.push_back(contact);
                if (collisionCallback_) {
                    collisionCallback_(
                        bodyA.GetID(),
                        bodyB.GetID(),
                        contact
                    );
                }
                // Wake bodies on collision
                bodies_[iA].Wake();
                bodies_[iB].Wake();
            }
        }
    }

    bool PhysicsWorld::TestSphereSphere(
        RigidBody& a, RigidBody& b,
        ContactPoint& contact
    ) {
        Vec3 posA = a.GetPosition();
        Vec3 posB = b.GetPosition();
        f32  rA   = a.GetCollider().radius;
        f32  rB   = b.GetCollider().radius;

        Vec3 diff = {
            posA.x - posB.x,
            posA.y - posB.y,
            posA.z - posB.z
        };
        f32 distSq = diff.x*diff.x +
                     diff.y*diff.y +
                     diff.z*diff.z;
        f32 sumR = rA + rB;

        if (distSq >= sumR * sumR) return false;

        f32  dist    = std::sqrt(distSq);
        Vec3 normal  = dist > 0.0001f
            ? Vec3{diff.x/dist, diff.y/dist, diff.z/dist}
            : Vec3{0,1,0};

        contact.normal      = normal;
        contact.penetration = sumR - dist;
        contact.point = {
            posB.x + normal.x * rB,
            posB.y + normal.y * rB,
            posB.z + normal.z * rB
        };
        return true;
    }

    bool PhysicsWorld::TestSphereBox(
        RigidBody& sphere, RigidBody& box,
        ContactPoint& contact
    ) {
        Vec3 spherePos = sphere.GetPosition();
        Vec3 boxPos    = box.GetPosition();
        f32  r         = sphere.GetCollider().radius;
        Vec3 halfExt   = box.GetCollider().halfExtents;

        // Find closest point on box to sphere center
        Vec3 local = {
            spherePos.x - boxPos.x,
            spherePos.y - boxPos.y,
            spherePos.z - boxPos.z
        };

        // Clamp to box
        Vec3 closest = {
            std::max(-halfExt.x, std::min(local.x, halfExt.x)),
            std::max(-halfExt.y, std::min(local.y, halfExt.y)),
            std::max(-halfExt.z, std::min(local.z, halfExt.z))
        };

        Vec3 diff = {
            local.x - closest.x,
            local.y - closest.y,
            local.z - closest.z
        };

        f32 distSq = diff.x*diff.x +
                     diff.y*diff.y +
                     diff.z*diff.z;

        if (distSq >= r * r) return false;

        f32 dist = std::sqrt(distSq);
        Vec3 normal = dist > 0.0001f
            ? Vec3{diff.x/dist, diff.y/dist, diff.z/dist}
            : Vec3{0,1,0};

        contact.normal      = normal;
        contact.penetration = r - dist;
        contact.point = {
            boxPos.x + closest.x,
            boxPos.y + closest.y,
            boxPos.z + closest.z
        };
        return true;
    }

    bool PhysicsWorld::TestBoxBox(
        RigidBody& a, RigidBody& b,
        ContactPoint& contact
    ) {
        Vec3 posA  = a.GetPosition();
        Vec3 posB  = b.GetPosition();
        Vec3 heA   = a.GetCollider().halfExtents;
        Vec3 heB   = b.GetCollider().halfExtents;

        // AABB overlap test on each axis
        f32 dx = posA.x - posB.x;
        f32 dy = posA.y - posB.y;
        f32 dz = posA.z - posB.z;

        f32 overlapX = (heA.x + heB.x) - std::abs(dx);
        f32 overlapY = (heA.y + heB.y) - std::abs(dy);
        f32 overlapZ = (heA.z + heB.z) - std::abs(dz);

        if (overlapX <= 0 || overlapY <= 0 ||
            overlapZ <= 0) return false;

        // Minimum penetration axis
        if (overlapX < overlapY && overlapX < overlapZ) {
            contact.normal = {dx < 0 ? -1.0f : 1.0f, 0, 0};
            contact.penetration = overlapX;
        } else if (overlapY < overlapZ) {
            contact.normal = {0, dy < 0 ? -1.0f : 1.0f, 0};
            contact.penetration = overlapY;
        } else {
            contact.normal = {0, 0, dz < 0 ? -1.0f : 1.0f};
            contact.penetration = overlapZ;
        }

        contact.point = {
            (posA.x + posB.x) * 0.5f,
            (posA.y + posB.y) * 0.5f,
            (posA.z + posB.z) * 0.5f
        };
        return true;
    }

    bool PhysicsWorld::TestSpherePlane(
        RigidBody& sphere, RigidBody& plane,
        ContactPoint& contact
    ) {
        Vec3 pos    = sphere.GetPosition();
        f32  r      = sphere.GetCollider().radius;
        Vec3 normal = plane.GetCollider().planeNormal;
        f32  offset = plane.GetCollider().planeOffset;

        // Signed distance: positive = above plane
        // normal = (0,1,0), offset = planeY
        // dist = dot(pos, normal) - offset
        //      = pos.y - planeY
        f32 dist = Dot(pos, normal) - offset;

        // Collision only if sphere is touching or below plane
        if (dist >= r) return false;

        // Contact normal points from plane toward sphere
        contact.normal      = normal;
        contact.penetration = r - dist;

        // Contact point on plane surface
        contact.point = {
            pos.x - normal.x * r,
            pos.y - normal.y * r,
            pos.z - normal.z * r
        };
        return true;
    }

    bool PhysicsWorld::TestBoxPlane(
        RigidBody& box, RigidBody& plane,
        ContactPoint& contact
    ) {
        Vec3 pos    = box.GetPosition();
        Vec3 he     = box.GetCollider().halfExtents;
        Vec3 normal = plane.GetCollider().planeNormal;
        f32  offset = plane.GetCollider().planeOffset;

        // For axis-aligned box: project half extents onto normal
        // normal=(0,1,0): proj = he.y
        f32 proj = std::abs(he.x * normal.x) +
                   std::abs(he.y * normal.y) +
                   std::abs(he.z * normal.z);

        // dist = signed distance from box center to plane
        // positive = above plane
        f32 dist = Dot(pos, normal) - offset;

        // Bottom of box is at dist - proj from plane
        // Collision if bottom is below plane surface
        if (dist >= proj) return false;

        contact.normal      = normal;
        contact.penetration = proj - dist;

        // Contact point at bottom of box
        contact.point = {
            pos.x - normal.x * proj,
            pos.y - normal.y * proj,
            pos.z - normal.z * proj
        };
        return true;
    }

    void PhysicsWorld::ResolveContacts(
        std::vector<ContactPoint>& contacts
    ) {
        for (auto& contact : contacts) {
            ResolveContact(contact);
            PositionalCorrect(contact);
        }
    }

    void PhysicsWorld::ResolveContact(ContactPoint& contact) {
        RigidBody& bodyA = *(&bodies_[contact.bodyA]);
        RigidBody& bodyB = *(&bodies_[contact.bodyB]);

        // Relative velocity along normal
        Vec3 velA = bodyA.GetVelocity();
        Vec3 velB = bodyB.GetVelocity();

        Vec3 relVel = {
            velA.x - velB.x,
            velA.y - velB.y,
            velA.z - velB.z
        };

        f32 velAlongNormal = Dot(relVel, contact.normal);

        // Only resolve if moving toward each other
        if (velAlongNormal > 0) return;

        // Restitution (bounciness)
        f32 e = std::min(
            bodyA.GetCollider().restitution,
            bodyB.GetCollider().restitution
        );

        // Impulse scalar
        f32 invMassA = bodyA.GetInvMass();
        f32 invMassB = bodyB.GetInvMass();
        f32 invMassSum = invMassA + invMassB;

        if (invMassSum < 0.0001f) return;

        f32 j = -(1.0f + e) * velAlongNormal / invMassSum;

        Vec3 impulse = {
            contact.normal.x * j,
            contact.normal.y * j,
            contact.normal.z * j
        };

        // Apply impulse
        bodyA.ApplyImpulse(impulse);
        Vec3 negImpulse = {
            -impulse.x, -impulse.y, -impulse.z};
        bodyB.ApplyImpulse(negImpulse);

        // Friction impulse
        Vec3 relVelNew = {
            bodyA.GetVelocity().x - bodyB.GetVelocity().x,
            bodyA.GetVelocity().y - bodyB.GetVelocity().y,
            bodyA.GetVelocity().z - bodyB.GetVelocity().z
        };

        Vec3 tangent = {
            relVelNew.x - contact.normal.x *
                Dot(relVelNew, contact.normal),
            relVelNew.y - contact.normal.y *
                Dot(relVelNew, contact.normal),
            relVelNew.z - contact.normal.z *
                Dot(relVelNew, contact.normal)
        };

        f32 tangentLen = Length(tangent);
        if (tangentLen > 0.0001f) {
            tangent.x /= tangentLen;
            tangent.y /= tangentLen;
            tangent.z /= tangentLen;
        } else {
            return;
        }

        f32 jt = -Dot(relVelNew, tangent) / invMassSum;
        f32 mu = (bodyA.GetCollider().friction +
                  bodyB.GetCollider().friction) * 0.5f;

        Vec3 frictionImpulse;
        if (std::abs(jt) < j * mu) {
            frictionImpulse = {
                tangent.x * jt,
                tangent.y * jt,
                tangent.z * jt
            };
        } else {
            frictionImpulse = {
                tangent.x * -j * mu,
                tangent.y * -j * mu,
                tangent.z * -j * mu
            };
        }

        bodyA.ApplyImpulse(frictionImpulse);
        Vec3 negFriction = {
            -frictionImpulse.x,
            -frictionImpulse.y,
            -frictionImpulse.z
        };
        bodyB.ApplyImpulse(negFriction);
    }

    void PhysicsWorld::PositionalCorrect(
        ContactPoint& contact
    ) {
        // Baumgarte stabilization — prevents sinking
        const f32 percent  = 0.6f;  // correction factor
        const f32 slop     = 0.005f; // penetration tolerance

        RigidBody& bodyA = *(&bodies_[contact.bodyA]);
        RigidBody& bodyB = *(&bodies_[contact.bodyB]);

        f32 invMassA   = bodyA.GetInvMass();
        f32 invMassB   = bodyB.GetInvMass();
        f32 invMassSum = invMassA + invMassB;

        if (invMassSum < 0.0001f) return;

        f32 corrMag = std::max(
            contact.penetration - slop, 0.0f)
            / invMassSum * percent;

        Vec3 correction = {
            contact.normal.x * corrMag,
            contact.normal.y * corrMag,
            contact.normal.z * corrMag
        };

        // Move bodies apart proportionally to inverse mass
        Vec3 posA = bodyA.GetPosition();
        posA.x += correction.x * invMassA;
        posA.y += correction.y * invMassA;
        posA.z += correction.z * invMassA;
        bodyA.SetPosition(posA);

        Vec3 posB = bodyB.GetPosition();
        posB.x -= correction.x * invMassB;
        posB.y -= correction.y * invMassB;
        posB.z -= correction.z * invMassB;
        bodyB.SetPosition(posB);
    }

    RaycastResult PhysicsWorld::Raycast(
        const Vec3& origin,
        const Vec3& direction,
        f32         maxDistance
    ) const {
        RaycastResult best;
        best.distance = maxDistance;

        Vec3 dir = Normalize(direction);

        for (u32 i = 0;
             i < static_cast<u32>(bodies_.size()); i++) {
            const RigidBody& body = bodies_[i];
            if (body.GetCollider().shape ==
                ColliderShape::Plane) continue;

            if (body.GetCollider().shape ==
                ColliderShape::Sphere) {
                Vec3 pos = body.GetPosition();
                f32  r   = body.GetCollider().radius;

                Vec3 oc = {
                    origin.x - pos.x,
                    origin.y - pos.y,
                    origin.z - pos.z
                };

                f32 a = Dot(dir, dir);
                f32 b = 2.0f * Dot(oc, dir);
                f32 c = Dot(oc, oc) - r * r;
                f32 disc = b*b - 4*a*c;

                if (disc >= 0) {
                    f32 t = (-b - std::sqrt(disc)) / (2*a);
                    if (t > 0 && t < best.distance) {
                        best.hit      = true;
                        best.distance = t;
                        best.bodyIndex = i;
                        best.point = {
                            origin.x + dir.x * t,
                            origin.y + dir.y * t,
                            origin.z + dir.z * t
                        };
                        best.normal = Normalize({
                            best.point.x - pos.x,
                            best.point.y - pos.y,
                            best.point.z - pos.z
                        });
                    }
                }
            }
            else if (body.GetCollider().shape ==
                     ColliderShape::Box) {
                Vec3 pos = body.GetPosition();
                Vec3 he  = body.GetCollider().halfExtents;

                Vec3 tMin = {
                    (pos.x-he.x - origin.x) /
                        (dir.x == 0 ? 1e-8f : dir.x),
                    (pos.y-he.y - origin.y) /
                        (dir.y == 0 ? 1e-8f : dir.y),
                    (pos.z-he.z - origin.z) /
                        (dir.z == 0 ? 1e-8f : dir.z)
                };
                Vec3 tMax = {
                    (pos.x+he.x - origin.x) /
                        (dir.x == 0 ? 1e-8f : dir.x),
                    (pos.y+he.y - origin.y) /
                        (dir.y == 0 ? 1e-8f : dir.y),
                    (pos.z+he.z - origin.z) /
                        (dir.z == 0 ? 1e-8f : dir.z)
                };

                if (tMin.x > tMax.x) std::swap(tMin.x, tMax.x);
                if (tMin.y > tMax.y) std::swap(tMin.y, tMax.y);
                if (tMin.z > tMax.z) std::swap(tMin.z, tMax.z);

                f32 tEnter = std::max({tMin.x,tMin.y,tMin.z});
                f32 tExit  = std::min({tMax.x,tMax.y,tMax.z});

                if (tEnter < tExit && tEnter > 0 &&
                    tEnter < best.distance) {
                    best.hit       = true;
                    best.distance  = tEnter;
                    best.bodyIndex = i;
                    best.point = {
                        origin.x + dir.x * tEnter,
                        origin.y + dir.y * tEnter,
                        origin.z + dir.z * tEnter
                    };
                    best.normal = {0, 1, 0};
                }
            }
        }
        return best;
    }

    // ── PhysicsSystemImpl ─────────────────────────────────────
    PhysicsSystemImpl::PhysicsSystemImpl()  = default;
    PhysicsSystemImpl::~PhysicsSystemImpl() = default;

    VoidResult PhysicsSystemImpl::Initialize() {
        world_ = std::make_unique<PhysicsWorld>();
        world_->SetGravity({0, -9.81f, 0});
        std::cout << "[Physics] World initialized. "
                  << "Gravity: (0, -9.81, 0)\n";
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
        EntityID e, const RigidBodyDesc& desc
    ) {
        if (!world_) return;
        u32 bodyId = world_->AddBody(desc);
        entityToBody_[e] = bodyId;
        bodyToEntity_[bodyId] = e;
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
        auto* body = world_->GetBody(it->second);
        if (body) body->SetVelocity(v);
    }

    Vec3 PhysicsSystemImpl::GetVelocity(EntityID e) const {
        auto it = entityToBody_.find(e);
        if (it == entityToBody_.end()) return Vec3::Zero();
        auto* body = world_->GetBody(it->second);
        return body ? body->GetVelocity() : Vec3::Zero();
    }

    void PhysicsSystemImpl::ApplyForce(
        EntityID e, const Vec3& f
    ) {
        auto it = entityToBody_.find(e);
        if (it == entityToBody_.end()) return;
        auto* body = world_->GetBody(it->second);
        if (body) body->ApplyForce(f);
    }

    void PhysicsSystemImpl::ApplyImpulse(
        EntityID e, const Vec3& impulse
    ) {
        auto it = entityToBody_.find(e);
        if (it == entityToBody_.end()) return;
        auto* body = world_->GetBody(it->second);
        if (body) body->ApplyImpulse(impulse);
    }

    RaycastHit PhysicsSystemImpl::Raycast(
        const Vec3& origin,
        const Vec3& direction,
        f32         maxDist
    ) const {
        RaycastHit hit;
        if (!world_) return hit;
        auto result = world_->Raycast(
            origin, direction, maxDist);
        hit.hit      = result.hit;
        hit.distance = result.distance;
        hit.point    = result.point;
        hit.normal   = result.normal;
        return hit;
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
        auto* it = const_cast<PhysicsWorld*>(
            world_.get())->GetBody(id);
        return it ? it->GetPosition() : Vec3::Zero();
    }

    // ── PhysicsModule ─────────────────────────────────────────
    PhysicsModule::PhysicsModule()  = default;
    PhysicsModule::~PhysicsModule() = default;

    VoidResult PhysicsModule::Initialize(
        const ModuleInitParams& params
    ) {
        ILogger* logger = nullptr;
        if (params.context) {
            logger = params.context->Logger();
        }

        if (logger) logger->Info("Physics", "Initializing...");

        physics_ = std::make_unique<PhysicsSystemImpl>();
        auto r   = physics_->Initialize();
        if (r.IsErr()) return r;

        if (params.context) {
            params.context->Register<IPhysics>(
                physics_.get());
        }

        if (logger) {
            logger->Info("Physics",
                "Physics world ready. "
                "Fixed step: 1/60s  SubSteps: 4");
        }

        return VoidResult::Ok();
    }

    void PhysicsModule::OnUpdate(f32 deltaTime) {
        if (!physics_) return;

        // Fixed timestep accumulator
        accumulator_ += deltaTime;

        // Clamp to prevent spiral of death
        if (accumulator_ > 0.2f) accumulator_ = 0.2f;

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
        ModuleDescriptor desc;
        desc.name        = "Physics";
        desc.version     = "0.1.0";
        desc.apiVersion  = RIFTCORE_API_VERSION;
        desc.description = "Rigid body physics simulation";
        return desc;
    }

    RIFTCORE_IMPLEMENT_MODULE(PhysicsModule)

} // namespace RiftCore











