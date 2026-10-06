#pragma warning(disable: 4190)

#include <Scene/SceneSystem.h>
#include <RiftCore/Core/ILogger.h>
#include <RiftCore/Physics/IPhysics.h>
#include <RiftCore/Audio/IAudio.h>
#include <RiftCore/ECS/IECS.h>

#include <json.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>

namespace RiftCore {

    using json = nlohmann::json;

    // -- JSON helpers --------------------------------------------

    static json Vec3ToJson(const Vec3& v) {
        return json::array({v.x, v.y, v.z});
    }

    static Vec3 JsonToVec3(const json& j, Vec3 def = Vec3::Zero()) {
        if (!j.is_array() || j.size() < 3) return def;
        return {j[0].get<float>(), j[1].get<float>(), j[2].get<float>()};
    }

    static json DescToJson(const SceneNodeDesc& d, SceneNodeID id, bool active) {
        json n;
        n["id"]       = id;
        n["name"]     = d.name;
        n["parentID"] = d.parentID;
        n["active"]   = active;
        n["position"] = Vec3ToJson(d.position);
        n["rotation"] = Vec3ToJson(d.rotation);
        n["scale"]    = Vec3ToJson(d.scale);
        if (d.hasMesh) {
            json m;
            m["path"]      = d.mesh.meshPath;
            m["material"]  = d.mesh.materialName;
            m["albedo"]    = Vec3ToJson(d.mesh.albedo);
            m["metallic"]  = d.mesh.metallic;
            m["roughness"] = d.mesh.roughness;
            n["mesh"]      = m;
        }
        if (d.hasPhysics) {
            json p;
            p["isStatic"]    = d.physics.isStatic;
            p["mass"]        = d.physics.mass;
            p["restitution"] = d.physics.restitution;
            p["friction"]    = d.physics.friction;
            p["shape"]       = d.physics.colliderShape;
            p["halfExtents"] = Vec3ToJson(d.physics.halfExtents);
            p["radius"]      = d.physics.radius;
            n["physics"]     = p;
        }
        if (d.hasAudio) {
            json a;
            a["clipPath"]    = d.audio.clipPath;
            a["volume"]      = d.audio.volume;
            a["looping"]     = d.audio.looping;
            a["is3D"]        = d.audio.is3D;
            a["playOnStart"] = d.audio.playOnStart;
            n["audio"]       = a;
        }
        if (d.hasLight) {
            json l;
            l["type"]      = d.light.type;
            l["color"]     = Vec3ToJson(d.light.color);
            l["intensity"] = d.light.intensity;
            l["range"]     = d.light.range;
            n["light"]     = l;
        }
        return n;
    }

    static SceneNodeDesc JsonToDesc(const json& n) {
        SceneNodeDesc d;
        d.name     = n.value("name", "Node");
        d.position = JsonToVec3(n.value("position", json::array()));
        d.rotation = JsonToVec3(n.value("rotation", json::array()));
        d.scale    = JsonToVec3(n.value("scale", json::array({1, 1, 1})), Vec3::One());
        d.parentID = n.value("parentID", INVALID_NODE);

        if (n.contains("mesh")) {
            auto& m = n["mesh"];
            d.hasMesh           = true;
            d.mesh.meshPath     = m.value("path", "");
            d.mesh.materialName = m.value("material", "");
            d.mesh.albedo       = JsonToVec3(m.value("albedo", json::array()), Vec3::One());
            d.mesh.metallic     = m.value("metallic", 0.0f);
            d.mesh.roughness    = m.value("roughness", 0.5f);
        }
        if (n.contains("physics")) {
            auto& p = n["physics"];
            d.hasPhysics            = true;
            d.physics.isStatic      = p.value("isStatic", false);
            d.physics.mass          = p.value("mass", 1.0f);
            d.physics.restitution   = p.value("restitution", 0.4f);
            d.physics.friction      = p.value("friction", 0.5f);
            d.physics.colliderShape = p.value("shape", "box");
            d.physics.halfExtents   = JsonToVec3(p.value("halfExtents", json::array()),
                                                 Vec3::One() * 0.5f);
            d.physics.radius        = p.value("radius", 0.5f);
        }
        if (n.contains("audio")) {
            auto& a = n["audio"];
            d.hasAudio          = true;
            d.audio.clipPath    = a.value("clipPath", "");
            d.audio.volume      = a.value("volume", 1.0f);
            d.audio.looping     = a.value("looping", false);
            d.audio.is3D        = a.value("is3D", false);
            d.audio.playOnStart = a.value("playOnStart", false);
        }
        if (n.contains("light")) {
            auto& l = n["light"];
            d.hasLight        = true;
            d.light.type      = l.value("type", "directional");
            d.light.color     = JsonToVec3(l.value("color", json::array()), Vec3::One());
            d.light.intensity = l.value("intensity", 1.0f);
            d.light.range     = l.value("range", 20.0f);
        }

        // Scenes saved before the model library had no mesh path: the
        // collider decided what was drawn. Give those nodes a real mesh
        // and bake the collider size into the scale.
        if (d.hasMesh && d.mesh.meshPath.empty()) {
            const String& shape = d.physics.colliderShape;
            if (d.hasPhysics && shape == "sphere") {
                d.mesh.meshPath = "primitive:sphere";
                f32 s = d.physics.radius * 2.0f;
                d.scale = { d.scale.x * s, d.scale.y * s, d.scale.z * s };
            } else if (d.hasPhysics && shape == "plane") {
                d.mesh.meshPath = "primitive:plane";
                d.scale = { d.physics.halfExtents.x * 2.0f, 1.0f,
                            d.physics.halfExtents.z * 2.0f };
            } else if (d.hasPhysics) {
                d.mesh.meshPath = "primitive:cube";
                d.scale = { d.scale.x * d.physics.halfExtents.x * 2.0f,
                            d.scale.y * d.physics.halfExtents.y * 2.0f,
                            d.scale.z * d.physics.halfExtents.z * 2.0f };
            } else {
                d.mesh.meshPath = "primitive:cube";
            }
        }
        return d;
    }

    static SceneNodeDesc NodeToDesc(const SceneNode& node) {
        SceneNodeDesc d;
        d.name       = node.GetName();
        d.position   = node.GetLocalPosition();
        d.rotation   = node.GetLocalRotation();
        d.scale      = node.GetLocalScale();
        d.parentID   = node.GetParentID();
        d.hasMesh    = node.hasMesh;     d.mesh    = node.meshDesc;
        d.hasPhysics = node.hasPhysics;  d.physics = node.physicsDesc;
        d.hasAudio   = node.hasAudio;    d.audio   = node.audioDesc;
        d.hasLight   = node.hasLight;    d.light   = node.lightDesc;
        return d;
    }

    // -- SceneSystem ---------------------------------------------

    SceneSystem::SceneSystem()  = default;
    SceneSystem::~SceneSystem() { Shutdown(); }

    VoidResult SceneSystem::Initialize() {
        if (context_) logger_ = context_->Logger();
        if (logger_)  logger_->Info("Scene", "Scene system initialized.");
        return VoidResult::Ok();
    }

    void SceneSystem::Shutdown() {
        ClearScene();
    }

    // Keeps scene nodes and physics bodies in step: a node moved by the
    // editor or a script teleports its body; otherwise the body drives
    // the node.
    void SceneSystem::Update(f32 deltaTime) {
        RIFTCORE_UNUSED(deltaTime);
        if (!context_) return;
        auto* physics = context_->Get<IPhysics>();
        if (!physics) return;

        std::lock_guard<std::recursive_mutex> lock(mutex_);
        for (auto& [id, node] : nodes_) {
            if (!node->hasPhysics) { node->ClearDirty(); continue; }
            EntityID e = node->GetEntityID();
            if (node->IsDirty()) {
                if (node->physicsDesc.colliderShape == "plane") {
                    // A plane's height is part of its shape: rebuild it.
                    physics->RemoveRigidBody(e);
                    CreatePhysicsBody(*node, NodeToDesc(*node));
                } else {
                    physics->SetBodyTransform(
                        e, node->GetLocalPosition(), node->GetLocalRotation());
                }
                node->ClearDirty();
            } else if (!node->physicsDesc.isStatic) {
                Vec3 p, r;
                if (physics->GetBodyTransform(e, p, r)) {
                    node->SyncFromPhysics(p, r);
                }
            }
        }
    }

    // -- Scene management ----------------------------------------

    VoidResult SceneSystem::NewScene(const String& name) {
        ClearScene();
        sceneName_     = name;
        sceneFilePath_ = "";
        if (logger_) logger_->Info("Scene", "New scene created: " + name);
        return VoidResult::Ok();
    }

    void SceneSystem::ReleaseNodeResources(SceneNode& node) {
        if (!context_) return;
        if (node.hasPhysics) {
            if (auto* physics = context_->Get<IPhysics>()) {
                physics->RemoveRigidBody(node.GetEntityID());
            }
        }
        if (node.GetAudioSourceID() != 0) {
            if (auto* audio = context_->Get<IAudio>()) {
                audio->DestroySource(node.GetAudioSourceID());
            }
            node.SetAudioSourceID(0);
        }
    }

    void SceneSystem::ClearScene() {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        for (auto& [id, node] : nodes_) {
            ReleaseNodeResources(*node);
            if (context_ && node->ownsEcsEntity) {
                if (auto* ecs = context_->Get<IECS>()) {
                    ecs->DestroyEntity(node->GetEntityID());
                }
            }
        }
        nodes_.clear();
        rootNodes_.clear();
        sceneName_     = "";
        sceneFilePath_ = "";
    }

    SceneInfo SceneSystem::GetSceneInfo() const {
        SceneInfo info;
        info.name      = sceneName_;
        info.filePath  = sceneFilePath_;
        info.nodeCount = static_cast<u32>(nodes_.size());
        info.isLoaded  = !sceneName_.empty();
        return info;
    }

    // -- Node management -----------------------------------------

    Result<SceneNodeID> SceneSystem::CreateNode(const SceneNodeDesc& desc) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);

        SceneNodeID id = nextNodeID_.fetch_add(1);
        auto node = std::make_unique<SceneNode>(
            id, desc.name.empty() ? "Node_" + std::to_string(id) : desc.name);

        node->SetLocalPosition(desc.position);
        node->SetLocalRotation(desc.rotation);
        node->SetLocalScale   (desc.scale);

        node->hasMesh    = desc.hasMesh;     node->meshDesc    = desc.mesh;
        node->hasPhysics = desc.hasPhysics;  node->physicsDesc = desc.physics;
        node->hasAudio   = desc.hasAudio;    node->audioDesc   = desc.audio;
        node->hasLight   = desc.hasLight;    node->lightDesc   = desc.light;

        SceneNode* parent = desc.parentID != INVALID_NODE
            ? GetNodeRaw(desc.parentID) : nullptr;
        if (parent) {
            node->SetParentID(desc.parentID);
            node->SetParentNode(parent);
            parent->AddChild(id);
        } else {
            rootNodes_.push_back(id);
        }

        CreateECSEntity(*node, desc);
        if (desc.hasPhysics) CreatePhysicsBody(*node, desc);
        if (desc.hasAudio)   CreateAudioSource(*node, desc);

        nodes_[id] = std::move(node);
        return Result<SceneNodeID>::Ok(id);
    }

    void SceneSystem::CreateECSEntity(SceneNode& node, const SceneNodeDesc& desc) {
        RIFTCORE_UNUSED(desc);
        IECS* ecs = context_ ? context_->Get<IECS>() : nullptr;
        if (ecs) {
            node.SetEntityID(ecs->CreateEntity());
            node.ownsEcsEntity = true;
        } else {
            // No ECS module loaded (editor, headless tools): the node id
            // still has to be a unique key for the physics body.
            node.SetEntityID(0x8000000000000000ull | node.GetID());
        }
    }

    void SceneSystem::CreatePhysicsBody(SceneNode& node, const SceneNodeDesc& desc) {
        if (!context_) return;
        auto* physics = context_->Get<IPhysics>();
        if (!physics) return;

        RigidBodyDesc rb;
        rb.position             = desc.position;
        rb.mass                 = desc.physics.mass;
        rb.isStatic             = desc.physics.isStatic;
        rb.collider.restitution = desc.physics.restitution;
        rb.collider.friction    = desc.physics.friction;

        if (desc.physics.colliderShape == "sphere") {
            rb.collider.shape  = ColliderShape::Sphere;
            rb.collider.radius = desc.physics.radius;
        } else if (desc.physics.colliderShape == "plane") {
            rb.collider.shape       = ColliderShape::Plane;
            rb.collider.planeNormal = {0, 1, 0};
            rb.collider.planeOffset = desc.position.y;
            rb.isStatic             = true;
        } else if (desc.physics.colliderShape == "capsule") {
            rb.collider.shape       = ColliderShape::Capsule;
            rb.collider.radius      = desc.physics.radius;
            rb.collider.halfExtents = desc.physics.halfExtents;
        } else {
            rb.collider.shape       = ColliderShape::Box;
            rb.collider.halfExtents = desc.physics.halfExtents;
        }

        physics->AddRigidBody(node.GetEntityID(), rb);
        physics->SetBodyTransform(node.GetEntityID(), desc.position, desc.rotation);
    }

    void SceneSystem::CreateAudioSource(SceneNode& node, const SceneNodeDesc& desc) {
        if (!context_ || desc.audio.clipPath.empty()) return;
        auto* audio = context_->Get<IAudio>();
        if (!audio) return;

        AudioClipDesc clipDesc;
        clipDesc.filePath = desc.audio.clipPath;
        clipDesc.is3D     = desc.audio.is3D;
        auto clipResult = audio->LoadClip(clipDesc);
        if (clipResult.IsErr()) return;

        AudioSourceDesc srcDesc;
        srcDesc.clipID       = clipResult.Value();
        srcDesc.position     = desc.position;
        srcDesc.volume       = desc.audio.volume;
        srcDesc.looping      = desc.audio.looping;
        srcDesc.is3D         = desc.audio.is3D;
        srcDesc.playOnCreate = desc.audio.playOnStart;

        auto srcResult = audio->CreateSource(srcDesc);
        if (srcResult.IsOk()) node.SetAudioSourceID(srcResult.Value());
    }

    void SceneSystem::DestroyNode(SceneNodeID id) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        auto it = nodes_.find(id);
        if (it == nodes_.end()) return;

        // Children go with their parent.
        std::vector<SceneNodeID> children = it->second->GetChildren();
        for (SceneNodeID child : children) DestroyNode(child);

        it = nodes_.find(id);
        if (it == nodes_.end()) return;
        SceneNode& node = *it->second;

        ReleaseNodeResources(node);
        if (context_ && node.ownsEcsEntity) {
            if (auto* ecs = context_->Get<IECS>()) ecs->DestroyEntity(node.GetEntityID());
        }

        SceneNodeID parentID = node.GetParentID();
        if (SceneNode* parent = GetNodeRaw(parentID)) {
            parent->RemoveChild(id);
        }
        rootNodes_.erase(std::remove(rootNodes_.begin(), rootNodes_.end(), id),
                         rootNodes_.end());
        nodes_.erase(it);
    }

    ISceneNode* SceneSystem::GetNode(SceneNodeID id) {
        return GetNodeRaw(id);
    }

    SceneNode* SceneSystem::GetNodeRaw(SceneNodeID id) {
        auto it = nodes_.find(id);
        return it != nodes_.end() ? it->second.get() : nullptr;
    }

    ISceneNode* SceneSystem::FindNode(const String& name) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        for (auto& [id, node] : nodes_) {
            if (node->GetName() == name) return node.get();
        }
        return nullptr;
    }

    // Iteration is in creation order and works on a snapshot of the ids,
    // so the callback may create or destroy nodes.
    void SceneSystem::ForEachNode(std::function<void(ISceneNode*)> fn) {
        std::vector<SceneNodeID> ids;
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            ids.reserve(nodes_.size());
            for (auto& [id, node] : nodes_) ids.push_back(id);
        }
        std::sort(ids.begin(), ids.end());
        for (SceneNodeID id : ids) {
            if (SceneNode* n = GetNodeRaw(id)) fn(n);
        }
    }

    void SceneSystem::ForEachRootNode(std::function<void(ISceneNode*)> fn) {
        std::vector<SceneNodeID> ids;
        {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            ids = rootNodes_;
        }
        for (SceneNodeID id : ids) {
            if (SceneNode* n = GetNodeRaw(id)) fn(n);
        }
    }

    bool SceneSystem::GetNodeDesc(SceneNodeID id, SceneNodeDesc& out) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        SceneNode* node = GetNodeRaw(id);
        if (!node) return false;
        out = NodeToDesc(*node);
        return true;
    }

    bool SceneSystem::UpdateNode(SceneNodeID id, const SceneNodeDesc& d) {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        SceneNode* node = GetNodeRaw(id);
        if (!node) return false;

        ReleaseNodeResources(*node);

        if (!d.name.empty()) node->SetName(d.name);
        node->SetLocalPosition(d.position);
        node->SetLocalRotation(d.rotation);
        node->SetLocalScale   (d.scale);

        node->hasMesh    = d.hasMesh;     node->meshDesc    = d.mesh;
        node->hasPhysics = d.hasPhysics;  node->physicsDesc = d.physics;
        node->hasAudio   = d.hasAudio;    node->audioDesc   = d.audio;
        node->hasLight   = d.hasLight;    node->lightDesc   = d.light;

        if (d.hasPhysics) CreatePhysicsBody(*node, d);
        if (d.hasAudio)   CreateAudioSource(*node, d);
        return true;
    }

    // -- Serialization -------------------------------------------

    String SceneSystem::SerializeScene() {
        std::lock_guard<std::recursive_mutex> lock(mutex_);
        json root;
        root["scene"]   = sceneName_;
        root["version"] = "1.1";
        root["nodes"]   = json::array();

        std::vector<SceneNodeID> ids;
        for (auto& [id, node] : nodes_) ids.push_back(id);
        std::sort(ids.begin(), ids.end());
        for (SceneNodeID id : ids) {
            SceneNode* node = GetNodeRaw(id);
            root["nodes"].push_back(DescToJson(NodeToDesc(*node), id, node->IsActive()));
        }
        return root.dump(4);
    }

    VoidResult SceneSystem::DeserializeScene(const String& jsonText) {
        json root = json::parse(jsonText, nullptr, false);
        if (root.is_discarded() || !root.is_object()) {
            return VoidResult::Err("Scene JSON parse error");
        }

        String keepPath = sceneFilePath_;
        ClearScene();
        sceneName_     = root.value("scene", "unnamed");
        sceneFilePath_ = keepPath;
        if (!root.contains("nodes") || !root["nodes"].is_array()) {
            return VoidResult::Ok();
        }

        struct Pending { SceneNodeID fileID; bool active; SceneNodeDesc desc; bool done; };
        std::vector<Pending> pending;
        try {
            for (auto& n : root["nodes"]) {
                pending.push_back({ n.value("id", INVALID_NODE),
                                    n.value("active", true), JsonToDesc(n), false });
            }
        } catch (const json::exception& e) {
            return VoidResult::Err(String("Scene JSON error: ") + e.what());
        }

        // Node ids are reassigned on load, so parents are created first
        // and children are pointed at the new ids.
        std::unordered_map<SceneNodeID, SceneNodeID> remap;
        bool progress = true;
        while (progress) {
            progress = false;
            for (auto& p : pending) {
                if (p.done) continue;
                SceneNodeID fileParent = p.desc.parentID;
                if (fileParent != INVALID_NODE) {
                    auto it = remap.find(fileParent);
                    if (it == remap.end()) continue;   // parent not created yet
                    p.desc.parentID = it->second;
                }
                auto r = CreateNode(p.desc);
                if (r.IsOk()) {
                    if (p.fileID != INVALID_NODE) remap[p.fileID] = r.Value();
                    if (SceneNode* n = GetNodeRaw(r.Value())) n->SetActive(p.active);
                }
                p.done   = true;
                progress = true;
            }
        }
        // Anything left points at a parent that does not exist: keep it as a root.
        for (auto& p : pending) {
            if (p.done) continue;
            p.desc.parentID = INVALID_NODE;
            CreateNode(p.desc);
        }
        return VoidResult::Ok();
    }

    VoidResult SceneSystem::SaveScene(const String& path) {
        std::ofstream file(path);
        if (!file.is_open()) {
            return VoidResult::Err("Cannot write scene file: " + path);
        }
        file << SerializeScene();
        file.close();
        sceneFilePath_ = path;
        if (logger_) {
            logger_->Info("Scene", "Scene saved: " + path + " (" +
                std::to_string(nodes_.size()) + " nodes)");
        }
        return VoidResult::Ok();
    }

    VoidResult SceneSystem::LoadScene(const String& path) {
        std::ifstream file(path);
        if (!file.is_open()) {
            return VoidResult::Err("Cannot open scene file: " + path);
        }
        std::stringstream ss;
        ss << file.rdbuf();
        file.close();

        auto r = DeserializeScene(ss.str());
        if (r.IsErr()) return r;
        sceneFilePath_ = path;
        if (logger_) {
            logger_->Info("Scene", "Scene loaded: " + sceneName_ + " (" +
                std::to_string(nodes_.size()) + " nodes)");
        }
        return VoidResult::Ok();
    }

    // -- SceneModule ---------------------------------------------

    SceneModule::SceneModule()  = default;
    SceneModule::~SceneModule() = default;

    VoidResult SceneModule::Initialize(const ModuleInitParams& params) {
        ILogger* log = params.context ? params.context->Logger() : nullptr;
        if (log) log->Info("Scene", "Initializing...");

        scene_ = std::make_unique<SceneSystem>();
        scene_->SetContext(params.context);
        auto r = scene_->Initialize();
        if (r.IsErr()) return r;

        if (params.context) {
            params.context->Register<ISceneSystem>(scene_.get());
        }
        if (log) log->Info("Scene", "Scene system ready.");
        return VoidResult::Ok();
    }

    void SceneModule::OnUpdate(f32 dt) {
        if (scene_) scene_->Update(dt);
    }

    void SceneModule::Shutdown() {
        if (scene_) scene_->Shutdown();
        scene_.reset();
    }

    ModuleDescriptor SceneModule::GetDescriptor() const {
        ModuleDescriptor d;
        d.name        = "Scene";
        d.version     = "0.2.0";
        d.apiVersion  = RIFTCORE_API_VERSION;
        d.description = "Scene graph + JSON save/load";
        return d;
    }

    RIFTCORE_IMPLEMENT_MODULE(SceneModule)

} // namespace RiftCore
