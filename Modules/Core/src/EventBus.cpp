#include <Core/EventBus.h>
#include <cstring>

namespace RiftCore {

    EventBus::EventBus()  = default;
    EventBus::~EventBus() = default;

    EventSubscriptionID EventBus::SubscribeInternal(
        std::type_index                  type,
        std::function<void(const void*)> handler
    ) {
        std::lock_guard<std::mutex> lock(subscriberMutex_);
        EventSubscriptionID subID{ nextID_.fetch_add(1) };
        subscribers_[type].push_back(
            Subscription{ subID, std::move(handler) }
        );
        return subID;
    }

    void EventBus::Unsubscribe(EventSubscriptionID id) {
        std::lock_guard<std::mutex> lock(subscriberMutex_);
        for (auto& [type, subs] : subscribers_) {
            subs.erase(
                std::remove_if(subs.begin(), subs.end(),
                    [&id](const Subscription& s) {
                        return s.id.id == id.id;
                    }),
                subs.end()
            );
        }
    }

    void EventBus::PublishInternal(
        std::type_index type,
        const void*     data
    ) {
        std::lock_guard<std::mutex> lock(subscriberMutex_);
        auto it = subscribers_.find(type);
        if (it == subscribers_.end()) return;
        for (auto& sub : it->second) {
            sub.handler(data);
        }
    }

    void EventBus::PublishQueuedInternal(
        std::type_index type,
        const void*     data,
        usize           size
    ) {
        std::vector<byte> bytes(size);
        std::memcpy(bytes.data(), data, size);

        std::lock_guard<std::mutex> lock(queueMutex_);
        eventQueue_.emplace(type, std::move(bytes));
    }

    void EventBus::ProcessQueue() {
        std::queue<QueuedEvent> localQueue;
        {
            std::lock_guard<std::mutex> lock(queueMutex_);
            std::swap(localQueue, eventQueue_);
        }
        while (!localQueue.empty()) {
            auto& ev = localQueue.front();
            PublishInternal(ev.type, ev.data.data());
            localQueue.pop();
        }
    }

} // namespace RiftCore
