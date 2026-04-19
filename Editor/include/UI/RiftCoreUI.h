#pragma once
#include <UI/Styling/ImGuiTheme.h>
#include <UI/Commands/CommandBuffer.h>
#include <memory>
#include <map>
#include <string>

struct GLFWwindow;

namespace RiftCore::UI {
    class RiftCoreUI {
    public:
        RiftCoreUI();
        ~RiftCoreUI();

        bool Initialize(GLFWwindow* window);
        void Shutdown();

        void BeginFrame();
        void EndFrame();

        void OnUIRender();

        CommandBuffer& GetCommandBuffer() { return m_CommandBuffer; }

    private:
        void RenderMainDockspace();
        void RenderTopToolbar();
        void RenderMenuBar();

        GLFWwindow* m_Window = nullptr;
        CommandBuffer m_CommandBuffer;
        bool m_EditorOpen = true;

        bool m_ShowStatsWindow = true;
        bool m_ShowAssetBrowser = true;
        bool m_ShowLogWindow = false;

    private:
        // Use unsigned int instead of GLuint to avoid header errors
        std::map<std::string, unsigned int> m_IconCache;
        unsigned int LoadTexture(const std::string& path);
    };
}