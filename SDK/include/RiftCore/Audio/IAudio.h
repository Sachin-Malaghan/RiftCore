// ── SDK/include/RiftCore/Audio/IAudio.h ──────────────────────
#pragma once
#include "../Common/Platform.h"
#include "../Common/Types.h"
#include "../Common/Result.h"

namespace RiftCore {

    using AudioClipID  = u32;
    using AudioSourceID= u32;
    constexpr AudioClipID   INVALID_AUDIO_CLIP   = 0;
    constexpr AudioSourceID INVALID_AUDIO_SOURCE = 0;

    struct AudioClipDesc {
        String   filePath;
        bool     stream      = false;  // stream from disk vs load all
        bool     is3D        = false;
    };

    struct AudioSourceDesc {
        AudioClipID clipID    = INVALID_AUDIO_CLIP;
        Vec3        position  = Vec3::Zero();
        f32         volume    = 1.0f;
        f32         pitch     = 1.0f;
        f32         minDist   = 1.0f;
        f32         maxDist   = 100.0f;
        bool        looping   = false;
        bool        playOnStart = false;
    };

    class IAudio {
    public:
        virtual ~IAudio() = default;

        virtual VoidResult      Initialize()                                  = 0;
        virtual void            Shutdown()                                    = 0;
        virtual void            Update(f32 deltaTime)                         = 0;

        virtual Result<AudioClipID>   LoadClip(const AudioClipDesc& desc)    = 0;
        virtual void                  UnloadClip(AudioClipID id)              = 0;

        virtual Result<AudioSourceID> CreateSource(const AudioSourceDesc& d) = 0;
        virtual void                  DestroySource(AudioSourceID id)         = 0;

        virtual void Play   (AudioSourceID id)                               = 0;
        virtual void Pause  (AudioSourceID id)                               = 0;
        virtual void Stop   (AudioSourceID id)                               = 0;
        virtual bool IsPlaying(AudioSourceID id)                       const  = 0;

        virtual void SetSourcePosition(AudioSourceID id, const Vec3& pos)    = 0;
        virtual void SetSourceVolume  (AudioSourceID id, f32 volume)         = 0;

        virtual void SetListenerPosition(const Vec3& pos)                    = 0;
        virtual void SetListenerOrientation(const Vec3& fwd, const Vec3& up) = 0;

        virtual void SetMasterVolume(f32 volume)                             = 0;
        virtual f32  GetMasterVolume()                                 const  = 0;
    };

} // namespace RiftCore