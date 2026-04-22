#pragma once
/**
 * @file RiftCoreUI.h
 * @brief DEPRECATED - Use <Renderer/HUD.h> instead
 * 
 * This header is kept for backward compatibility only.
 * All functionality has been consolidated into the HUD class.
 * 
 * @deprecated Use HUD class from <Renderer/HUD.h>
 * @see HUD
 */

#include <Renderer/HUD.h>
#include <imgui.h>

namespace RiftCore::UI {
    /**
     * @brief Must be called once after HUD::Initialize() from main.cpp.
     * Propagates the ImGui context from the Renderer DLL into this UI DLL
     * so all panels can call ImGui functions safely.
     */
    void InitPanelImGuiContext(ImGuiContext* ctx);
}

// Compatibility alias - RiftCoreUI is now just an alias for HUD
namespace RiftCore::UI {
    
    /**
     * @class RiftCoreUI
     * @deprecated Use RiftCore::HUD instead
     * 
     * This class is deprecated. The HUD class from <Renderer/HUD.h>
     * provides all the same functionality with additional features:
     * - Docking support
     * - Menu bar and toolbar
     * - CommandBuffer for action dispatch
     * - Panel visibility management
     * - Icon loading system
     */
    using RiftCoreUI = ::RiftCore::HUD;
    
    // Re-export types that were previously in this namespace
    using HUDConfig = ::RiftCore::HUDConfig;
    using HUDCallbacks = ::RiftCore::HUDCallbacks;
    
    // Re-export CommandBuffer types for convenience
    using CommandBuffer = ::RiftCore::CommandBuffer;
    using EditorCommandType = ::RiftCore::EditorCommandType;
    using EditorCommand = ::RiftCore::EditorCommand;
}
