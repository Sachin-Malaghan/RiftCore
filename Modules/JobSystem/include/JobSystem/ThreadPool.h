#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Job/IJobSystem.h>

#include <JobSystem/Job.h>

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <functional>

#ifdef JOBSYSTEM_EXPORTS
    #define JOBSYSTEM_API RIFTCORE_EXPORT
#else
    #define JOBSYSTEM_API RIFTCORE_IMPORT
#endif

namespace RiftCore {

    // Thread-safe priority queue
    class JOBSYSTEM_API JobQueue {
    public:
        JobQueue()  = default;
        ~JobQueue() = default;
        RIFTCORE_NOCOPY_NOMOVE(JobQueue);

        void Push(Job job);
        bool Pop(Job& outJob);
        bool IsEmpty() const;
        u32  Size()    const;
        void Clear();

    private:
        std::priority_queue<Job, std::vector<Job>, std::less<Job>> queue_;
        mutable std::mutex mutex_;
    };

    // Thread pool - N worker threads pulling from JobQueue
    class JOBSYSTEM_API ThreadPool {
    public:
        explicit ThreadPool(u32 threadCount = 0);
        ~ThreadPool();
        RIFTCORE_NOCOPY_NOMOVE(ThreadPool);

        JobHandle Submit(Job job);

        JobHandle Submit(
            JobFunction fn,
            JobPriority priority = JobPriority::Normal,
            const char* name     = "Job"
        );

        void ParallelFor(
            u32                      count,
            std::function<void(u32)> body,
            JobPriority              priority = JobPriority::Normal
        );

        void WaitAll();

        u32  GetThreadCount()    const { return threadCount_;               }
        u32  GetPendingCount()   const { return pendingJobs_.load();        }
        u64  GetTotalProcessed() const { return totalJobsProcessed_.load(); }
        bool IsRunning()         const { return running_.load();            }

    private:
        void WorkerThreadLoop(u32 threadIndex);

        u32                      threadCount_ = 0;
        std::vector<std::thread> threads_;
        JobQueue                 queue_;

        std::mutex              wakeMutex_;
        std::condition_variable wakeCondition_;

        std::atomic<bool> running_{ false };
        std::atomic<u32>  pendingJobs_{ 0 };
        std::atomic<u32>  activeJobs_{ 0 };
        std::atomic<u64>  totalJobsProcessed_{ 0 };

        std::mutex              allDoneMutex_;
        std::condition_variable allDoneCondition_;
    };

} // namespace RiftCore

#pragma warning(pop)
