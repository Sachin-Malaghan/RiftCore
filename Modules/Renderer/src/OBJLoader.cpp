#include <Renderer/OBJLoader.h>

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <unordered_map>

namespace RiftCore {

    // ── OBJModel::GetCombinedMesh ─────────────────────────────
    MeshData OBJModel::GetCombinedMesh() const {
        MeshData combined;
        combined.name = name;

        for (auto& sub : subMeshes) {
            u32 baseIndex = static_cast<u32>(
                combined.vertices.size());

            for (auto& v : sub.mesh.vertices) {
                combined.vertices.push_back(v);
            }
            for (auto idx : sub.mesh.indices) {
                combined.indices.push_back(baseIndex + idx);
            }
        }
        return combined;
    }

    // ── Helper functions ──────────────────────────────────────
    String OBJLoader::GetDirectory(const String& filePath) {
        size_t pos = filePath.find_last_of("/\\");
        if (pos == String::npos) return "./";
        return filePath.substr(0, pos + 1);
    }

    String OBJLoader::Trim(const String& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == String::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    std::vector<String> OBJLoader::Split(
        const String& s, char delim
    ) {
        std::vector<String> result;
        std::stringstream ss(s);
        String token;
        while (std::getline(ss, token, delim)) {
            result.push_back(token);
        }
        return result;
    }

    void OBJLoader::ParseFaceIndex(
        const String& token,
        i32& posIdx, i32& texIdx, i32& normIdx
    ) {
        posIdx  = 0;
        texIdx  = 0;
        normIdx = 0;

        auto parts = Split(token, '/');
        if (parts.size() >= 1 && !parts[0].empty())
            posIdx  = std::stoi(parts[0]);
        if (parts.size() >= 2 && !parts[1].empty())
            texIdx  = std::stoi(parts[1]);
        if (parts.size() >= 3 && !parts[2].empty())
            normIdx = std::stoi(parts[2]);
    }

    Vertex3D OBJLoader::BuildVertex(
        const std::vector<Vec3>& positions,
        const std::vector<Vec2>& texCoords,
        const std::vector<Vec3>& normals,
        i32 posIdx, i32 texIdx, i32 normIdx,
        const Vec3& defaultColor
    ) {
        Vertex3D v;
        v.color = defaultColor;

        // OBJ indices are 1-based
        // Negative indices count from end
        if (posIdx != 0) {
            i32 idx = posIdx > 0
                ? posIdx - 1
                : static_cast<i32>(positions.size()) + posIdx;
            if (idx >= 0 &&
                idx < static_cast<i32>(positions.size())) {
                v.position = positions[idx];
            }
        }

        if (texIdx != 0) {
            i32 idx = texIdx > 0
                ? texIdx - 1
                : static_cast<i32>(texCoords.size()) + texIdx;
            if (idx >= 0 &&
                idx < static_cast<i32>(texCoords.size())) {
                v.texCoord = texCoords[idx];
            }
        }

        if (normIdx != 0) {
            i32 idx = normIdx > 0
                ? normIdx - 1
                : static_cast<i32>(normals.size()) + normIdx;
            if (idx >= 0 &&
                idx < static_cast<i32>(normals.size())) {
                v.normal = normals[idx];
            }
        }

        return v;
    }

    // ── Load MTL file ─────────────────────────────────────────
    void OBJLoader::LoadMTL(
        const String& mtlPath,
        std::unordered_map<String, OBJMaterial>& materials
    ) {
        std::ifstream file(mtlPath);
        if (!file.is_open()) {
            std::cerr << "[OBJLoader] Cannot open MTL: "
                      << mtlPath << "\n";
            return;
        }

        OBJMaterial* current = nullptr;
        String line;

        while (std::getline(file, line)) {
            line = Trim(line);
            if (line.empty() || line[0] == '#') continue;

            std::istringstream iss(line);
            String keyword;
            iss >> keyword;

            if (keyword == "newmtl") {
                String matName;
                iss >> matName;
                materials[matName] = OBJMaterial{};
                materials[matName].name = matName;
                current = &materials[matName];
            }
            else if (current) {
                if (keyword == "Ka") {
                    iss >> current->ambient.x
                        >> current->ambient.y
                        >> current->ambient.z;
                }
                else if (keyword == "Kd") {
                    iss >> current->diffuse.x
                        >> current->diffuse.y
                        >> current->diffuse.z;
                }
                else if (keyword == "Ks") {
                    iss >> current->specular.x
                        >> current->specular.y
                        >> current->specular.z;
                }
                else if (keyword == "Ns") {
                    iss >> current->shininess;
                }
                else if (keyword == "map_Kd") {
                    iss >> current->diffuseMap;
                }
                else if (keyword == "map_Bump" ||
                         keyword == "bump") {
                    iss >> current->normalMap;
                }
            }
        }

        std::cout << "[OBJLoader] Loaded MTL: "
                  << materials.size()
                  << " materials from "
                  << mtlPath << "\n";
    }

    // ── Load OBJ file ─────────────────────────────────────────
    Result<OBJModel> OBJLoader::Load(const String& filePath) {
        std::ifstream file(filePath);
        if (!file.is_open()) {
            return Result<OBJModel>::Err(
                "Cannot open OBJ file: " + filePath);
        }

        String dir = GetDirectory(filePath);

        // Raw data arrays
        std::vector<Vec3> positions;
        std::vector<Vec2> texCoords;
        std::vector<Vec3> normals;

        OBJModel model;
        model.name = filePath;

        // Current sub-mesh being built
        String currentMat = "default";
        OBJSubMesh currentSubMesh;
        currentSubMesh.materialName = currentMat;

        // Vertex deduplication map
        // Key: "posIdx/texIdx/normIdx"
        // Value: index in vertices array
        std::unordered_map<String, u32> vertexCache;

        String line;
        u32 lineNum = 0;

        while (std::getline(file, line)) {
            lineNum++;
            line = Trim(line);
            if (line.empty() || line[0] == '#') continue;

            std::istringstream iss(line);
            String keyword;
            iss >> keyword;

            // ── Vertex position ───────────────────────────
            if (keyword == "v") {
                Vec3 pos;
                iss >> pos.x >> pos.y >> pos.z;
                positions.push_back(pos);
            }
            // ── Texture coordinate ────────────────────────
            else if (keyword == "vt") {
                Vec2 uv;
                iss >> uv.x >> uv.y;
                texCoords.push_back(uv);
            }
            // ── Vertex normal ─────────────────────────────
            else if (keyword == "vn") {
                Vec3 norm;
                iss >> norm.x >> norm.y >> norm.z;
                normals.push_back(norm);
            }
            // ── Material library ──────────────────────────
            else if (keyword == "mtllib") {
                String mtlFile;
                iss >> mtlFile;
                LoadMTL(dir + mtlFile, model.materials);
            }
            // ── Use material ──────────────────────────────
            else if (keyword == "usemtl") {
                String matName;
                iss >> matName;

                // Save current sub-mesh if it has faces
                if (!currentSubMesh.mesh.indices.empty()) {
                    model.subMeshes.push_back(currentSubMesh);
                }

                // Start new sub-mesh
                currentSubMesh = OBJSubMesh{};
                currentSubMesh.materialName = matName;
                currentMat = matName;
                vertexCache.clear();
            }
            // ── Object/Group name ─────────────────────────
            else if (keyword == "o" || keyword == "g") {
                String name;
                iss >> name;
                if (!currentSubMesh.mesh.indices.empty()) {
                    model.subMeshes.push_back(currentSubMesh);
                    currentSubMesh = OBJSubMesh{};
                    currentSubMesh.materialName = currentMat;
                    vertexCache.clear();
                }
            }
            // ── Face ──────────────────────────────────────
            else if (keyword == "f") {
                // Get material color for vertex color
                Vec3 matColor = {0.8f, 0.8f, 0.8f};
                auto matIt = model.materials.find(currentMat);
                if (matIt != model.materials.end()) {
                    matColor = matIt->second.diffuse;
                }

                // Read all face vertices (supports quads+)
                std::vector<String> faceTokens;
                String token;
                while (iss >> token) {
                    faceTokens.push_back(token);
                }

                if (faceTokens.size() < 3) continue;

                // Triangulate the face (fan triangulation)
                // Handles triangles, quads, and polygons
                std::vector<u32> faceIndices;

                for (auto& ftoken : faceTokens) {
                    i32 posIdx = 0, texIdx = 0, normIdx = 0;
                    ParseFaceIndex(ftoken,
                        posIdx, texIdx, normIdx);

                    // Check vertex cache
                    String cacheKey =
                        std::to_string(posIdx) + "/" +
                        std::to_string(texIdx)  + "/" +
                        std::to_string(normIdx);

                    auto cacheIt = vertexCache.find(cacheKey);
                    if (cacheIt != vertexCache.end()) {
                        faceIndices.push_back(cacheIt->second);
                    } else {
                        Vertex3D v = BuildVertex(
                            positions, texCoords, normals,
                            posIdx, texIdx, normIdx,
                            matColor
                        );

                        u32 idx = static_cast<u32>(
                            currentSubMesh.mesh.vertices.size());
                        currentSubMesh.mesh.vertices.push_back(v);
                        vertexCache[cacheKey] = idx;
                        faceIndices.push_back(idx);
                    }
                }

                // Fan triangulation: (0,1,2), (0,2,3), ...
                for (size_t i = 1;
                     i + 1 < faceIndices.size(); i++) {
                    currentSubMesh.mesh.indices.push_back(
                        faceIndices[0]);
                    currentSubMesh.mesh.indices.push_back(
                        faceIndices[i]);
                    currentSubMesh.mesh.indices.push_back(
                        faceIndices[i+1]);
                }
            }
        }

        // Save last sub-mesh
        if (!currentSubMesh.mesh.indices.empty()) {
            model.subMeshes.push_back(currentSubMesh);
        }

        // If no normals in file, generate flat normals
        for (auto& sub : model.subMeshes) {
            bool hasNormals = false;
            for (auto& v : sub.mesh.vertices) {
                if (v.normal.x != 0 || v.normal.y != 0
                    || v.normal.z != 0) {
                    hasNormals = true;
                    break;
                }
            }

            if (!hasNormals) {
                // Generate flat normals per triangle
                auto& verts   = sub.mesh.vertices;
                auto& indices = sub.mesh.indices;
                for (size_t i = 0;
                     i + 2 < indices.size(); i += 3) {
                    auto& v0 = verts[indices[i]];
                    auto& v1 = verts[indices[i+1]];
                    auto& v2 = verts[indices[i+2]];

                    Vec3 ab = {
                        v1.position.x - v0.position.x,
                        v1.position.y - v0.position.y,
                        v1.position.z - v0.position.z
                    };
                    Vec3 ac = {
                        v2.position.x - v0.position.x,
                        v2.position.y - v0.position.y,
                        v2.position.z - v0.position.z
                    };

                    // Cross product
                    Vec3 n = {
                        ab.y*ac.z - ab.z*ac.y,
                        ab.z*ac.x - ab.x*ac.z,
                        ab.x*ac.y - ab.y*ac.x
                    };
                    f32 len = std::sqrt(
                        n.x*n.x + n.y*n.y + n.z*n.z);
                    if (len > 0.0001f) {
                        n.x /= len;
                        n.y /= len;
                        n.z /= len;
                    }

                    v0.normal = n;
                    v1.normal = n;
                    v2.normal = n;
                }
            }
        }

        u32 totalVerts = 0;
        u32 totalTris  = 0;
        for (auto& sub : model.subMeshes) {
            totalVerts += static_cast<u32>(
                sub.mesh.vertices.size());
            totalTris  += static_cast<u32>(
                sub.mesh.indices.size()) / 3;
        }

        std::cout << "[OBJLoader] Loaded: " << filePath
                  << "\n  SubMeshes: "
                  << model.subMeshes.size()
                  << "  Verts: "  << totalVerts
                  << "  Tris: "   << totalTris  << "\n";

        model.isValid = true;
        return Result<OBJModel>::Ok(std::move(model));
    }

    // ── Load just mesh data ───────────────────────────────────
    Result<MeshData> OBJLoader::LoadMesh(
        const String& filePath
    ) {
        auto result = Load(filePath);
        if (result.IsErr()) {
            return Result<MeshData>::Err(result.Error());
        }
        return Result<MeshData>::Ok(
            result.Value().GetCombinedMesh());
    }

} // namespace RiftCore
