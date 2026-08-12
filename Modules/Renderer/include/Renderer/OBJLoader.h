#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>

#include <Renderer/RenderTypes.h>

#include <vector>
#include <string>
#include <unordered_map>

#ifdef RENDERER_EXPORTS
    #define RENDERER_API RIFTCORE_EXPORT
#else
    #define RENDERER_API RIFTCORE_IMPORT
#endif









namespace RiftCore {

    // ── Material from MTL file ────────────────────────────────
    struct OBJMaterial {
        String name;
        Vec3   ambient   = {0.2f, 0.2f, 0.2f};
        Vec3   diffuse   = {0.8f, 0.8f, 0.8f};
        Vec3   specular  = {0.0f, 0.0f, 0.0f};
        f32    shininess = 32.0f;
        String diffuseMap;   // path to texture file
        String normalMap;
    };

    // ── Sub-mesh (one material group) ─────────────────────────
    struct OBJSubMesh {
        String   materialName;
        MeshData mesh;
    };

    // ── Loaded OBJ result ─────────────────────────────────────
    struct OBJModel {
        String                              name;
        std::vector<OBJSubMesh>             subMeshes;
        std::unordered_map<String,
            OBJMaterial>                    materials;
        bool                                isValid = false;

        // Get combined mesh (all sub-meshes merged)
        MeshData GetCombinedMesh() const;
    };

    // ── OBJ Loader ────────────────────────────────────────────
    class RENDERER_API OBJLoader {
    public:
        OBJLoader()  = default;
        ~OBJLoader() = default;

        // Load .obj file from path
        // Automatically loads associated .mtl if found
        Result<OBJModel> Load(const String& filePath);

        // Load just the mesh, ignore materials
        Result<MeshData> LoadMesh(const String& filePath);

    private:
        // Parse .mtl material library file
        void LoadMTL(
            const String& mtlPath,
            std::unordered_map<String,
                OBJMaterial>& materials
        );

        // Convert OBJ face indices to Vertex3D
        Vertex3D BuildVertex(
            const std::vector<Vec3>& positions,
            const std::vector<Vec2>& texCoords,
            const std::vector<Vec3>& normals,
            i32 posIdx,
            i32 texIdx,
            i32 normIdx,
            const Vec3& defaultColor
        );

        // Get directory from file path
        String GetDirectory(const String& filePath);

        // Trim whitespace
        String Trim(const String& s);

        // Split string by delimiter
        std::vector<String> Split(
            const String& s, char delim);

        // Parse face index "v/vt/vn" or "v//vn" or "v"
        void ParseFaceIndex(
            const String& token,
            i32& posIdx, i32& texIdx, i32& normIdx
        );
    };

} // namespace RiftCore

#pragma warning(pop)
