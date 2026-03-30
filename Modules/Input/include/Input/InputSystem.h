#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>
#include <RiftCore/Core/IModule.h>
#include <RiftCore/Core/IEventBus.h>
#include <RiftCore/Input/IInput.h>

#include <array>
#include <memory>
#include <atomic>
#include <mutex>

// Forward declare GLFW to avoid including it in header
struct GLFWwindow;

#ifdef INPUT_EXPORTS
    #define INPUT_API RIFTCORE_EXPORT
#else
    #define INPUT_API RIFTCORE_IMPORT
#endif

namespace RiftCore {

    // Key state: tracks current + previous frame
    enum class KeyState : u8 {
        Up       = 0,   // not pressed
        Pressed  = 1,   // just pressed this frame
        Down     = 2,   // held down
        Released = 3    // just released this frame
    };

    class INPUT_API InputSystem : public IInput {
    public:
        InputSystem();
        ~InputSystem();

        // Called by module to attach to GLFW window
        VoidResult AttachToWindow(
            GLFWwindow* window,
            IEventBus*  eventBus
        );

        // IInput interface
        void Update() override;

        bool IsKeyDown    (Key key)         const override;
        bool IsKeyPressed (Key key)         const override;
        bool IsKeyReleased(Key key)         const override;

        Vec2 GetMousePosition() const override;
        Vec2 GetMouseDelta()    const override;
        f32  GetMouseScrollY()  const override;

        bool IsMouseDown    (MouseButton btn) const override;
        bool IsMousePressed (MouseButton btn) const override;
        bool IsMouseReleased(MouseButton btn) const override;

        void SetCursorVisible(bool visible) override;
        void SetCursorLocked (bool locked)  override;
        bool IsCursorLocked  ()       const override;

        // GLFW callbacks - called by GLFW on events
        void OnKey(i32 key, i32 action, i32 mods);
        void OnMouseButton(i32 button, i32 action, i32 mods);
        void OnMouseMove(f64 x, f64 y);
        void OnMouseScroll(f64 xOffset, f64 yOffset);

    private:
        static const i32 KEY_COUNT   = 512;
        static const i32 MOUSE_COUNT = 8;

        // Current and previous frame key states
        std::array<KeyState, KEY_COUNT>   keyStates_{};
        std::array<KeyState, MOUSE_COUNT> mouseStates_{};

        // Mouse position
        Vec2 mousePos_     = {0, 0};
        Vec2 lastMousePos_ = {0, 0};
        Vec2 mouseDelta_   = {0, 0};
        f32  scrollY_      = 0.0f;
        bool firstMouse_   = true;
        bool cursorLocked_ = false;

        GLFWwindow* window_   = nullptr;
        IEventBus*  eventBus_ = nullptr;

        mutable std::mutex mutex_;

        bool IsValidKey        (Key key)         const;
        bool IsValidMouseButton(MouseButton btn) const;
    };

    // ── IModule wrapper ───────────────────────────────────────
    class INPUT_API InputModule : public IModule {
    public:
        InputModule();
        ~InputModule();

        VoidResult       Initialize(const ModuleInitParams& params) override;
        void             OnUpdate(f32 deltaTime)                    override;
        void             Shutdown()                                 override;
        ModuleDescriptor GetDescriptor()                      const override;

        InputSystem* GetInputSystem() { return input_.get(); }

    private:
        std::unique_ptr<InputSystem> input_;
    };

} // namespace RiftCore

#pragma warning(pop)
