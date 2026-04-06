#pragma warning(disable: 4190)
#include <OpenGLBackend/GLDevice.h>
#include <RiftCore/Common/EngineContext.h>
#include <RiftCore/Core/ILogger.h>
#include <RiftCore/Core/IModule.h>
#include <iostream>

namespace RiftCore {

    class OpenGLBackendModule : public IModule {
    public:
        OpenGLBackendModule()  = default;
        ~OpenGLBackendModule() = default;

        VoidResult Initialize(
            const ModuleInitParams& params
        ) override {
            ILogger* logger = nullptr;
            if (params.context) {
                logger = params.context->Logger();
            }

            if (logger) {
                logger->Info("OpenGL",
                    "OpenGL Backend initializing...");
            }

            rhi_ = std::make_unique<OpenGLRHI>();

            // Register RHI in context
            if (params.context) {
                params.context->Register<IRHI>(rhi_.get());
            }

            if (logger) {
                logger->Info("OpenGL",
                    "OpenGL RHI registered.");
            }

            return VoidResult::Ok();
        }

        void OnUpdate(f32 deltaTime) override {
            RIFTCORE_UNUSED(deltaTime);
        }

        void Shutdown() override {
            std::cout << "[OpenGL] Shutting down...\n";
            rhi_.reset();
            std::cout << "[OpenGL] Shutdown complete.\n";
        }

        ModuleDescriptor GetDescriptor() const override {
            ModuleDescriptor desc;
            desc.name        = "OpenGLBackend";
            desc.version     = "0.1.0";
            desc.apiVersion  = RIFTCORE_API_VERSION;
            desc.description = "OpenGL 4.6 RHI Backend";
            return desc;
        }

        OpenGLRHI* GetRHI() { return rhi_.get(); }

    private:
        std::unique_ptr<OpenGLRHI> rhi_;
    };

    RIFTCORE_IMPLEMENT_MODULE(OpenGLBackendModule)

} // namespace RiftCore
