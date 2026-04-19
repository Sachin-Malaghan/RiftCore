#pragma once
#include <vector>
#include <string>

namespace RiftCore::UI {
    class ConsolePanel {
    public:
        void AddLog(const std::string& msg);
        void OnUIRender();
    private:
        std::vector<std::string> m_Logs;
    };
}
