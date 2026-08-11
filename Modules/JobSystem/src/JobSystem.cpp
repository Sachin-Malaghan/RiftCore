#pragma warning(disable: 4190)
#include <JobSystem/JobSystem.h>
#include <RiftCore/Common/EngineContext.h>
#include <RiftCore/Core/ILogger.h>
#include <iostream>
#include <thread>






namespace RiftCore {

    // -- JobSystemImpl -----------------------------------------
    JobSystemImpl::JobSystemImpl()  = default;
    JobSystemImpl::~JobSystemImpl() {
        if (threadPool_) threadPool_->WaitAll();
    }

    JobHandle JobSystemImpl::Submit(JobDesc desc) {
        Job job(std::move(desc.function), desc.priority, desc.debugName);
        return threadPool_->Submit(std::move(job));
    }

    void JobSystemImpl::ParallelFor(
        u32                      count,
        std::function<void(u32)> body,
        JobPriority              priority
    ) {
        threadPool_->ParallelFor(count, std::move(body), priority);
    }

    void JobSystemImpl::WaitAll() {
        if (threadPool_) threadPool_->WaitAll();
    }

    u32 JobSystemImpl::GetWorkerThreadCount() const {
        return threadPool_ ? threadPool_->GetThreadCount() : 0;
    }

    u32 JobSystemImpl::GetPendingJobCount() const {
        return threadPool_ ? threadPool_->GetPendingCount() : 0;
    }

    // -- JobSystemModule ---------------------------------------

    JobSystemModule::JobSystemModule()  = default;
    JobSystemModule::~JobSystemModule() = default;

    VoidResult JobSystemModule::Initialize(
        const ModuleInitParams& params
    ) {
        ILogger* logger = nullptr;
        if (params.context) {
            logger = params.context->Logger();
        }

        if (logger) logger->Info("JobSystem", "Initializing...");

        jobSystem_ = std::make_unique<JobSystemImpl>();

        u32 hw      = std::thread::hardware_concurrency();
        u32 workers = (hw > 1) ? (hw - 1) : 1;

        jobSystem_->threadPool_ = std::make_unique<ThreadPool>(workers);

        if (params.context) {
            params.context->Register<IJobSystem>(jobSystem_.get());
        }

        if (logger) {
            logger->Info("JobSystem",
                "Ready. Workers: " + std::to_string(workers));
        }

        return VoidResult::Ok();
    }

    void JobSystemModule::OnUpdate(f32 deltaTime) {
        RIFTCORE_UNUSED(deltaTime);
    }

    void JobSystemModule::Shutdown() {
        std::cout << "[JobSystem] Shutting down...\n";
        if (jobSystem_ && jobSystem_->threadPool_) {
            jobSystem_->threadPool_->WaitAll();
            jobSystem_->threadPool_.reset();
        }
        jobSystem_.reset();
        std::cout << "[JobSystem] Shutdown complete.\n";
    }

    ModuleDescriptor JobSystemModule::GetDescriptor() const {
        ModuleDescriptor desc;
        desc.name        = "JobSystem";
        desc.version     = "0.1.0";
        desc.apiVersion  = RIFTCORE_API_VERSION;
        desc.description = "Thread pool job scheduler";
        return desc;
    }

    RIFTCORE_IMPLEMENT_MODULE(JobSystemModule)

} // namespace RiftCore
