#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>
#include <RiftCore/Core/IModule.h>
#include <RiftCore/Job/IJobSystem.h>

#include <JobSystem/ThreadPool.h>

#include <memory>

#ifdef JOBSYSTEM_EXPORTS
    #define JOBSYSTEM_API RIFTCORE_EXPORT
#else
    #define JOBSYSTEM_API RIFTCORE_IMPORT
#endif






namespace RiftCore {

    class JOBSYSTEM_API JobSystemImpl : public IJobSystem {
    public:
        JobSystemImpl();
        ~JobSystemImpl();

        JobHandle Submit(JobDesc desc)                        override;
        void      ParallelFor(u32, std::function<void(u32)>,
                              JobPriority) override;
        void      WaitAll()                                   override;
        u32       GetWorkerThreadCount()                const override;
        u32       GetPendingJobCount()                  const override;

        // Public so Module and main.cpp can access stats
        std::unique_ptr<ThreadPool> threadPool_;
    };

    class JOBSYSTEM_API JobSystemModule : public IModule {
    public:
        JobSystemModule();
        ~JobSystemModule();

        VoidResult       Initialize(const ModuleInitParams& params) override;
        void             OnUpdate(f32 deltaTime)                    override;
        void             Shutdown()                                 override;
        ModuleDescriptor GetDescriptor()                      const override;

        JobSystemImpl* GetJobSystem() { return jobSystem_.get(); }

    private:
        std::unique_ptr<JobSystemImpl> jobSystem_;
    };

} // namespace RiftCore

#pragma warning(pop)
