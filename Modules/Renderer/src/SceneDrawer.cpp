#include <Renderer/SceneDrawer.h>
#include <Renderer/Camera.h>
#include <Renderer/OBJLoader.h>
#include <RiftCore/Common/EulerUtil.h>

#include <algorithm>
#include <vector>

namespace RiftCore {

    SceneDrawer::SceneDrawer(RenderSystem* renderer) : renderer_(renderer) {}

    SceneDrawer::~SceneDrawer() { Clear(); }

    void SceneDrawer::Clear() {
        if (renderer_) {
            std::vector<GPUMesh*> unique;
            for (auto& [path, mesh] : meshes_) {
                if (mesh && std::find(unique.begin(), unique.end(), mesh) == unique.end()) {
                    unique.push_back(mesh);
                }
            }
            for (GPUMesh* mesh : unique) renderer_->DestroyMesh(mesh);
        }
        meshes_.clear();
    }

    GPUMesh* SceneDrawer::GetMesh(const String& path) {
        auto it = meshes_.find(path);
        if (it != meshes_.end()) return it->second;

        MeshData data;
        bool ok = MeshFactory::CreateBuiltin(path, data);
        if (!ok && path.size() > 4 && path.find(':') == String::npos) {
            OBJLoader loader;
            auto r = loader.LoadMesh(path);
            if (r.IsOk() && !r.Value().vertices.empty()) {
                data = r.Value();
                ok   = true;
            }
        }

        GPUMesh* mesh = nullptr;
        if (ok) {
            auto up = renderer_->UploadMesh(data);
            if (up.IsOk()) mesh = up.Value();
        }
        if (!mesh && path != "primitive:cube") {
            mesh = GetMesh("primitive:cube");   // shared fallback, freed once
        }
        meshes_[path] = mesh;
        return mesh;
    }

    Mat4 SceneDrawer::WorldMatrix(ISceneSystem* scene, ISceneNode* node) {
        Vec3 r = node->GetLocalRotation();
        Mat4 m = Math::TRSFull(node->GetLocalPosition(), r.x, r.y, r.z,
                               node->GetLocalScale());
        int guard = 0;
        SceneNodeID parentID = node->GetParentID();
        while (parentID != INVALID_NODE && scene && guard++ < 64) {
            ISceneNode* p = scene->GetNode(parentID);
            if (!p) break;
            Vec3 pr = p->GetLocalRotation();
            m = Math::Multiply(
                Math::TRSFull(p->GetLocalPosition(), pr.x, pr.y, pr.z, p->GetLocalScale()), m);
            parentID = p->GetParentID();
        }
        return m;
    }

    void SceneDrawer::Draw(ISceneSystem* scene, const Light& defaultSun) {
        if (!scene || !renderer_) return;

        Light sun = defaultSun;
        bool  haveSun = false;

        struct Item { ISceneNode* node; SceneNodeDesc desc; };
        std::vector<Item> items;

        scene->ForEachNode([&](ISceneNode* node) {
            if (!node->IsActive()) return;
            SceneNodeDesc d;
            if (!scene->GetNodeDesc(node->GetID(), d)) return;

            if (d.hasLight && !haveSun && d.light.type == "directional") {
                // A light node shines along its local -Y axis.
                auto m = EulerUtil::FromEulerDeg(d.rotation);
                sun.type      = LightType::Directional;
                sun.direction = { -m.m[0][1], -m.m[1][1], -m.m[2][1] };
                sun.color     = d.light.color;
                sun.intensity = d.light.intensity;
                haveSun       = true;
            }
            if (d.hasMesh) items.push_back({ node, std::move(d) });
        });

        renderer_->SubmitLight(sun);

        for (auto& it : items) {
            DrawCall dc;
            dc.mesh = GetMesh(it.desc.mesh.meshPath);
            if (!dc.mesh) continue;
            dc.material.albedo    = it.desc.mesh.albedo;
            dc.material.metallic  = it.desc.mesh.metallic;
            dc.material.roughness = it.desc.mesh.roughness;
            dc.transform          = WorldMatrix(scene, it.node);
            renderer_->Submit(dc);
        }
    }

} // namespace RiftCore
