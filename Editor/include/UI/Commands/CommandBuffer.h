#pragma once
#include <string>
#include <vector>
#include <mutex>

namespace RiftCore::UI {
    enum class EditorCommandType {
        // Your existing ones
        SpawnEntity, 
        DeleteEntity,
        TransformUpdate,
        Play,
        Stop,
        Build,
        Save,
        Load,
        Export,
        Undo,
        Redo,
        Settings,
        ToggleNodes,
        Debug,     // Added for the Debug button
        Console,   // Added for the Console button
        Import,    // Added for the Import button
        Anims      // Added for the Anims button
    };

    struct EditorCommand {
        EditorCommandType Type;
        std::string Payload;
        void* DataPtr = nullptr;
    };

    class CommandBuffer {
    public:
        void Push(EditorCommand cmd) {
            std::lock_guard<std::mutex> lock(m_Mutex);
            m_Queue.push_back(std::move(cmd));
        }
        std::vector<EditorCommand> Flush() {
            std::lock_guard<std::mutex> lock(m_Mutex);
            return std::move(m_Queue);
        }
    private:
        std::vector<EditorCommand> m_Queue;
        std::mutex m_Mutex;
    };
}
