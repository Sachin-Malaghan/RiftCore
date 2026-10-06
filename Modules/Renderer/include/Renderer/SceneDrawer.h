#pragma once
#pragma warning(push)
#pragma warning(disable: 4251 4275)

// Turns a scene into draw calls. Shared by the Editor and the Runtime so
// both show exactly the same thing.

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Scene/ISceneSystem.h>
#include <Renderer/RenderSystem.h>
#include <Renderer/RenderTypes.h>

#include <string>
#include <unordered_map>

namespace RiftCore {

    class MaterialLibrary;

    class RENDERER_API SceneDrawer {
    public:
        explicit SceneDrawer(RenderSystem* renderer);
        ~SceneDrawer();
        RIFTCORE_NOCOPY_NOMOVE(SceneDrawer);

        // Mesh for a node's mesh path ("primitive:..", "model:..", or an
        // .obj file). Cached; unknown paths fall back to the cube.
        GPUMesh* GetMesh(const String& path);

        // Submits the scene's light and every active mesh node. Call
        // between RenderSystem::BeginFrame and EndFrame. `defaultSun` is
        // used when the scene has no directional light node.
        void Draw(ISceneSystem* scene, const Light& defaultSun);

        // World matrix of a node (parents included).
        static Mat4 WorldMatrix(ISceneSystem* scene, ISceneNode* node);

        // Frees every cached GPU mesh.
        void Clear();

    private:
        RenderSystem* renderer_ = nullptr;
        std::unordered_map<String, GPUMesh*> meshes_;
        MaterialLibrary* materials_ = nullptr;   // named materials (Realistic mode)
    };

} // namespace RiftCore

#pragma warning(pop)
