#pragma once
// Small helpers shared by the Editor, the Runtime and the scripting API.
#include <RiftCore/Scene/ISceneSystem.h>
#include <RiftCore/Scene/ModelCatalog.h>
#include <cmath>

namespace RiftCore::SceneUtil {

    // Sizes the node's collider to its mesh and scale. Every built-in
    // model fits a unit cube, so the scale is the size.
    inline void FitCollider(SceneNodeDesc& d) {
        const ModelCatalogEntry* e = FindModel(d.mesh.meshPath.c_str());
        Vec3 s = { std::fabs(d.scale.x), std::fabs(d.scale.y), std::fabs(d.scale.z) };

        if (d.mesh.meshPath == "primitive:plane" && d.physics.isStatic) {
            d.physics.colliderShape = "plane";          // infinite ground
            d.physics.halfExtents   = { s.x * 0.5f, 0.05f, s.z * 0.5f };
        } else if (e && std::strcmp(e->collider, "sphere") == 0) {
            d.physics.colliderShape = "sphere";
            d.physics.radius        = 0.5f * (s.x > s.y ? (s.x > s.z ? s.x : s.z)
                                                        : (s.y > s.z ? s.y : s.z));
        } else {
            d.physics.colliderShape = "box";
            d.physics.halfExtents   = { s.x * 0.5f, s.y * 0.5f, s.z * 0.5f };
            if (d.physics.halfExtents.y < 0.02f) d.physics.halfExtents.y = 0.02f;
        }
    }

    // mode: "none", "static" or "dynamic".
    inline void SetPhysicsMode(SceneNodeDesc& d, const char* mode, f32 mass = 1.0f) {
        String m = mode ? mode : "none";
        if (m == "static" || m == "dynamic") {
            d.hasPhysics       = true;
            d.physics.isStatic = (m == "static");
            d.physics.mass     = d.physics.isStatic ? 0.0f : (mass > 0.0f ? mass : 1.0f);
            FitCollider(d);
        } else {
            d.hasPhysics = false;
        }
    }

    // Fills `d` for a library model or mesh file. `kind` is a catalog name
    // ("cube", "model:car", ...), an .obj path, "empty" or "light".
    // Returns false if the kind is unknown.
    inline bool MakeNodeDesc(const char* kind, SceneNodeDesc& d) {
        String k = kind ? kind : "";
        if (k == "empty") return true;
        if (k == "light") {
            d.hasLight = true;
            return true;
        }
        if (const ModelCatalogEntry* e = FindModel(k.c_str())) {
            d.hasMesh       = true;
            d.mesh.meshPath = e->path;
            if (d.name.empty()) d.name = e->label;
            return true;
        }
        if (k.size() > 4 && (k.rfind(".obj") == k.size() - 4 || k.rfind(".OBJ") == k.size() - 4)) {
            d.hasMesh       = true;
            d.mesh.meshPath = k;
            return true;
        }
        return false;
    }

} // namespace RiftCore::SceneUtil
