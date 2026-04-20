/*──────────────────────────────────────────────────────────────────────────
 *  PhysicsTypes.h  –  Internal mathematics & data structures for the
 *                      RiftCore Physics plug-in DLL.
 *
 *  Everything that lives here is an *implementation detail*.
 *  Public / SDK types (ColliderShape, ColliderDesc, RigidBodyDesc,
 *  RaycastHit, IPhysics) come from  <RiftCore/Physics/IPhysics.h>.
 *
 *  Major features compared with the previous revision:
 *    • Full 3×3 matrix type   – for world-space inertia tensors.
 *    • Quaternion type        – proper 3-D orientation (no Euler lock).
 *    • Physics material       – static/dynamic friction + combine modes.
 *    • Contact manifold       – up to 4 cached contacts, warm-starting.
 *    • Spatial-hash broadphase – O(n) expected, replaces O(n²) brute.
 *    • Constraint descriptors – distance, hinge, ball-socket, spring, …
 *    • Collision-layer masks  – per-body filtering.
 *    • Utility math namespace – Dot / Cross / inertia helpers.
 *──────────────────────────────────────────────────────────────────────────*/
#pragma once

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Physics/IPhysics.h>

#include <vector>
#include <functional>
#include <cmath>
#include <algorithm>
#include <unordered_map>

#ifdef PHYSICS_EXPORTS
    #define PHYSICS_API RIFTCORE_EXPORT
#else
    #define PHYSICS_API RIFTCORE_IMPORT
#endif

namespace RiftCore {

/* ═══════════════════════════════════════════════════════════════════════
 *  Mat3 – Row-major 3×3 matrix
 *
 *  Used for:
 *    1. Local-space and world-space inertia tensors.
 *    2. Converting a Quat into a rotation matrix so we can
 *       transform half-extents when computing oriented AABBs.
 * ═══════════════════════════════════════════════════════════════════════*/
struct Mat3 {
    f32 m[3][3] = {};

    /* ---------- factory helpers ------------------------------------ */

    /// All-zero matrix.
    static Mat3 Zero() { Mat3 r; return r; }

    /// Identity matrix (ones on the diagonal).
    static Mat3 Identity() {
        Mat3 r;
        r.m[0][0] = r.m[1][1] = r.m[2][2] = 1.0f;
        return r;
    }

    /// Diagonal matrix – useful for inertia tensors of symmetric shapes.
    static Mat3 Diagonal(f32 a, f32 b, f32 c) {
        Mat3 r;
        r.m[0][0] = a;  r.m[1][1] = b;  r.m[2][2] = c;
        return r;
    }

    /* ---------- arithmetic --------------------------------------- */

    /// Matrix × matrix product (standard 3×3).
    Mat3 operator*(const Mat3& o) const {
        Mat3 r;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++) {
                r.m[i][j] = 0;
                for (int k = 0; k < 3; k++)
                    r.m[i][j] += m[i][k] * o.m[k][j];
            }
        return r;
    }

    /// Matrix × column-vector product.
    Vec3 operator*(const Vec3& v) const {
        return {
            m[0][0]*v.x + m[0][1]*v.y + m[0][2]*v.z,
            m[1][0]*v.x + m[1][1]*v.y + m[1][2]*v.z,
            m[2][0]*v.x + m[2][1]*v.y + m[2][2]*v.z
        };
    }

    /// Transpose (swap rows ↔ columns).  For rotation matrices, the
    /// transpose equals the inverse, which is very handy.
    Mat3 Transposed() const {
        Mat3 r;
        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 3; j++)
                r.m[i][j] = m[j][i];
        return r;
    }

    /// Full analytic inverse using the cofactor / determinant formula.
    /// Falls back to Identity if the matrix is singular (det ≈ 0).
    Mat3 Inverse() const {
        f32 det =
            m[0][0]*(m[1][1]*m[2][2] - m[1][2]*m[2][1]) -
            m[0][1]*(m[1][0]*m[2][2] - m[1][2]*m[2][0]) +
            m[0][2]*(m[1][0]*m[2][1] - m[1][1]*m[2][0]);
        if (std::abs(det) < 1e-12f) return Identity();
        f32 inv = 1.0f / det;
        Mat3 r;
        r.m[0][0] = (m[1][1]*m[2][2] - m[1][2]*m[2][1]) * inv;
        r.m[0][1] = (m[0][2]*m[2][1] - m[0][1]*m[2][2]) * inv;
        r.m[0][2] = (m[0][1]*m[1][2] - m[0][2]*m[1][1]) * inv;
        r.m[1][0] = (m[1][2]*m[2][0] - m[1][0]*m[2][2]) * inv;
        r.m[1][1] = (m[0][0]*m[2][2] - m[0][2]*m[2][0]) * inv;
        r.m[1][2] = (m[0][2]*m[1][0] - m[0][0]*m[1][2]) * inv;
        r.m[2][0] = (m[1][0]*m[2][1] - m[1][1]*m[2][0]) * inv;
        r.m[2][1] = (m[0][1]*m[2][0] - m[0][0]*m[2][1]) * inv;
        r.m[2][2] = (m[0][0]*m[1][1] - m[0][1]*m[1][0]) * inv;
        return r;
    }
};

/* ═══════════════════════════════════════════════════════════════════════
 *  Quat – Unit quaternion (w, x, y, z)
 *
 *  Quaternions avoid gimbal lock and interpolation artefacts inherent
 *  in Euler angles.  Every RigidBody stores its orientation as a Quat
 *  and integrates angular velocity directly into it each sub-step.
 * ═══════════════════════════════════════════════════════════════════════*/
struct Quat {
    f32 w = 1, x = 0, y = 0, z = 0;

    static Quat Identity() { return {1, 0, 0, 0}; }

    f32 LengthSq() const { return w*w + x*x + y*y + z*z; }

    /// Re-normalise to unit length (drift accumulates over time).
    Quat Normalized() const {
        f32 len = std::sqrt(LengthSq());
        if (len < 1e-8f) return Identity();
        f32 inv = 1.0f / len;
        return {w*inv, x*inv, y*inv, z*inv};
    }

    Quat Conjugate() const { return {w, -x, -y, -z}; }

    /// Hamilton product – combines two rotations.
    Quat operator*(const Quat& q) const {
        return {
            w*q.w - x*q.x - y*q.y - z*q.z,
            w*q.x + x*q.w + y*q.z - z*q.y,
            w*q.y - x*q.z + y*q.w + z*q.x,
            w*q.z + x*q.y - y*q.x + z*q.w
        };
    }

    /// Rotate vector v by this quaternion:  q·v·q⁻¹
    /// Uses the optimised formula (avoids building a full matrix).
    Vec3 Rotate(const Vec3& v) const {
        Vec3 u = {x, y, z};
        f32 s  = w;
        f32 dotUV = u.x*v.x + u.y*v.y + u.z*v.z;
        f32 dotUU = u.x*u.x + u.y*u.y + u.z*u.z;
        Vec3 cross = {
            u.y*v.z - u.z*v.y,
            u.z*v.x - u.x*v.z,
            u.x*v.y - u.y*v.x
        };
        return {
            2.0f*dotUV*u.x + (s*s - dotUU)*v.x + 2.0f*s*cross.x,
            2.0f*dotUV*u.y + (s*s - dotUU)*v.y + 2.0f*s*cross.y,
            2.0f*dotUV*u.z + (s*s - dotUU)*v.z + 2.0f*s*cross.z
        };
    }

    /// Build the equivalent 3×3 rotation matrix.
    Mat3 ToMat3() const {
        Mat3 r;
        f32 xx = x*x, yy = y*y, zz = z*z;
        f32 xy = x*y, xz = x*z, yz = y*z;
        f32 wx = w*x, wy = w*y, wz = w*z;
        r.m[0][0] = 1 - 2*(yy+zz);  r.m[0][1] = 2*(xy-wz);      r.m[0][2] = 2*(xz+wy);
        r.m[1][0] = 2*(xy+wz);      r.m[1][1] = 1 - 2*(xx+zz);  r.m[1][2] = 2*(yz-wx);
        r.m[2][0] = 2*(xz-wy);      r.m[2][1] = 2*(yz+wx);      r.m[2][2] = 1 - 2*(xx+yy);
        return r;
    }

    /// Construct from an axis (must be unit-length) and an angle (rad).
    static Quat FromAxisAngle(const Vec3& axis, f32 angle) {
        f32 half = angle * 0.5f;
        f32 s = std::sin(half);
        return {std::cos(half), axis.x*s, axis.y*s, axis.z*s};
    }

    /// First-order integration:  q += ½·(0,ω)·q · dt
    /// Then re-normalise.  Called every sub-step.
    void IntegrateAngularVelocity(const Vec3& omega, f32 dt) {
        Quat dq = {0, omega.x * 0.5f, omega.y * 0.5f, omega.z * 0.5f};
        Quat spin = {
            dq.w*w - dq.x*x - dq.y*y - dq.z*z,
            dq.w*x + dq.x*w + dq.y*z - dq.z*y,
            dq.w*y - dq.x*z + dq.y*w + dq.z*x,
            dq.w*z + dq.x*y - dq.y*x + dq.z*w
        };
        w += spin.w * dt;
        x += spin.x * dt;
        y += spin.y * dt;
        z += spin.z * dt;
        *this = this->Normalized();
    }
};

/* ═══════════════════════════════════════════════════════════════════════
 *  Collision-layer bitmasks
 *
 *  Each body carries a `layer` and a `mask`.  Two bodies can collide
 *  only when  (a.layer & b.mask) && (b.layer & a.mask).
 * ═══════════════════════════════════════════════════════════════════════*/
using CollisionLayer = u32;
using CollisionMask  = u32;

namespace CollisionLayers {
    constexpr u32 Default    = (1 << 0);
    constexpr u32 Static     = (1 << 1);
    constexpr u32 Dynamic    = (1 << 2);
    constexpr u32 Kinematic  = (1 << 3);
    constexpr u32 Trigger    = (1 << 4);
    constexpr u32 Projectile = (1 << 5);
    constexpr u32 Character  = (1 << 6);
    constexpr u32 Debris     = (1 << 7);
    constexpr u32 All        = 0xFFFFFFFF;
}

/* ═══════════════════════════════════════════════════════════════════════
 *  PhysicsMaterial – surface properties
 *
 *  Stored per-body.  When two bodies collide the engine *combines*
 *  their friction / restitution values using the chosen CombineMode.
 * ═══════════════════════════════════════════════════════════════════════*/
struct PhysicsMaterial {
    f32 restitution     = 0.3f;
    f32 staticFriction  = 0.6f;
    f32 dynamicFriction = 0.4f;
    f32 density         = 1.0f;

    enum class CombineMode : u8 {
        Average, Minimum, Maximum, Multiply
    };
    CombineMode frictionCombine    = CombineMode::Average;
    CombineMode restitutionCombine = CombineMode::Average;

    /// Combine two material values according to a mode.
    static f32 Combine(f32 a, f32 b, CombineMode mode) {
        switch (mode) {
            case CombineMode::Average:  return (a + b) * 0.5f;
            case CombineMode::Minimum:  return std::min(a, b);
            case CombineMode::Maximum:  return std::max(a, b);
            case CombineMode::Multiply: return a * b;
            default: return (a + b) * 0.5f;
        }
    }
};

/* ═══════════════════════════════════════════════════════════════════════
 *  AABB – Axis-Aligned Bounding Box
 *
 *  Used by the broad-phase (spatial hash) and by raycasts.
 *  Now includes surface-area, merge, fatten, contain, and
 *  slab-based ray intersection.
 * ═══════════════════════════════════════════════════════════════════════*/
struct AABB {
    Vec3 min = Vec3::Zero();
    Vec3 max = Vec3::Zero();

    /// Do two AABBs overlap on all three axes?
    bool Overlaps(const AABB& other) const {
        return min.x <= other.max.x && max.x >= other.min.x &&
               min.y <= other.max.y && max.y >= other.min.y &&
               min.z <= other.max.z && max.z >= other.min.z;
    }

    Vec3 Center() const {
        return {(min.x+max.x)*0.5f, (min.y+max.y)*0.5f, (min.z+max.z)*0.5f};
    }

    Vec3 HalfExtents() const {
        return {(max.x-min.x)*0.5f, (max.y-min.y)*0.5f, (max.z-min.z)*0.5f};
    }

    /// Surface area – used for SAH cost heuristics in future BVH.
    f32 SurfaceArea() const {
        Vec3 d = {max.x-min.x, max.y-min.y, max.z-min.z};
        return 2.0f * (d.x*d.y + d.y*d.z + d.z*d.x);
    }

    /// Smallest AABB enclosing both `this` and `other`.
    AABB Merged(const AABB& other) const {
        return {
            {std::min(min.x, other.min.x),
             std::min(min.y, other.min.y),
             std::min(min.z, other.min.z)},
            {std::max(max.x, other.max.x),
             std::max(max.y, other.max.y),
             std::max(max.z, other.max.z)}
        };
    }

    /// Expand by a uniform margin on every side (useful for sleeping tolerance).
    AABB Fattened(f32 margin) const {
        return {
            {min.x-margin, min.y-margin, min.z-margin},
            {max.x+margin, max.y+margin, max.z+margin}
        };
    }

    bool Contains(const Vec3& p) const {
        return p.x >= min.x && p.x <= max.x &&
               p.y >= min.y && p.y <= max.y &&
               p.z >= min.z && p.z <= max.z;
    }

    /// Kay/Kajiya slab-method ray intersection.
    /// `invDir` = 1/direction (pre-computed by the caller).
    bool RayIntersect(const Vec3& origin, const Vec3& invDir,
                      f32 maxDist, f32& tMin) const {
        f32 t1 = (min.x - origin.x) * invDir.x;
        f32 t2 = (max.x - origin.x) * invDir.x;
        f32 t3 = (min.y - origin.y) * invDir.y;
        f32 t4 = (max.y - origin.y) * invDir.y;
        f32 t5 = (min.z - origin.z) * invDir.z;
        f32 t6 = (max.z - origin.z) * invDir.z;

        f32 tNear = std::max({std::min(t1,t2), std::min(t3,t4), std::min(t5,t6)});
        f32 tFar  = std::min({std::max(t1,t2), std::max(t3,t4), std::max(t5,t6)});

        if (tNear > tFar || tFar < 0 || tNear > maxDist) return false;
        tMin = tNear > 0 ? tNear : tFar;
        return true;
    }
};

/* ═══════════════════════════════════════════════════════════════════════
 *  ContactPoint / ContactManifold
 *
 *  The solver works on *manifolds* (up to 4 contact points shared by
 *  a body-pair).  Each contact caches its accumulated impulse so the
 *  next frame can "warm-start" and converge faster.
 * ═══════════════════════════════════════════════════════════════════════*/
struct ContactPoint {
    Vec3  point;                         ///< World-space contact location.
    Vec3  normal;                        ///< From body-A towards body-B.
    f32   penetration          = 0.0f;   ///< Overlap depth (positive = colliding).
    u32   bodyA                = 0;      ///< Index into bodies_ array.
    u32   bodyB                = 0;

    /* ── warm-starting accumulators (Sequential-Impulse solver) ─── */
    f32   normalImpulseAccum   = 0.0f;
    f32   tangentImpulseAccum  = 0.0f;
    f32   bitangentImpulseAccum= 0.0f;

    /* ── pre-computed solver constants (filled once per step) ───── */
    Vec3  rA, rB;                        ///< Contact offset from centre of mass.
    f32   normalMass            = 0.0f;  ///< Effective mass along normal.
    f32   tangentMass           = 0.0f;
    f32   bitangentMass         = 0.0f;
    f32   bias                  = 0.0f;  ///< Baumgarte position-bias.
    Vec3  tangent;                       ///< Friction direction 1.
    Vec3  bitangent;                     ///< Friction direction 2.
    f32   restitution           = 0.0f;  ///< Combined restitution for this pair.
    f32   friction              = 0.0f;  ///< Combined friction for this pair.
};

constexpr u32 MAX_MANIFOLD_POINTS = 4;

struct ContactManifold {
    u32          bodyA       = 0;
    u32          bodyB       = 0;
    u32          numContacts = 0;
    ContactPoint contacts[MAX_MANIFOLD_POINTS];
    bool         isTrigger   = false;

    /// Unique key used to look up the previous frame's manifold for
    /// warm-starting.  Combines both body indices into a single u64.
    u64 Key() const {
        return (static_cast<u64>(std::min(bodyA,bodyB)) << 32) |
                static_cast<u64>(std::max(bodyA,bodyB));
    }
};

/* ═══════════════════════════════════════════════════════════════════════
 *  RaycastResult  (internal, richer than the SDK RaycastHit)
 * ═══════════════════════════════════════════════════════════════════════*/
struct RaycastResult {
    bool  hit        = false;
    f32   distance   = 0.0f;
    Vec3  point      = Vec3::Zero();
    Vec3  normal     = Vec3::Up();
    u32   bodyIndex  = 0;
    u32   bodyID     = 0;
};

/* ═══════════════════════════════════════════════════════════════════════
 *  PhysicsStats – telemetry exposed to profilers / debug UI
 * ═══════════════════════════════════════════════════════════════════════*/
struct PhysicsStats {
    u32 bodyCount        = 0;
    u32 activeCount      = 0;
    u32 contactCount     = 0;
    u32 manifoldCount    = 0;
    u32 collisionPairs   = 0;
    u32 islandCount      = 0;
    u32 solverIterations = 0;
    f32 broadPhaseMs     = 0.0f;
    f32 narrowPhaseMs    = 0.0f;
    f32 solverMs         = 0.0f;
    f32 stepTimeMs       = 0.0f;
};

/* ═══════════════════════════════════════════════════════════════════════
 *  Constraint / Joint descriptors
 *
 *  Constraints link two bodies and restrict their relative motion.
 *  The solver resolves them together with contacts each sub-step.
 * ═══════════════════════════════════════════════════════════════════════*/
enum class ConstraintType : u8 {
    Distance,      ///< Keep two anchor points at a fixed distance.
    Hinge,         ///< Rotation around a single axis.
    BallSocket,    ///< Free rotation, no translation.
    Fixed,         ///< No relative motion at all.
    Slider,        ///< Translation along one axis only.
    Spring         ///< Soft constraint (Hooke's law + damping).
};

struct ConstraintDesc {
    ConstraintType type      = ConstraintType::Distance;
    u32            bodyA     = 0;
    u32            bodyB     = 0;
    Vec3           anchorA   = Vec3::Zero();  ///< In body-A local space.
    Vec3           anchorB   = Vec3::Zero();  ///< In body-B local space.
    Vec3           axis      = {0, 1, 0};     ///< Hinge / slider axis.
    f32            distance  = 1.0f;
    f32            stiffness = 0.0f;          ///< 0 = perfectly rigid.
    f32            damping   = 0.0f;
    f32            minLimit  = 0.0f;
    f32            maxLimit  = 0.0f;
    bool           enableLimits = false;
};

struct Constraint {
    ConstraintDesc desc;
    u32            id = 0;
};

/* ═══════════════════════════════════════════════════════════════════════
 *  SpatialHash – uniform-grid broadphase
 *
 *  Every Step() we clear the grid, insert every active AABB, then
 *  for each body query only the cells it touches.  Expected cost is
 *  O(n) when the cell size is chosen close to the median body size.
 * ═══════════════════════════════════════════════════════════════════════*/
class SpatialHash {
public:
    void Clear() { cells_.clear(); }

    void SetCellSize(f32 size) {
        cellSize_    = size;
        invCellSize_ = 1.0f / size;
    }

    f32 GetCellSize() const { return cellSize_; }

    /// Insert a body index into every cell its AABB touches.
    void Insert(u32 index, const AABB& aabb) {
        i32 minX = static_cast<i32>(std::floor(aabb.min.x * invCellSize_));
        i32 minY = static_cast<i32>(std::floor(aabb.min.y * invCellSize_));
        i32 minZ = static_cast<i32>(std::floor(aabb.min.z * invCellSize_));
        i32 maxX = static_cast<i32>(std::floor(aabb.max.x * invCellSize_));
        i32 maxY = static_cast<i32>(std::floor(aabb.max.y * invCellSize_));
        i32 maxZ = static_cast<i32>(std::floor(aabb.max.z * invCellSize_));

        for (i32 x = minX; x <= maxX; x++)
            for (i32 y = minY; y <= maxY; y++)
                for (i32 z = minZ; z <= maxZ; z++)
                    cells_[HashKey(x, y, z)].push_back(index);
    }

    /// Return every body index stored in cells overlapped by `aabb`.
    /// Caller is responsible for de-duplicating and self-filtering.
    void Query(const AABB& aabb, std::vector<u32>& results) const {
        i32 minX = static_cast<i32>(std::floor(aabb.min.x * invCellSize_));
        i32 minY = static_cast<i32>(std::floor(aabb.min.y * invCellSize_));
        i32 minZ = static_cast<i32>(std::floor(aabb.min.z * invCellSize_));
        i32 maxX = static_cast<i32>(std::floor(aabb.max.x * invCellSize_));
        i32 maxY = static_cast<i32>(std::floor(aabb.max.y * invCellSize_));
        i32 maxZ = static_cast<i32>(std::floor(aabb.max.z * invCellSize_));

        for (i32 x = minX; x <= maxX; x++)
            for (i32 y = minY; y <= maxY; y++)
                for (i32 z = minZ; z <= maxZ; z++) {
                    auto it = cells_.find(HashKey(x, y, z));
                    if (it != cells_.end())
                        for (u32 idx : it->second)
                            results.push_back(idx);
                }
    }

private:
    /// Large-prime spatial hash (Thomas Wang style).
    u64 HashKey(i32 x, i32 y, i32 z) const {
        constexpr u64 p1 = 73856093ULL;
        constexpr u64 p2 = 19349663ULL;
        constexpr u64 p3 = 83492791ULL;
        return (static_cast<u64>(x) * p1) ^
               (static_cast<u64>(y) * p2) ^
               (static_cast<u64>(z) * p3);
    }

    f32 cellSize_    = 2.0f;
    f32 invCellSize_ = 0.5f;
    std::unordered_map<u64, std::vector<u32>> cells_;
};

/* ═══════════════════════════════════════════════════════════════════════
 *  PhysMath – commonly used vector / inertia helpers
 *
 *  Kept in a namespace so they don't pollute the global scope and
 *  can be used from both RigidBody.cpp and PhysicsWorld.cpp.
 * ═══════════════════════════════════════════════════════════════════════*/
namespace PhysMath {

    inline f32 Dot(const Vec3& a, const Vec3& b) {
        return a.x*b.x + a.y*b.y + a.z*b.z;
    }

    inline Vec3 Cross(const Vec3& a, const Vec3& b) {
        return {a.y*b.z - a.z*b.y,
                a.z*b.x - a.x*b.z,
                a.x*b.y - a.y*b.x};
    }

    inline f32 Length(const Vec3& v) {
        return std::sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
    }

    inline f32 LengthSq(const Vec3& v) {
        return v.x*v.x + v.y*v.y + v.z*v.z;
    }

    inline Vec3 Normalize(const Vec3& v) {
        f32 len = Length(v);
        if (len < 1e-7f) return {0, 1, 0};
        f32 inv = 1.0f / len;
        return {v.x*inv, v.y*inv, v.z*inv};
    }

    inline Vec3 Scale(const Vec3& v, f32 s) {
        return {v.x*s, v.y*s, v.z*s};
    }

    inline Vec3 Add(const Vec3& a, const Vec3& b) {
        return {a.x+b.x, a.y+b.y, a.z+b.z};
    }

    inline Vec3 Sub(const Vec3& a, const Vec3& b) {
        return {a.x-b.x, a.y-b.y, a.z-b.z};
    }

    inline Vec3 Neg(const Vec3& v) {
        return {-v.x, -v.y, -v.z};
    }

    inline f32 Clamp(f32 v, f32 lo, f32 hi) {
        return std::max(lo, std::min(v, hi));
    }

    /// Build an orthonormal tangent basis from a unit normal.
    /// Uses the Frisvad / Pixar "Building an Orthonormal Basis" trick.
    inline void ComputeBasis(const Vec3& n, Vec3& t, Vec3& b) {
        if (std::abs(n.x) >= 0.57735f)
            t = Normalize({n.y, -n.x, 0.0f});
        else
            t = Normalize({0.0f, n.z, -n.y});
        b = Cross(n, t);
    }

    /* ── Inertia-tensor factories (body-local, diagonal) ─────── */

    /// Solid sphere:  I = (2/5) m r²
    inline Mat3 SphereInertia(f32 mass, f32 radius) {
        f32 i = (2.0f / 5.0f) * mass * radius * radius;
        return Mat3::Diagonal(i, i, i);
    }

    /// Solid box:  I_x = m/12 (h² + d²),  etc.
    inline Mat3 BoxInertia(f32 mass, const Vec3& halfExtents) {
        f32 w2 = 4.0f * halfExtents.x * halfExtents.x;
        f32 h2 = 4.0f * halfExtents.y * halfExtents.y;
        f32 d2 = 4.0f * halfExtents.z * halfExtents.z;
        f32 k  = mass / 12.0f;
        return Mat3::Diagonal(k*(h2+d2), k*(w2+d2), k*(w2+h2));
    }

    /// Capsule (Y-axis cylinder + two hemispheres).
    inline Mat3 CapsuleInertia(f32 mass, f32 radius, f32 halfHeight) {
        f32 h  = 2.0f * halfHeight;
        f32 r2 = radius * radius;
        f32 h2 = h * h;
        f32 cylFrac = h / (h + (4.0f / 3.0f) * radius);
        f32 cylM = mass * cylFrac;
        f32 capM = mass - cylM;
        f32 iy = cylM * r2 * 0.5f + capM * 2.0f / 5.0f * r2;
        f32 ix = cylM * (r2 / 4.0f + h2 / 12.0f) +
                 capM * (2.0f / 5.0f * r2 + halfHeight * halfHeight);
        return Mat3::Diagonal(ix, iy, ix);
    }
}

} // namespace RiftCore
