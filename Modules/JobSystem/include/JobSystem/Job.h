#pragma once

// Job.h - Internal job representation used by ThreadPool
// Uses types from IJobSystem.h - NO redefinition

#include <RiftCore/Job/IJobSystem.h>
#include <functional>

namespace RiftCore {

    // Internal job structure used inside ThreadPool
    // Extends the SDK JobDesc concept with state tracking
    struct Job {
        JobFunction               function;
        JobPriority               priority  = JobPriority::Normal;
        const char*               debugName = "UnnamedJob";
        std::shared_ptr<JobState> state;

        Job() = default;

        Job(JobFunction  fn,
            JobPriority  prio = JobPriority::Normal,
            const char*  name = "Job")
            : function(std::move(fn))
            , priority(prio)
            , debugName(name)
            , state(std::make_shared<JobState>())
        {}

        // Lower enum value = higher priority
        // std::priority_queue uses operator< for ordering
        bool operator<(const Job& other) const {
            return static_cast<u8>(priority) >
                   static_cast<u8>(other.priority);
        }
    };

} // namespace RiftCore
