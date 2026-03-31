#pragma once

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>

namespace RiftCore {

    // ── Audio handles ─────────────────────────────────────────
    using AudioClipID   = u32;
    using AudioSourceID = u32;
    constexpr AudioClipID   INVALID_CLIP   = 0;
    constexpr AudioSourceID INVALID_SOURCE = 0;

    // ── Clip descriptor ───────────────────────────────────────
    struct AudioClipDesc {
        String filePath;
        bool   stream      = false;  // stream from disk (music)
        bool   is3D        = false;  // 3D positional audio
        f32    defaultVolume = 1.0f;
        f32    defaultPitch  = 1.0f;
    };

    // ── Source descriptor ─────────────────────────────────────
    struct AudioSourceDesc {
        AudioClipID clipID      = INVALID_CLIP;
        Vec3        position    = Vec3::Zero();
        f32         volume      = 1.0f;
        f32         pitch       = 1.0f;
        f32         minDistance = 1.0f;
        f32         maxDistance = 50.0f;
        bool        looping     = false;
        bool        playOnCreate= false;
        bool        is3D        = false;
    };

    // ── IAudio interface ──────────────────────────────────────
    class IAudio {
    public:
        virtual ~IAudio() = default;

        virtual VoidResult Initialize()               = 0;
        virtual void       Shutdown()                 = 0;
        virtual void       Update(f32 deltaTime)      = 0;

        // ── Clip management ───────────────────────────────────
        virtual Result<AudioClipID> LoadClip(
            const AudioClipDesc& desc)                = 0;
        virtual void UnloadClip(AudioClipID id)       = 0;
        virtual void UnloadAll()                      = 0;

        // ── Source management ─────────────────────────────────
        virtual Result<AudioSourceID> CreateSource(
            const AudioSourceDesc& desc)              = 0;
        virtual void DestroySource(AudioSourceID id)  = 0;

        // ── Playback ──────────────────────────────────────────
        virtual void Play    (AudioSourceID id)       = 0;
        virtual void Pause   (AudioSourceID id)       = 0;
        virtual void Stop    (AudioSourceID id)       = 0;
        virtual void Restart (AudioSourceID id)       = 0;
        virtual bool IsPlaying(AudioSourceID id)const = 0;
        virtual bool IsLooping(AudioSourceID id)const = 0;
        virtual void SetLooping(AudioSourceID id,
                                bool loop)            = 0;

        // ── Volume + Pitch ────────────────────────────────────
        virtual void SetVolume(AudioSourceID id,
                               f32 volume)            = 0;
        virtual f32  GetVolume(AudioSourceID id)const = 0;
        virtual void SetPitch (AudioSourceID id,
                               f32 pitch)             = 0;
        virtual f32  GetPitch (AudioSourceID id)const = 0;

        // ── 3D positioning ────────────────────────────────────
        virtual void SetSourcePosition(
            AudioSourceID id, const Vec3& pos)        = 0;
        virtual void SetListenerPosition(
            const Vec3& pos)                          = 0;
        virtual void SetListenerOrientation(
            const Vec3& forward,
            const Vec3& up)                           = 0;

        // ── Master volume ─────────────────────────────────────
        virtual void SetMasterVolume(f32 volume)      = 0;
        virtual f32  GetMasterVolume()          const = 0;

        // ── Convenience: play a clip directly ─────────────────
        // Creates a temporary source, plays, auto-destroys
        virtual AudioSourceID PlayOneShot(
            AudioClipID clipID,
            f32 volume = 1.0f,
            f32 pitch  = 1.0f)                        = 0;

        // ── Query ─────────────────────────────────────────────
        virtual u32 GetActiveSourceCount() const      = 0;
        virtual u32 GetLoadedClipCount()   const      = 0;
    };

} // namespace RiftCore
