#include <Scene/SceneSystem.h>
#include <RiftCore/Core/ILogger.h>
#include <RiftCore/Physics/IPhysics.h>
#include <RiftCore/Audio/IAudio.h>
#include <RiftCore/ECS/IECS.h>

#include <json.hpp>

#include <fstream>
#include <iostream>
#include <sstream>

namespace RiftCore {

    using json = nlohmann::json;

    // ── Helpers ───────────────────────────────────────────────
    static json Vec3ToJson(const Vec3& v) {
        return json::array({v.x, v.y, v.z});
    }

    static Vec3 JsonToVec3(const json& j,
                           Vec3 def = Vec3::Zero()) {
        if (!j.is_array() || j.size() < 3) return def;
        return {j[0].get<float>(),
                j[1].get<float>(),
                j[2].get<float>()};
    }

    // ── SceneSystem ───────────────────────────────────────────
    SceneSystem::SceneSystem()  = default;
    SceneSystem::~SceneSystem() { Shutdown(); }

    VoidResult SceneSystem::Initialize() {
        if (context_) {
            logger_ = context_->Logger();
        }
        if (logger_) {
            logger_->Info("Scene","Scene system initialized.");
        }
        return VoidResult::Ok();
    }

    void SceneSystem::Shutdown() {
        ClearScene();
        if (logger_) {
            logger_->Info("Scene","Scene system shutdown.");
        }
    }

    void SceneSystem::Update(f32 deltaTime) {
        RIFTCORE_UNUSED(deltaTime);

        // Sync physics body positions to scene nodes
        if (!context_) return;
        auto* physics = context_->Get<IPhysics>();
        if (!physics) return;

        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [id, node] : nodes_) {
            if (node->hasPhysics &&
                node->GetPhysicsBodyID() != 0) {
                // Get physics position and update node
                // (physics drives transform for dynamic objects)
                RIFTCORE_UNUSED(node);
            }
        }
    }

    // ── Scene management ──────────────────────────────────────
    VoidResult SceneSystem::NewScene(const String& name) {
        ClearScene();
        sceneName_     = name;
        sceneFilePath_ = "";
        if (logger_) {
            logger_->Info("Scene",
                "New scene created: " + name);
        }
        return VoidResult::Ok();
    }

    void SceneSystem::ClearScene() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (context_) {
            // ── Remove all physics bodies ─────────────────
            auto* physics = context_->Get<IPhysics>();
            if (physics) {
                for (auto& [id, node] : nodes_) {
                    if (node->hasPhysics) {
                        // Remove via ECS entity ID
                        EntityID eid = node->GetEntityID();
                        if (eid != 0) {
                            physics->RemoveRigidBody(eid);
                        }
                    }
                }
            }

            // ── Remove all audio sources ──────────────────
            auto* audio = context_->Get<IAudio>();
            if (audio) {
                for (auto& [id, node] : nodes_) {
                    if (node->hasAudio &&
                        node->GetAudioSourceID() != 0) {
                        audio->DestroySource(
                            node->GetAudioSourceID());
                    }
                }
            }

            // ── Remove all ECS entities ───────────────────
            auto* ecs = context_->Get<IECS>();
            if (ecs) {
                for (auto& [id, node] : nodes_) {
                    EntityID eid = node->GetEntityID();
                    if (eid != 0) {
                        ecs->DestroyEntity(eid);
                    }
                }
            }
        }

        nodes_.clear();
        rootNodes_.clear();
        sceneName_     = "";
        sceneFilePath_ = "";

        if (logger_) {
            logger_->Info("Scene","Scene cleared.");
        }
    }

    SceneInfo SceneSystem::GetSceneInfo() const {
        SceneInfo info;
        info.name       = sceneName_;
        info.filePath   = sceneFilePath_;
        info.nodeCount  = static_cast<u32>(nodes_.size());
        info.isLoaded   = !sceneName_.empty();
        return info;
    }

    // ── Node management ───────────────────────────────────────
    Result<SceneNodeID> SceneSystem::CreateNode(
        const SceneNodeDesc& desc
    ) {
        SceneNodeID id = nextNodeID_.fetch_add(1);

        auto node = std::make_unique<SceneNode>(
            id, desc.name.empty() ?
            "Node_" + std::to_string(id) : desc.name);

        node->SetLocalPosition(desc.position);
        node->SetLocalRotation(desc.rotation);
        node->SetLocalScale   (desc.scale);

        // Store component descriptors
        if (desc.hasMesh) {
            node->hasMesh  = true;
            node->meshDesc = desc.mesh;
        }
        if (desc.hasPhysics) {
            node->hasPhysics  = true;
            node->physicsDesc = desc.physics;
        }
        if (desc.hasAudio) {
            node->hasAudio  = true;
            node->audioDesc = desc.audio;
        }
        if (desc.hasLight) {
            node->hasLight  = true;
            node->lightDesc = desc.light;
        }

        // Parent-child relationship
        if (desc.parentID != INVALID_NODE) {
            node->SetParentID(desc.parentID);
            auto* parent = GetNodeRaw(desc.parentID);
            if (parent) {
                parent->AddChild(id);
                node->SetParentNode(parent);
            }
        } else {
            rootNodes_.push_back(id);
        }

        // Create ECS entity
        CreateECSEntity(*node, desc);

        // Create physics body
        if (desc.hasPhysics) {
            CreatePhysicsBody(*node, desc);
        }

        // Create audio source
        if (desc.hasAudio) {
            CreateAudioSource(*node, desc);
        }

        std::lock_guard<std::mutex> lock(mutex_);
        auto* ptr = node.get();
        nodes_[id] = std::move(node);
        RIFTCORE_UNUSED(ptr);

        return Result<SceneNodeID>::Ok(id);
    }

    void SceneSystem::CreateECSEntity(
        SceneNode& node, const SceneNodeDesc& desc
    ) {
        if (!context_) return;
        auto* ecs = context_->Get<IECS>();
        if (!ecs) return;

        EntityID eid = ecs->CreateEntity();
        node.SetEntityID(eid);
        RIFTCORE_UNUSED(desc);
    }

    void SceneSystem::CreatePhysicsBody(
        SceneNode& node, const SceneNodeDesc& desc
    ) {
        if (!context_) return;
        auto* physics = context_->Get<IPhysics>();
        if (!physics) return;

        RigidBodyDesc rbDesc;
        rbDesc.position  = desc.position;
        rbDesc.mass      = desc.physics.mass;
        rbDesc.isStatic  = desc.physics.isStatic;
        rbDesc.collider.restitution = desc.physics.restitution;
        rbDesc.collider.friction    = desc.physics.friction;

        if (desc.physics.colliderShape == "sphere") {
            rbDesc.collider.shape  = ColliderShape::Sphere;
            rbDesc.collider.radius = desc.physics.radius;
        } else if (desc.physics.colliderShape == "plane") {
            rbDesc.collider.shape       = ColliderShape::Plane;
            rbDesc.collider.planeNormal = {0,1,0};
            rbDesc.collider.planeOffset = desc.position.y;
        } else {
            rbDesc.collider.shape       = ColliderShape::Box;
            rbDesc.collider.halfExtents =
                desc.physics.halfExtents;
        }

        physics->AddRigidBody(node.GetEntityID(), rbDesc);
    }

    void SceneSystem::CreateAudioSource(
        SceneNode& node, const SceneNodeDesc& desc
    ) {
        if (!context_ || desc.audio.clipPath.empty()) return;
        auto* audio = context_->Get<IAudio>();
        if (!audio) return;

        AudioClipDesc clipDesc;
        clipDesc.filePath = desc.audio.clipPath;
        clipDesc.is3D     = desc.audio.is3D;

        auto clipResult = audio->LoadClip(clipDesc);
        if (clipResult.IsErr()) return;

        AudioSourceDesc srcDesc;
        srcDesc.clipID      = clipResult.Value();
        srcDesc.position    = desc.position;
        srcDesc.volume      = desc.audio.volume;
        srcDesc.looping     = desc.audio.looping;
        srcDesc.is3D        = desc.audio.is3D;
        srcDesc.playOnCreate= desc.audio.playOnStart;

        auto srcResult = audio->CreateSource(srcDesc);
        if (srcResult.IsOk()) {
            node.SetAudioSourceID(srcResult.Value());
        }
    }

    void SceneSystem::DestroyNode(SceneNodeID id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = nodes_.find(id);
        if (it == nodes_.end()) return;

        // Remove from parent
        SceneNodeID parentID = it->second->GetParentID();
        if (parentID != INVALID_NODE) {
            auto* parent = GetNodeRaw(parentID);
            if (parent) parent->RemoveChild(id);
        } else {
            rootNodes_.erase(
                std::remove(rootNodes_.begin(),
                            rootNodes_.end(), id),
                rootNodes_.end());
        }

        nodes_.erase(it);
    }

    ISceneNode* SceneSystem::GetNode(SceneNodeID id) {
        return GetNodeRaw(id);
    }

    SceneNode* SceneSystem::GetNodeRaw(SceneNodeID id) {
        auto it = nodes_.find(id);
        return it != nodes_.end()
            ? it->second.get() : nullptr;
    }

    ISceneNode* SceneSystem::FindNode(const String& name) {
        for (auto& [id, node] : nodes_) {
            if (node->GetName() == name) return node.get();
        }
        return nullptr;
    }

    void SceneSystem::ForEachNode(
        std::function<void(ISceneNode*)> fn
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [id, node] : nodes_) {
            fn(node.get());
        }
    }

    void SceneSystem::ForEachRootNode(
        std::function<void(ISceneNode*)> fn
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        for (SceneNodeID rootID : rootNodes_) {
            auto it = nodes_.find(rootID);
            if (it != nodes_.end()) {
                fn(it->second.get());
            }
        }
    }

    // ── Save scene to JSON ────────────────────────────────────
    VoidResult SceneSystem::SaveScene(const String& path) {
        json root;
        root["scene"]   = sceneName_;
        root["version"] = "1.0";
        root["nodes"]   = json::array();

        std::lock_guard<std::mutex> lock(mutex_);

        for (auto& [id, node] : nodes_) {
            json nodeJ;
            nodeJ["id"]       = id;
            nodeJ["name"]     = node->GetName();
            nodeJ["parentID"] = node->GetParentID();
            nodeJ["active"]   = node->IsActive();
            nodeJ["position"] = Vec3ToJson(
                node->GetLocalPosition());
            nodeJ["rotation"] = Vec3ToJson(
                node->GetLocalRotation());
            nodeJ["scale"]    = Vec3ToJson(
                node->GetLocalScale());

            // Mesh component
            if (node->hasMesh) {
                json meshJ;
                meshJ["path"]      = node->meshDesc.meshPath;
                meshJ["material"]  = node->meshDesc.materialName;
                meshJ["albedo"]    = Vec3ToJson(
                    node->meshDesc.albedo);
                meshJ["metallic"]  = node->meshDesc.metallic;
                meshJ["roughness"] = node->meshDesc.roughness;
                nodeJ["mesh"]      = meshJ;
            }

            // Physics component
            if (node->hasPhysics) {
                json physJ;
                physJ["isStatic"]    =
                    node->physicsDesc.isStatic;
                physJ["mass"]        =
                    node->physicsDesc.mass;
                physJ["restitution"] =
                    node->physicsDesc.restitution;
                physJ["friction"]    =
                    node->physicsDesc.friction;
                physJ["shape"]       =
                    node->physicsDesc.colliderShape;
                physJ["halfExtents"] = Vec3ToJson(
                    node->physicsDesc.halfExtents);
                physJ["radius"]      =
                    node->physicsDesc.radius;
                nodeJ["physics"]     = physJ;
            }

            // Audio component
            if (node->hasAudio) {
                json audioJ;
                audioJ["clipPath"] =
                    node->audioDesc.clipPath;
                audioJ["volume"]   =
                    node->audioDesc.volume;
                audioJ["looping"]  =
                    node->audioDesc.looping;
                audioJ["is3D"]     =
                    node->audioDesc.is3D;
                audioJ["playOnStart"] =
                    node->audioDesc.playOnStart;
                nodeJ["audio"]     = audioJ;
            }

            root["nodes"].push_back(nodeJ);
        }

        std::ofstream file(path);
        if (!file.is_open()) {
            return VoidResult::Err(
                "Cannot write scene file: " + path);
        }

        file << root.dump(4);  // 4-space indent
        file.close();

        sceneFilePath_ = path;

        if (logger_) {
            logger_->Info("Scene",
                "Scene saved: " + path +
                " (" + std::to_string(nodes_.size()) +
                " nodes)");
        }

        return VoidResult::Ok();
    }

    // ── Load scene from JSON ──────────────────────────────────
    VoidResult SceneSystem::LoadScene(const String& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return VoidResult::Err(
                "Cannot open scene file: " + path);
        }

        json root;
        try {
            file >> root;
        } catch (const json::exception& e) {
            return VoidResult::Err(
                "JSON parse error: " +
                std::string(e.what()));
        }
        file.close();

        // Clear existing scene
        ClearScene();

        sceneName_     = root.value("scene", "unnamed");
        sceneFilePath_ = path;

        if (logger_) {
            logger_->Info("Scene",
                "Loading scene: " + sceneName_);
        }

        if (!root.contains("nodes")) {
            return VoidResult::Ok();  // empty scene ok
        }

        // First pass: create all nodes
        // (parent might come after child in file)
        std::vector<SceneNodeDesc> pendingDescs;

        for (auto& nodeJ : root["nodes"]) {
            SceneNodeDesc desc;
            desc.name     = nodeJ.value("name","Node");
            desc.position = JsonToVec3(
                nodeJ.value("position",json::array()));
            desc.rotation = JsonToVec3(
                nodeJ.value("rotation",json::array()));
            desc.scale    = JsonToVec3(
                nodeJ.value("scale",
                    json::array({1,1,1})), Vec3::One());
            desc.parentID = nodeJ.value(
                "parentID", INVALID_NODE);

            // Mesh component
            if (nodeJ.contains("mesh")) {
                auto& meshJ   = nodeJ["mesh"];
                desc.hasMesh  = true;
                desc.mesh.meshPath     =
                    meshJ.value("path","");
                desc.mesh.materialName =
                    meshJ.value("material","");
                desc.mesh.albedo =
                    JsonToVec3(meshJ.value(
                        "albedo",json::array()),
                        Vec3::One());
                desc.mesh.metallic  =
                    meshJ.value("metallic",  0.0f);
                desc.mesh.roughness =
                    meshJ.value("roughness", 0.5f);
            }

            // Physics component
            if (nodeJ.contains("physics")) {
                auto& physJ      = nodeJ["physics"];
                desc.hasPhysics  = true;
                desc.physics.isStatic   =
                    physJ.value("isStatic", false);
                desc.physics.mass       =
                    physJ.value("mass",     1.0f);
                desc.physics.restitution=
                    physJ.value("restitution", 0.4f);
                desc.physics.friction   =
                    physJ.value("friction",    0.5f);
                desc.physics.colliderShape =
                    physJ.value("shape", "box");
                desc.physics.halfExtents =
                    JsonToVec3(physJ.value(
                        "halfExtents",json::array()),
                        Vec3::One() * 0.5f);
                desc.physics.radius =
                    physJ.value("radius", 0.5f);
            }

            // Audio component
            if (nodeJ.contains("audio")) {
                auto& audioJ     = nodeJ["audio"];
                desc.hasAudio    = true;
                desc.audio.clipPath =
                    audioJ.value("clipPath","");
                desc.audio.volume   =
                    audioJ.value("volume", 1.0f);
                desc.audio.looping  =
                    audioJ.value("looping", false);
                desc.audio.is3D     =
                    audioJ.value("is3D", false);
                desc.audio.playOnStart =
                    audioJ.value("playOnStart", false);
            }

            pendingDescs.push_back(desc);
        }

        // Create nodes in order
        // Simple approach: parentID=0 means root
        u32 created = 0;
        for (auto& desc : pendingDescs) {
            auto result = CreateNode(desc);
            if (result.IsOk()) created++;
        }

        if (logger_) {
            logger_->Info("Scene",
                "Scene loaded: " + sceneName_ +
                " (" + std::to_string(created) +
                " nodes)");
        }

        return VoidResult::Ok();
    }

    // ── SceneModule ───────────────────────────────────────────
    SceneModule::SceneModule()  = default;
    SceneModule::~SceneModule() = default;

    VoidResult SceneModule::Initialize(
        const ModuleInitParams& params
    ) {
        ILogger* log = nullptr;
        if (params.context) log = params.context->Logger();

        if (log) log->Info("Scene","Initializing...");

        scene_ = std::make_unique<SceneSystem>();
        scene_->SetContext(params.context);

        auto r = scene_->Initialize();
        if (r.IsErr()) return r;

        if (params.context) {
            params.context->Register<ISceneSystem>(
                scene_.get());
        }

        if (log) log->Info("Scene","Scene system ready.");
        return VoidResult::Ok();
    }

    void SceneModule::OnUpdate(f32 dt) {
        if (scene_) scene_->Update(dt);
    }

    void SceneModule::Shutdown() {
        std::cout << "[Scene] Shutting down...\n";
        if (scene_) scene_->Shutdown();
        scene_.reset();
        std::cout << "[Scene] Shutdown complete.\n";
    }

    ModuleDescriptor SceneModule::GetDescriptor() const {
        ModuleDescriptor d;
        d.name        = "Scene";
        d.version     = "0.1.0";
        d.apiVersion  = RIFTCORE_API_VERSION;
        d.description = "Scene graph + JSON save/load";
        return d;
    }

    RIFTCORE_IMPLEMENT_MODULE(SceneModule)

} // namespace RiftCore


