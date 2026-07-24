#pragma once

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>

#include <functional>
#include <future>
#include <memory>
#include <chrono>
#include <atomic>









namespace RiftCore {

    // ── Job Priority ──────────────────────────────────────────
    enum class JobPriority : u8 {
        Critical = 0,
        High     = 1,
        Normal   = 2,
        Low      = 3,
        Idle     = 4
    };

    // ── JobState ──────────────────────────────────────────────
    // Shared between JobHandle and ThreadPool
    // Heap allocated, reference counted
    struct JobState {
        std::promise<void>       promise;
        std::shared_future<void> future;
        std::atomic<bool>        completed{ false };
        std::atomic<bool>        cancelled{ false };

        JobState() {
            future = promise.get_future().share();
        }

        RIFTCORE_NOCOPY_NOMOVE(JobState);
    };

    // ── JobHandle ─────────────────────────────────────────────
    class JobHandle {
    public:
        JobHandle() = default;

        explicit JobHandle(std::shared_ptr<JobState> state)
            : state_(std::move(state)) {}

        void Wait() const {
            if (IsValid()) state_->future.wait();
        }

        bool WaitFor(u32 milliseconds) const {
            if (!IsValid()) return true;
            return state_->future.wait_for(
                std::chrono::milliseconds(milliseconds)
            ) == std::future_status::ready;
        }

        bool IsComplete()  const {
            if (!IsValid()) return true;
            return state_->completed.load();
        }

        bool IsValid() const { return state_ != nullptr; }

        static JobHandle Invalid() { return {}; }

    private:
        std::shared_ptr<JobState> state_;
    };

    // ── Job Descriptor (SDK level) ────────────────────────────
    using JobFunction = std::function<void()>;

    struct JobDesc {
        JobFunction  function;
        JobPriority  priority  = JobPriority::Normal;
        const char*  debugName = "UnnamedJob";
        JobHandle    dependency;
    };

    // ── IJobSystem Interface ──────────────────────────────────
    class IJobSystem {
    public:
        virtual ~IJobSystem() = default;

        virtual JobHandle Submit(JobDesc desc) = 0;

        JobHandle Submit(
            JobFunction fn,
            JobPriority priority = JobPriority::Normal,
            const char* name     = "Job"
        ) {
            return Submit(JobDesc{ std::move(fn), priority, name });
        }

        virtual void ParallelFor(
            u32                      count,
            std::function<void(u32)> body,
            JobPriority              priority = JobPriority::Normal
        ) = 0;

        virtual void WaitAll()                    = 0;
        virtual u32  GetWorkerThreadCount() const = 0;
        virtual u32  GetPendingJobCount()   const = 0;
    };

} // namespace RiftCore
