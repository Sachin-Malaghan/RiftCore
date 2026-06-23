#include <JobSystem/ThreadPool.h>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <cassert>

namespace RiftCore {

    // ── JobQueue ─────────────────────────────────────────────
    void JobQueue::Push(Job job) {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(job));
    }

    bool JobQueue::Pop(Job& outJob) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.empty()) return false;
        outJob = std::move(const_cast<Job&>(queue_.top()));
        queue_.pop();
        return true;
    }

    bool JobQueue::IsEmpty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.empty();
    }

    u32 JobQueue::Size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<u32>(queue_.size());
    }

    void JobQueue::Clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!queue_.empty()) queue_.pop();
    }

    // ── ThreadPool ───────────────────────────────────────────

    ThreadPool::ThreadPool(u32 threadCount) {
        // If 0 passed use hardware concurrency minus 1
        // Always keep at least 1 worker thread
        if (threadCount == 0) {
            u32 hw = std::thread::hardware_concurrency();
            threadCount = (hw > 1) ? (hw - 1) : 1;
        }
        threadCount_ = threadCount;
        running_.store(true);

        threads_.reserve(threadCount_);
        for (u32 i = 0; i < threadCount_; ++i) {
            threads_.emplace_back(
                &ThreadPool::WorkerThreadLoop, this, i
            );
        }

        std::cout << "[ThreadPool] Started with "
                  << threadCount_ << " worker threads\n";
    }

    ThreadPool::~ThreadPool() {
        // Signal all threads to stop
        running_.store(false);
        wakeCondition_.notify_all();

        // Join all worker threads
        for (auto& thread : threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        std::cout << "[ThreadPool] Shutdown. "
                  << "Jobs processed: "
                  << totalJobsProcessed_.load() << "\n";
    }

    void ThreadPool::WorkerThreadLoop(u32 threadIndex) {
        // Set thread name for debugger
        // (Visual Studio shows thread names in debug window)
#ifdef RIFTCORE_PLATFORM_WINDOWS
        std::wstring name = L"RiftWorker_" +
                            std::to_wstring(threadIndex);
        // SetThreadDescription requires Windows 10+
        // Skip if not available - not critical
#endif

        while (true) {
            Job job;
            bool gotJob = false;

            // Try to get a job from the queue
            {
                std::unique_lock<std::mutex> lock(wakeMutex_);

                // Wait until there is a job OR we are shutting down
                wakeCondition_.wait(lock, [this] {
                    return !queue_.IsEmpty() || !running_.load();
                });

                // Check if we should stop
                if (!running_.load() && queue_.IsEmpty()) {
                    break;
                }
            }

            // Pop job outside the lock
            gotJob = queue_.Pop(job);

            if (gotJob && job.function) {
                activeJobs_.fetch_add(1);

                // Execute the job
                try {
                    job.function();
                }
                catch (const std::exception& e) {
                    std::cerr << "[ThreadPool] Job exception ["
                              << job.debugName << "]: "
                              << e.what() << "\n";
                }
                catch (...) {
                    std::cerr << "[ThreadPool] Unknown exception in job ["
                              << job.debugName << "]\n";
                }

                // Signal completion
                if (job.state) {
                    job.state->completed.store(true);
                    try {
                        job.state->promise.set_value();
                    }
                    catch (...) {
                        // Promise already set - ignore
                    }
                }

                activeJobs_.fetch_sub(1);
                pendingJobs_.fetch_sub(1);
                totalJobsProcessed_.fetch_add(1);

                // Notify WaitAll() if everything is done
                if (pendingJobs_.load() == 0 &&
                    activeJobs_.load() == 0) {
                    allDoneCondition_.notify_all();
                }
            }
        }
    }

    JobHandle ThreadPool::Submit(Job job) {
        assert(running_.load() && "ThreadPool not running");

        JobHandle handle(job.state);

        pendingJobs_.fetch_add(1);
        queue_.Push(std::move(job));

        // Wake one worker thread
        wakeCondition_.notify_one();

        return handle;
    }

    JobHandle ThreadPool::Submit(
        std::function<void()> fn,
        JobPriority           priority,
        const char*           name
    ) {
        return Submit(Job(std::move(fn), priority, name));
    }

    void ThreadPool::ParallelFor(
        u32                      count,
        std::function<void(u32)> body,
        JobPriority              priority
    ) {
        if (count == 0) return;

        // Track how many jobs remain
        auto remaining = std::make_shared<std::atomic<u32>>(count);
        auto allDone   = std::make_shared<std::promise<void>>();
        auto future    = allDone->get_future().share();

        for (u32 i = 0; i < count; ++i) {
            u32 index = i;
            Submit(
                [body, index, remaining, allDone]() {
                    body(index);
                    u32 prev = remaining->fetch_sub(1);
                    if (prev == 1) {
                        // Last job - signal completion
                        try {
                            allDone->set_value();
                        }
                        catch (...) {}
                    }
                },
                priority,
                "ParallelFor"
            );
        }

        // Wait for all parallel jobs
        future.wait();
    }

    void ThreadPool::WaitAll() {
        if (pendingJobs_.load() == 0 &&
            activeJobs_.load()  == 0) {
            return;
        }

        std::unique_lock<std::mutex> lock(allDoneMutex_);
        allDoneCondition_.wait(lock, [this] {
            return pendingJobs_.load() == 0 &&
                   activeJobs_.load()  == 0;
        });
    }

} // namespace RiftCore
