#pragma once

#include <RiftCore/Common/Types.h>
namespace RiftCore {

    // ── Core built-in components ──────────────────────────────
    // These are plain data structs - no virtual functions

    struct TransformComponent {
        Vec3 position = Vec3::Zero();
        Quat rotation = Quat::Identity();
        Vec3 scale    = Vec3::One();

        TransformComponent() = default;
        TransformComponent(const Vec3& pos)
            : position(pos) {}
        TransformComponent(const Vec3& pos,
                           const Quat& rot,
                           const Vec3& scl)
            : position(pos), rotation(rot), scale(scl) {}
    };

    struct VelocityComponent {
        Vec3 linear  = Vec3::Zero();
        Vec3 angular = Vec3::Zero();

        VelocityComponent() = default;
        VelocityComponent(const Vec3& lin)
            : linear(lin) {}
    };

    struct HealthComponent {
        f32 current = 100.0f;
        f32 maximum = 100.0f;
        bool isDead = false;

        HealthComponent() = default;
        HealthComponent(f32 max)
            : current(max), maximum(max) {}

        f32  GetPercent() const {
            return maximum > 0 ? current / maximum : 0;
        }
        void TakeDamage(f32 dmg) {
            current -= dmg;
            if (current <= 0) { current = 0; isDead = true; }
        }
        void Heal(f32 amount) {
            current = (current + amount > maximum)
                    ? maximum : current + amount;
            isDead  = false;
        }
    };

    struct TagComponent {
        String tag;
        TagComponent() = default;
        TagComponent(const String& t) : tag(t) {}
    };

    struct ActiveComponent {
        bool active = true;
        ActiveComponent() = default;
        ActiveComponent(bool a) : active(a) {}
    };

    struct NameComponent {
        String name;
        NameComponent() = default;
        NameComponent(const String& n) : name(n) {}
    };

    struct MeshComponent {
        MeshHandle     mesh;
        MaterialHandle material;
        bool           castShadow    = true;
        bool           receiveShadow = true;

        MeshComponent() = default;
        MeshComponent(MeshHandle m, MaterialHandle mat)
            : mesh(m), material(mat) {}
    };

    struct CameraComponent {
        f32  fovY        = 60.0f;
        f32  nearPlane   = 0.1f;
        f32  farPlane    = 1000.0f;
        bool isPrimary   = false;

        CameraComponent() = default;
        CameraComponent(f32 fov, bool primary = false)
            : fovY(fov), isPrimary(primary) {}
    };

    struct ScriptComponent {
        String scriptPath;
        bool   started = false;

        ScriptComponent() = default;
        ScriptComponent(const String& path)
            : scriptPath(path) {}
    };

} // namespace RiftCore
