#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Core/IEventBus.h>

#include <unordered_map>
#include <vector>
#include <functional>
#include <mutex>
#include <queue>
#include <memory>
#include <typeindex>
#include <atomic>

#ifdef CORE_EXPORTS
    #define CORE_API RIFTCORE_EXPORT
#else
    #define CORE_API RIFTCORE_IMPORT
#endif

namespace RiftCore {

    class CORE_API EventBus : public IEventBus {
    public:
        EventBus();
        ~EventBus();

        void Unsubscribe(EventSubscriptionID id) override;
        void ProcessQueue() override;

    protected:
        EventSubscriptionID SubscribeInternal(
            std::type_index                  type,
            std::function<void(const void*)> handler
        ) override;

        void PublishInternal(
            std::type_index type,
            const void*     data
        ) override;

        void PublishQueuedInternal(
            std::type_index type,
            const void*     data,
            usize           size
        ) override;

    private:
        struct Subscription {
            EventSubscriptionID              id;
            std::function<void(const void*)> handler;
        };

        struct QueuedEvent {
            std::type_index   type;
            std::vector<byte> data;

            // Explicit constructor - fixes deleted default constructor
            QueuedEvent(std::type_index t, std::vector<byte> d)
                : type(t), data(std::move(d)) {}
        };

        std::unordered_map<
            std::type_index,
            std::vector<Subscription>
        > subscribers_;

        std::queue<QueuedEvent> eventQueue_;
        std::mutex              subscriberMutex_;
        std::mutex              queueMutex_;
        std::atomic<u64>        nextID_{ 1 };
    };

} // namespace RiftCore

#pragma warning(pop)
