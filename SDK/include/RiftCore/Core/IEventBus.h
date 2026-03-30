// ============================================================
// IEventBus.h
// Decoupled event system — publish/subscribe pattern.
//
// HOW IT WORKS:
//   Publisher (e.g., Input DLL):
//     eventBus->Publish(KeyPressedEvent{Key::Space});
//
//   Subscriber (e.g., Player system):
//     eventBus->Subscribe<KeyPressedEvent>([](auto& e){
//         player.Jump();
//     });
//
// No direct connection between publisher and subscriber.
// ============================================================
#pragma once

#include "../Common/Platform.h"
#include "../Common/Types.h"

#include <functional>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>

namespace RiftCore {

    // ── Subscription handle ───────────────────────────────────
    // Store this to unsubscribe later
    struct EventSubscriptionID {
        u64 id = 0;
        bool IsValid() const { return id != 0; }
    };

    // ── IEventBus interface ───────────────────────────────────
    class IEventBus {
    public:
        virtual ~IEventBus() = default;

        // ── Subscribe to an event type ────────────────────────
        // Returns a subscription ID — store it to unsubscribe
        // Handler is called synchronously when event is published
        template<typename EventType>
        EventSubscriptionID Subscribe(
            std::function<void(const EventType&)> handler
        );

        // ── Unsubscribe ───────────────────────────────────────
        virtual void Unsubscribe(EventSubscriptionID id) = 0;

        // ── Publish — immediate dispatch ──────────────────────
        // Calls all subscribers synchronously on calling thread
        template<typename EventType>
        void Publish(const EventType& event);

        // ── Publish — queued dispatch ─────────────────────────
        // Stores event, dispatches on ProcessQueue() call
        // Safe to call from any thread
        template<typename EventType>
        void PublishQueued(const EventType& event);

        // ── Process queued events ─────────────────────────────
        // Call this from main thread once per frame
        virtual void ProcessQueue() = 0;

    protected:
        // Internal virtuals that templates call
        // Allows template methods to work across DLL boundary

        virtual EventSubscriptionID SubscribeInternal(
            std::type_index                         eventType,
            std::function<void(const void*)>        handler
        ) = 0;

        virtual void PublishInternal(
            std::type_index eventType,
            const void*     eventData
        ) = 0;

        virtual void PublishQueuedInternal(
            std::type_index eventType,
            const void*     eventData,
            usize           eventSize
        ) = 0;
    };

    // ── Template implementation ───────────────────────────────
    template<typename EventType>
    EventSubscriptionID IEventBus::Subscribe(
        std::function<void(const EventType&)> handler
    ) {
        return SubscribeInternal(
            std::type_index(typeid(EventType)),
            [h = std::move(handler)](const void* data) {
                h(*static_cast<const EventType*>(data));
            }
        );
    }

    template<typename EventType>
    void IEventBus::Publish(const EventType& event) {
        PublishInternal(
            std::type_index(typeid(EventType)),
            static_cast<const void*>(&event)
        );
    }

    template<typename EventType>
    void IEventBus::PublishQueued(const EventType& event) {
        PublishQueuedInternal(
            std::type_index(typeid(EventType)),
            static_cast<const void*>(&event),
            sizeof(EventType)
        );
    }

    // ── Built-in Engine Events ────────────────────────────────
    // All modules can listen to these

    struct EngineStartEvent    {};
    struct EngineShutdownEvent {};

    struct WindowResizeEvent {
        u32 width  = 0;
        u32 height = 0;
    };

    struct WindowCloseEvent {};

    struct FrameBeginEvent {
        f32 deltaTime = 0.0f;
        u64 frameIndex = 0;
    };

    struct FrameEndEvent {
        u64 frameIndex = 0;
    };

    struct ModuleLoadedEvent {
        String moduleName;
    };

    struct ModuleUnloadedEvent {
        String moduleName;
    };

} // namespace RiftCore