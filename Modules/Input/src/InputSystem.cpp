#include <Input/InputSystem.h>
#include <RiftCore/Common/EngineContext.h>
#include <RiftCore/Core/ILogger.h>
#include <RiftCore/RHI/IRHIDevice.h>

// GLAD before GLFW
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <iostream>
#include <algorithm>

namespace RiftCore {

    // ── GLFW Static Callbacks ─────────────────────────────────
    // GLFW cannot call member functions directly
    // We use glfwSetWindowUserPointer to pass our InputSystem*
    // then cast it back inside the callback

    static void CB_Key(GLFWwindow* w, i32 key,
                       i32 scancode, i32 action, i32 mods) {
        RIFTCORE_UNUSED(scancode);
        auto* sys = static_cast<InputSystem*>(
            glfwGetWindowUserPointer(w));
        if (sys) sys->OnKey(key, action, mods);
    }

    static void CB_MouseButton(GLFWwindow* w,
                               i32 button, i32 action, i32 mods) {
        auto* sys = static_cast<InputSystem*>(
            glfwGetWindowUserPointer(w));
        if (sys) sys->OnMouseButton(button, action, mods);
    }

    static void CB_MouseMove(GLFWwindow* w, f64 x, f64 y) {
        auto* sys = static_cast<InputSystem*>(
            glfwGetWindowUserPointer(w));
        if (sys) sys->OnMouseMove(x, y);
    }

    static void CB_Scroll(GLFWwindow* w,
                          f64 xOffset, f64 yOffset) {
        auto* sys = static_cast<InputSystem*>(
            glfwGetWindowUserPointer(w));
        if (sys) sys->OnMouseScroll(xOffset, yOffset);
    }

    // ── InputSystem ───────────────────────────────────────────
    InputSystem::InputSystem() {
        keyStates_.fill(KeyState::Up);
        mouseStates_.fill(KeyState::Up);
    }

    InputSystem::~InputSystem() = default;

    VoidResult InputSystem::AttachToWindow(
        GLFWwindow* window,
        IEventBus*  eventBus
    ) {
        if (!window) {
            return VoidResult::Err("Window is null");
        }

        window_   = window;
        eventBus_ = eventBus;

        // Store our InputSystem* in GLFW window user pointer
        glfwSetWindowUserPointer(window_, this);

        // Register GLFW callbacks
        glfwSetKeyCallback         (window_, CB_Key);
        glfwSetMouseButtonCallback (window_, CB_MouseButton);
        glfwSetCursorPosCallback   (window_, CB_MouseMove);
        glfwSetScrollCallback      (window_, CB_Scroll);

        // Get initial mouse position
        f64 mx = 0, my = 0;
        glfwGetCursorPos(window_, &mx, &my);
        mousePos_     = { static_cast<f32>(mx),
                          static_cast<f32>(my) };
        lastMousePos_ = mousePos_;

        std::cout << "[Input] Attached to window.\n";
        return VoidResult::Ok();
    }

    void InputSystem::Update() {
        std::lock_guard<std::mutex> lock(mutex_);

        // Transition states from frame to frame:
        // Pressed  → Down     (was just pressed, now held)
        // Released → Up       (was just released, now up)
        for (auto& state : keyStates_) {
            if (state == KeyState::Pressed)  state = KeyState::Down;
            if (state == KeyState::Released) state = KeyState::Up;
        }
        for (auto& state : mouseStates_) {
            if (state == KeyState::Pressed)  state = KeyState::Down;
            if (state == KeyState::Released) state = KeyState::Up;
        }

        // Calculate delta for this frame
        mouseDelta_ = {
            mousePos_.x - lastMousePos_.x,
            mousePos_.y - lastMousePos_.y
        };
        lastMousePos_ = mousePos_;

        // Reset scroll - only valid for the frame it happened
        scrollY_ = 0.0f;
    }

    // ── Keyboard queries ──────────────────────────────────────
    bool InputSystem::IsValidKey(Key key) const {
        i32 k = static_cast<i32>(key);
        return k >= 0 && k < KEY_COUNT;
    }

    bool InputSystem::IsKeyDown(Key key) const {
        if (!IsValidKey(key)) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        auto s = keyStates_[static_cast<i32>(key)];
        return s == KeyState::Down || s == KeyState::Pressed;
    }

    bool InputSystem::IsKeyPressed(Key key) const {
        if (!IsValidKey(key)) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        return keyStates_[static_cast<i32>(key)]
               == KeyState::Pressed;
    }

    bool InputSystem::IsKeyReleased(Key key) const {
        if (!IsValidKey(key)) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        return keyStates_[static_cast<i32>(key)]
               == KeyState::Released;
    }

    // ── Mouse queries ─────────────────────────────────────────
    bool InputSystem::IsValidMouseButton(MouseButton btn) const {
        i32 b = static_cast<i32>(btn);
        return b >= 0 && b < MOUSE_COUNT;
    }

    Vec2 InputSystem::GetMousePosition() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return mousePos_;
    }

    Vec2 InputSystem::GetMouseDelta() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return mouseDelta_;
    }

    f32 InputSystem::GetMouseScrollY() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return scrollY_;
    }

    bool InputSystem::IsMouseDown(MouseButton btn) const {
        if (!IsValidMouseButton(btn)) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        auto s = mouseStates_[static_cast<i32>(btn)];
        return s == KeyState::Down || s == KeyState::Pressed;
    }

    bool InputSystem::IsMousePressed(MouseButton btn) const {
        if (!IsValidMouseButton(btn)) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        return mouseStates_[static_cast<i32>(btn)]
               == KeyState::Pressed;
    }

    bool InputSystem::IsMouseReleased(MouseButton btn) const {
        if (!IsValidMouseButton(btn)) return false;
        std::lock_guard<std::mutex> lock(mutex_);
        return mouseStates_[static_cast<i32>(btn)]
               == KeyState::Released;
    }

    void InputSystem::SetCursorVisible(bool visible) {
        if (!window_) return;
        glfwSetInputMode(window_, GLFW_CURSOR,
            visible ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_HIDDEN);
    }

    void InputSystem::SetCursorLocked(bool locked) {
        if (!window_) return;
        cursorLocked_ = locked;
        glfwSetInputMode(window_, GLFW_CURSOR,
            locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        if (locked) {
            firstMouse_ = true;
        }
    }

    bool InputSystem::IsCursorLocked() const {
        return cursorLocked_;
    }

    // ── GLFW Callbacks ────────────────────────────────────────
    void InputSystem::OnKey(i32 key, i32 action, i32 mods) {
        RIFTCORE_UNUSED(mods);
        if (key < 0 || key >= KEY_COUNT) return;

        std::lock_guard<std::mutex> lock(mutex_);

        if (action == GLFW_PRESS) {
            keyStates_[key] = KeyState::Pressed;

            // Publish event to bus (outside lock to avoid deadlock)
            if (eventBus_) {
                KeyPressedEvent ev;
                ev.key    = static_cast<Key>(key);
                ev.repeat = false;
                // Must unlock before publishing to avoid deadlock
                // with event handlers that query input
                // We use a local copy approach
            }
        }
        else if (action == GLFW_RELEASE) {
            keyStates_[key] = KeyState::Released;
        }
        else if (action == GLFW_REPEAT) {
            // Keep Down state, publish repeat event
            keyStates_[key] = KeyState::Down;
        }
    }

    void InputSystem::OnMouseButton(
        i32 button, i32 action, i32 mods
    ) {
        RIFTCORE_UNUSED(mods);
        if (button < 0 || button >= MOUSE_COUNT) return;

        std::lock_guard<std::mutex> lock(mutex_);

        if (action == GLFW_PRESS) {
            mouseStates_[button] = KeyState::Pressed;
        }
        else if (action == GLFW_RELEASE) {
            mouseStates_[button] = KeyState::Released;
        }
    }

    void InputSystem::OnMouseMove(f64 x, f64 y) {
        std::lock_guard<std::mutex> lock(mutex_);

        f32 fx = static_cast<f32>(x);
        f32 fy = static_cast<f32>(y);

        if (firstMouse_) {
            lastMousePos_ = { fx, fy };
            firstMouse_   = false;
        }

        mousePos_ = { fx, fy };
    }

    void InputSystem::OnMouseScroll(f64 xOffset, f64 yOffset) {
        RIFTCORE_UNUSED(xOffset);
        std::lock_guard<std::mutex> lock(mutex_);
        scrollY_ += static_cast<f32>(yOffset);
    }

    // ── InputModule ───────────────────────────────────────────
    InputModule::InputModule()  = default;
    InputModule::~InputModule() = default;

    VoidResult InputModule::Initialize(
        const ModuleInitParams& params
    ) {
        ILogger* logger = nullptr;
        if (params.context) {
            logger = params.context->Logger();
        }

        if (logger) logger->Info("Input", "Initializing...");

        input_ = std::make_unique<InputSystem>();

        // Get GLFWwindow from the RHI device
        // The OpenGLBackend DLL created the window
        // We get it through the IRHI → IRHIDevice → GLDevice
        GLFWwindow* window = nullptr;

        if (params.context) {
            auto* rhi = params.context->RHI();
            if (rhi) {
                // We know it's our OpenGL RHI
                // The device was already created in the demo
                // For now we find the window via GLFW
                window = glfwGetCurrentContext();
            }
        }

        if (window) {
            auto* bus = params.context
                ? params.context->EventBus() : nullptr;

            auto r = input_->AttachToWindow(window, bus);
            if (r.IsErr()) {
                if (logger) logger->Warning("Input",
                    "Could not attach to window: " +
                    r.Error().message);
            }
        } else {
            if (logger) logger->Warning("Input",
                "No GLFW window found - input disabled");
        }

        if (params.context) {
            params.context->Register<IInput>(input_.get());
        }

        if (logger) logger->Info("Input", "Input system ready.");

        return VoidResult::Ok();
    }

    void InputModule::OnUpdate(f32 deltaTime) {
        // Input::Update() is called manually by the application
        // each frame AFTER glfwPollEvents() and BEFORE key checks.
        // Do NOT call it here to avoid double-update.
        RIFTCORE_UNUSED(deltaTime);
    }

    void InputModule::Shutdown() {
        std::cout << "[Input] Shutting down...\n";
        input_.reset();
        std::cout << "[Input] Shutdown complete.\n";
    }

    ModuleDescriptor InputModule::GetDescriptor() const {
        ModuleDescriptor desc;
        desc.name        = "Input";
        desc.version     = "0.1.0";
        desc.apiVersion  = RIFTCORE_API_VERSION;
        desc.description = "Keyboard + Mouse Input System";
        return desc;
    }

    RIFTCORE_IMPLEMENT_MODULE(InputModule)

} // namespace RiftCore

