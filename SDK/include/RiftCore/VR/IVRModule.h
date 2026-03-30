// ── SDK/include/RiftCore/VR/IVRModule.h ──────────────────────
#pragma once
#include "../Common/Platform.h"
#include "../Common/Types.h"
#include "../Common/Result.h"

namespace RiftCore {

    struct VRPose {
        Vec3  position = Vec3::Zero();
        Quat  rotation = Quat::Identity();
        bool  valid    = false;
    };

    struct VREyeData {
        Mat4     projectionMatrix = Mat4::Identity();
        Mat4     viewMatrix       = Mat4::Identity();
        Viewport viewport;
    };

    enum class VRHand : u8 { Left = 0, Right = 1 };

    struct VRControllerState {
        VRPose pose;
        bool   triggerPressed  = false;
        bool   gripPressed     = false;
        bool   menuPressed     = false;
        Vec2   thumbstick      = {0,0};
        f32    triggerValue    = 0.0f;
        f32    gripValue       = 0.0f;
    };

    class IVRModule {
    public:
        virtual ~IVRModule() = default;

        virtual VoidResult  Initialize()                                  = 0;
        virtual void        Shutdown()                                    = 0;
        virtual void        Update()                                      = 0;
        virtual bool        IsHMDConnected()                        const  = 0;
        virtual VRPose      GetHMDPose()                            const  = 0;
        virtual VREyeData   GetEyeData(u32 eyeIndex)               const  = 0;
        virtual VRControllerState GetControllerState(VRHand hand)  const  = 0;
        virtual void        SubmitFrame(
            u32 leftTextureHandle,
            u32 rightTextureHandle
        ) = 0;
        virtual void        SetRumble(VRHand hand, f32 intensity,
                                      f32 duration)                       = 0;
    };

} // namespace RiftCore