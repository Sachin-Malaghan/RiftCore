#pragma once
#include <string>

namespace RiftCore::UI {
    class AssetBrowserPanel {
    public:
        void OnUIRender();
    private:
        std::string m_CurrentDirectory = "Assets";
    };
}
