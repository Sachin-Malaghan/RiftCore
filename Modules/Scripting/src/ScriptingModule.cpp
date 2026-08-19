#include <Scripting/ScriptingModule.h>








namespace RiftCore {

    ScriptingModule::ScriptingModule(): m_scene(nullptr), m_logger(nullptr) 
    {
    
    }

    ScriptingModule::~ScriptingModule() {
        Shutdown();
    }

    VoidResult ScriptingModule::Initialize(const ModuleInitParams& params) {
        m_initialized = true;
        if (m_logger) {
            m_logger->Info("Scripting", "Module Online: ABI Safety Layer Active.");
        }
        return VoidResult::Ok();
    }

    void ScriptingModule::OnUpdate(f32 deltaTime) {
        if (!m_initialized) return;
        ProcessCommandQueue();
    }

    ModuleDescriptor ScriptingModule::GetDescriptor() const {
        ModuleDescriptor desc;
        desc.name = "Scripting";
        desc.version = "1.0.0";
        return desc;
    }

    void ScriptingModule::ProcessCommandQueue() {
        std::lock_guard<std::mutex> lock(m_queueMutex);

        while (!m_commandQueue.empty() && m_scene) {
            const auto& cmd = m_commandQueue.front();
            if (cmd.type == AutomationCommand::Type::SpawnNode) {
                m_scene->CreateNode(cmd.nodeDesc);
            }
            m_commandQueue.pop();
        }
    }

    VoidResult ScriptingModule::ExecuteString(const char* code) {
        if (!m_initialized) return VoidResult::Err("Module not initialized");
        if (!code) return VoidResult::Err("Code string is null");

        // Convert const char* to std::string safely inside the DLL's memory space
        std::string codeStr(code);

        if (codeStr == "RUN_SCENARIO_01") {
            std::lock_guard<std::mutex> lock(m_queueMutex);
            AutomationCommand spawn;
            spawn.type = AutomationCommand::Type::SpawnNode;
            spawn.nodeDesc.name = "Nuclear_Core_Alpha";
            m_commandQueue.push(spawn);

            if (m_logger) m_logger->Info("Scripting", "Scenario 01 queued.");
            return VoidResult::Ok();
        }

        return VoidResult::Err("Unknown Command");
    }

    void ScriptingModule::Shutdown() {
        m_initialized = false;
    }

    VoidResult ScriptingModule::LoadScript(const char* filePath) {
        return VoidResult::Ok();
    }

    void ScriptingModule::RegisterFunction(const char* name, void(*fn)()) {
        // We will implement raw function pointers later
    }

    // ── DLL EXPORTS ──────────────────────────────
    extern "C" {
        // Must take ZERO arguments to match PluginManager
        RIFTCORE_EXPORT IModule* CreateModule() {
            return new ScriptingModule();
        }

        RIFTCORE_EXPORT void DestroyModule(IModule* module) {
            if (module) delete module;
        }
    }
}
