#pragma once

// PhysicsTypes.h
// Internal physics types used within the Physics DLL.
// ColliderShape, ColliderDesc, RigidBodyDesc are
// defined in SDK/IPhysics.h — include that instead.

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Physics/IPhysics.h>

#include <vector>
#include <functional>

#ifdef PHYSICS_EXPORTS
    #define PHYSICS_API RIFTCORE_EXPORT
#else
    #define PHYSICS_API RIFTCORE_IMPORT
#endif

namespace RiftCore {

    // ── AABB (Axis-Aligned Bounding Box) ──────────────────────
    struct AABB {
        Vec3 min = Vec3::Zero();
        Vec3 max = Vec3::Zero();

        bool Overlaps(const AABB& other) const {
            return min.x <= other.max.x &&
                   max.x >= other.min.x &&
                   min.y <= other.max.y &&
                   max.y >= other.min.y &&
                   min.z <= other.max.z &&
                   max.z >= other.min.z;
        }

        Vec3 Center() const {
            return {
                (min.x + max.x) * 0.5f,
                (min.y + max.y) * 0.5f,
                (min.z + max.z) * 0.5f
            };
        }

        Vec3 HalfExtents() const {
            return {
                (max.x - min.x) * 0.5f,
                (max.y - min.y) * 0.5f,
                (max.z - min.z) * 0.5f
            };
        }
    };

    // ── Contact point ─────────────────────────────────────────
    struct ContactPoint {
        Vec3  point;
        Vec3  normal;
        f32   penetration = 0.0f;
        u32   bodyA       = 0;
        u32   bodyB       = 0;
    };

    // ── Raycast result ────────────────────────────────────────
    struct RaycastResult {
        bool  hit        = false;
        f32   distance   = 0.0f;
        Vec3  point      = Vec3::Zero();
        Vec3  normal     = Vec3::Up();
        u32   bodyIndex  = 0;
    };

    // ── Physics stats ─────────────────────────────────────────
    struct PhysicsStats {
        u32 bodyCount      = 0;
        u32 activeCount    = 0;
        u32 contactCount   = 0;
        u32 collisionPairs = 0;
        f32 stepTimeMs     = 0.0f;
    };

} // namespace RiftCore
