// ============================================================
// Types.h
// All common math and primitive types used engine-wide.
// Uses GLM internally but wraps it behind our own names
// so we can swap math library without touching other code.
// ============================================================
#pragma once

#include "Platform.h"
#include <cstdint>
#include <cstddef>
#include <string>
#include <array>
#include <functional>
#include <cmath>

// ── Primitive Type Aliases ───────────────────────────────────
// Explicit-size types — NEVER use raw int/long in engine code
using i8  = int8_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using f32 = float;
using f64 = double;

using usize = size_t;
using isize = ptrdiff_t;
using byte  = uint8_t;

// ── Math Types ───────────────────────────────────────────────
// These are lightweight types — if you add GLM later,
// you can typedef Vec3 = glm::vec3 and nothing else changes.








namespace RiftCore {

    // ── 2D Vector ────────────────────────────────────────────
    struct Vec2 {
        f32 x = 0.0f, y = 0.0f;

        Vec2() = default;
        Vec2(f32 x, f32 y) : x(x), y(y) {}

        Vec2  operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
        Vec2  operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
        Vec2  operator*(f32 s)         const { return {x * s,   y * s};   }
        bool  operator==(const Vec2& o)const { return x==o.x && y==o.y;  }
    };

    // ── 3D Vector ────────────────────────────────────────────
    struct Vec3 {
        f32 x = 0.0f, y = 0.0f, z = 0.0f;

        Vec3() = default;
        Vec3(f32 x, f32 y, f32 z) : x(x), y(y), z(z) {}
        explicit Vec3(f32 v) : x(v), y(v), z(v) {}

        Vec3  operator+(const Vec3& o) const { return {x+o.x, y+o.y, z+o.z}; }
        Vec3  operator-(const Vec3& o) const { return {x-o.x, y-o.y, z-o.z}; }
        Vec3  operator*(f32 s)         const { return {x*s,   y*s,   z*s};   }
        Vec3  operator-()              const { return {-x, -y, -z};           }
        Vec3& operator+=(const Vec3& o)      { x+=o.x; y+=o.y; z+=o.z; return *this; }
        Vec3& operator*=(f32 s)              { x*=s;   y*=s;   z*=s;   return *this; }
        bool  operator==(const Vec3& o)const { return x==o.x && y==o.y && z==o.z; }

        f32   LengthSq() const { return x*x + y*y + z*z; }
        f32   Length()   const;     // defined in Types.cpp or inline math

        static Vec3 Zero()    { return {0,0,0}; }
        static Vec3 One()     { return {1,1,1}; }
        static Vec3 Up()      { return {0,1,0}; }
        static Vec3 Forward() { return {0,0,-1}; }
        static Vec3 Right()   { return {1,0,0}; }
    };

    // ── 4D Vector ────────────────────────────────────────────
    struct Vec4 {
        f32 x = 0.0f, y = 0.0f, z = 0.0f, w = 0.0f;

        Vec4() = default;
        Vec4(f32 x, f32 y, f32 z, f32 w) : x(x), y(y), z(z), w(w) {}
        Vec4(const Vec3& v, f32 w)        : x(v.x), y(v.y), z(v.z), w(w) {}
    };

    // ── Quaternion ───────────────────────────────────────────
    //  Unit quaternion stored as (x, y, z, w).
    //  Quaternions avoid gimbal lock and interpolation artefacts
    //  inherent in Euler angles.
    struct Quat {
        f32 x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;  // identity

        Quat() = default;
        Quat(f32 x, f32 y, f32 z, f32 w) : x(x), y(y), z(z), w(w) {}

        static Quat Identity() { return {0, 0, 0, 1}; }

        f32 LengthSq() const { return w*w + x*x + y*y + z*z; }

        /// Re-normalise to unit length (drift accumulates over time).
        Quat Normalized() const {
            f32 len = std::sqrt(LengthSq());
            if (len < 1e-8f) return Identity();
            f32 inv = 1.0f / len;
            return {x*inv, y*inv, z*inv, w*inv};
        }

        Quat Conjugate() const { return {-x, -y, -z, w}; }

        /// Hamilton product – combines two rotations.
        Quat operator*(const Quat& q) const {
            return {
                w*q.x + x*q.w + y*q.z - z*q.y,
                w*q.y - x*q.z + y*q.w + z*q.x,
                w*q.z + x*q.y - y*q.x + z*q.w,
                w*q.w - x*q.x - y*q.y - z*q.z
            };
        }

        /// Rotate vector v by this quaternion:  q·v·q⁻¹
        Vec3 Rotate(const Vec3& v) const {
            Vec3 u  = {x, y, z};
            f32  s  = w;
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

        /// Construct from an axis (must be unit-length) and an angle (rad).
        static Quat FromAxisAngle(const Vec3& axis, f32 angle) {
            f32 half = angle * 0.5f;
            f32 s = std::sin(half);
            return {axis.x*s, axis.y*s, axis.z*s, std::cos(half)};
        }

        /// First-order integration:  q += ½·(0,ω)·q · dt
        /// Then re-normalise.  Called every sub-step.
        void IntegrateAngularVelocity(const Vec3& omega, f32 dt) {
            Quat dq = {omega.x * 0.5f, omega.y * 0.5f, omega.z * 0.5f, 0};
            Quat spin = {
                dq.w*x + dq.x*w + dq.y*z - dq.z*y,
                dq.w*y - dq.x*z + dq.y*w + dq.z*x,
                dq.w*z + dq.x*y - dq.y*x + dq.z*w,
                dq.w*w - dq.x*x - dq.y*y - dq.z*z
            };
            x += spin.x * dt;
            y += spin.y * dt;
            z += spin.z * dt;
            w += spin.w * dt;
            *this = this->Normalized();
        }
    };

    // ── 4x4 Matrix ───────────────────────────────────────────
    // Column-major, matches OpenGL/Vulkan convention
    struct Mat4 {
        // 4 columns, each with 4 rows
        std::array<std::array<f32, 4>, 4> cols{};

        Mat4() = default;

        // Creates identity matrix
        static Mat4 Identity() {
            Mat4 m{};
            m.cols[0][0] = 1.0f;
            m.cols[1][1] = 1.0f;
            m.cols[2][2] = 1.0f;
            m.cols[3][3] = 1.0f;
            return m;
        }

        const f32* DataPtr() const {
            return &cols[0][0];
        }
    };

    // ── Transform ────────────────────────────────────────────
    // Represents position + rotation + scale of any object
    struct Transform {
        Vec3 position = Vec3::Zero();
        Quat rotation = Quat::Identity();
        Vec3 scale    = Vec3::One();

        Transform() = default;
        Transform(const Vec3& pos) : position(pos) {}
        Transform(const Vec3& pos, const Quat& rot, const Vec3& scl)
            : position(pos), rotation(rot), scale(scl) {}
    };

    // ── Color ────────────────────────────────────────────────
    struct Color {
        f32 r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;

        Color() = default;
        Color(f32 r, f32 g, f32 b, f32 a = 1.0f) : r(r), g(g), b(b), a(a) {}

        static Color White()       { return {1,1,1,1}; }
        static Color Black()       { return {0,0,0,1}; }
        static Color Red()         { return {1,0,0,1}; }
        static Color Green()       { return {0,1,0,1}; }
        static Color Blue()        { return {0,0,1,1}; }
        static Color Transparent() { return {0,0,0,0}; }

        u32 ToRGBA8() const {
            return  (static_cast<u32>(r * 255) << 24) |
                    (static_cast<u32>(g * 255) << 16) |
                    (static_cast<u32>(b * 255) <<  8) |
                    (static_cast<u32>(a * 255));
        }
    };

    // ── Rect ─────────────────────────────────────────────────
    struct Rect {
        f32 x = 0, y = 0, width = 0, height = 0;

        Rect() = default;
        Rect(f32 x, f32 y, f32 w, f32 h) : x(x), y(y), width(w), height(h) {}

        bool Contains(f32 px, f32 py) const {
            return px >= x && px <= x + width &&
                   py >= y && py <= y + height;
        }
    };

    // ── Viewport ─────────────────────────────────────────────
    struct Viewport {
        f32 x        = 0.0f;
        f32 y        = 0.0f;
        f32 width    = 1280.0f;
        f32 height   = 720.0f;
        f32 minDepth = 0.0f;
        f32 maxDepth = 1.0f;
    };

    // ── GUID ─────────────────────────────────────────────────
    // 128-bit unique identifier for assets, entities, etc.
    struct GUID {
        u64 high = 0;
        u64 low  = 0;

        GUID() = default;
        GUID(u64 h, u64 l) : high(h), low(l) {}

        bool operator==(const GUID& o) const {
            return high == o.high && low == o.low;
        }
        bool operator!=(const GUID& o) const { return !(*this == o); }
        bool operator< (const GUID& o) const {
            return high < o.high || (high == o.high && low < o.low);
        }
        bool IsNull() const { return high == 0 && low == 0; }

        static GUID Null() { return {0, 0}; }

        // Generate a simple pseudo-GUID
        // Real engine would use UuidCreate() or platform API
        static GUID Generate();

        std::string ToString() const;
    };

    // ── String Type Alias ─────────────────────────────────────
    using String     = std::string;
    using StringView = std::string_view;

    // ── Handle System ─────────────────────────────────────────
    // Type-safe handle — wraps u32 so we don't mix up IDs
    // Usage: Handle<Entity>, Handle<Texture>, Handle<Buffer>
    template<typename Tag>
    struct Handle {
        u32 id    = 0;
        u32 generation = 0;    // prevents use-after-free

        Handle() = default;
        Handle(u32 id, u32 gen) : id(id), generation(gen) {}

        bool IsValid()    const { return id != 0; }
        bool operator==(const Handle& o) const {
            return id == o.id && generation == o.generation;
        }
        bool operator!=(const Handle& o) const { return !(*this == o); }

        static Handle Invalid() { return {}; }
    };

    // Tags for typed handles — prevents mixing different handle types
    struct EntityTag{};
    struct TextureTag{};
    struct BufferTag{};
    struct MeshTag{};
    struct MaterialTag{};
    struct ShaderTag{};
    struct AudioClipTag{};
    struct AssetTag{};

    // Concrete handle types
    using EntityHandle   = Handle<EntityTag>;
    using TextureHandle  = Handle<TextureTag>;
    using BufferHandle   = Handle<BufferTag>;
    using MeshHandle     = Handle<MeshTag>;
    using MaterialHandle = Handle<MaterialTag>;
    using ShaderHandle   = Handle<ShaderTag>;
    using AssetHandle    = Handle<AssetTag>;

} // namespace RiftCore

// ── std::hash for GUID ───────────────────────────────────────
// Allows GUID to be used in std::unordered_map
namespace std {
    template<>
    struct hash<RiftCore::GUID> {
        size_t operator()(const RiftCore::GUID& g) const noexcept {
            size_t h1 = std::hash<uint64_t>{}(g.high);
            size_t h2 = std::hash<uint64_t>{}(g.low);
            return h1 ^ (h2 << 32) ^ (h2 >> 32);
        }
    };

    template<typename Tag>
    struct hash<RiftCore::Handle<Tag>> {
        size_t operator()(const RiftCore::Handle<Tag>& h) const noexcept {
            return std::hash<uint64_t>{}(
                (static_cast<uint64_t>(h.id) << 32) | h.generation
            );
        }
    };
} // namespace std
