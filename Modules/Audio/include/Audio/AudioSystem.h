#pragma once

#pragma warning(push)
#pragma warning(disable: 4251 4275)

#include <RiftCore/Common/Platform.h>
#include <RiftCore/Common/Types.h>
#include <RiftCore/Common/Result.h>
#include <RiftCore/Core/IModule.h>
#include <RiftCore/Audio/IAudio.h>

#include <unordered_map>
#include <memory>
#include <string>
#include <mutex>
#include <atomic>





// Forward declare miniaudio types
// We don't include miniaudio.h in header to keep
// compile times fast and avoid leaking its defines
struct ma_engine;
struct ma_sound;
struct ma_resource_manager;

#ifdef AUDIO_EXPORTS
    #define AUDIO_API RIFTCORE_EXPORT
#else
    #define AUDIO_API RIFTCORE_IMPORT
#endif

namespace RiftCore {

    // ── Internal clip data ────────────────────────────────────
    struct AudioClipData {
        AudioClipID id          = INVALID_CLIP;
        String      filePath;
        bool        is3D        = false;
        bool        stream      = false;
        f32         baseVolume  = 1.0f;
        f32         basePitch   = 1.0f;
        bool        loaded      = false;
    };

    // ── Internal source data ──────────────────────────────────
    struct AudioSourceData {
        AudioSourceID id         = INVALID_SOURCE;
        AudioClipID   clipID     = INVALID_CLIP;
        ma_sound*     sound      = nullptr;
        Vec3          position   = Vec3::Zero();
        f32           volume     = 1.0f;
        f32           pitch      = 1.0f;
        f32           minDist    = 1.0f;
        f32           maxDist    = 50.0f;
        bool          is3D       = false;
        bool          looping    = false;
        bool          oneShot    = false;   // auto-destroy
        bool          active     = false;
    };

    // ── AudioSystem ───────────────────────────────────────────
    class AUDIO_API AudioSystem : public IAudio {
    public:
        AudioSystem();
        ~AudioSystem();

        RIFTCORE_NOCOPY_NOMOVE(AudioSystem);

        // IAudio
        VoidResult Initialize()               override;
        void       Shutdown()                 override;
        void       Update(f32 deltaTime)      override;

        Result<AudioClipID> LoadClip(
            const AudioClipDesc& desc)        override;
        void UnloadClip(AudioClipID id)       override;
        void UnloadAll()                      override;

        Result<AudioSourceID> CreateSource(
            const AudioSourceDesc& desc)      override;
        void DestroySource(AudioSourceID id)  override;

        void Play    (AudioSourceID id)       override;
        void Pause   (AudioSourceID id)       override;
        void Stop    (AudioSourceID id)       override;
        void Restart (AudioSourceID id)       override;
        bool IsPlaying(AudioSourceID id)const override;
        bool IsLooping(AudioSourceID id)const override;
        void SetLooping(AudioSourceID id,
                        bool loop)            override;

        void SetVolume(AudioSourceID id,
                       f32 volume)            override;
        f32  GetVolume(AudioSourceID id)const override;
        void SetPitch (AudioSourceID id,
                       f32 pitch)             override;
        f32  GetPitch (AudioSourceID id)const override;

        void SetSourcePosition(
            AudioSourceID id,
            const Vec3& pos)                  override;
        void SetListenerPosition(
            const Vec3& pos)                  override;
        void SetListenerOrientation(
            const Vec3& forward,
            const Vec3& up)                   override;

        void SetMasterVolume(f32 volume)      override;
        f32  GetMasterVolume()          const override;

        AudioSourceID PlayOneShot(
            AudioClipID clipID,
            f32 volume = 1.0f,
            f32 pitch  = 1.0f)                override;

        u32 GetActiveSourceCount() const      override;
        u32 GetLoadedClipCount()   const      override;

    private:
        AudioClipData*         GetClip  (AudioClipID id);
        AudioSourceData*       GetSource(AudioSourceID id);
        const AudioSourceData* GetSource(AudioSourceID id) const;

        ma_engine*  engine_       = nullptr;
        f32         masterVolume_ = 1.0f;

        std::unordered_map<AudioClipID,   AudioClipData>   clips_;
        std::unordered_map<AudioSourceID, AudioSourceData> sources_;

        std::atomic<AudioClipID>   nextClipID_  { 1 };
        std::atomic<AudioSourceID> nextSourceID_{ 1 };

        mutable std::mutex mutex_;
        bool initialized_ = false;
    };

    // ── IModule wrapper ───────────────────────────────────────
    class AUDIO_API AudioModule : public IModule {
    public:
        AudioModule();
        ~AudioModule();

        VoidResult       Initialize(
            const ModuleInitParams& params) override;
        void             OnUpdate(f32 dt)   override;
        void             Shutdown()         override;
        ModuleDescriptor GetDescriptor()
                                      const override;

        AudioSystem* GetAudioSystem() {
            return audio_.get();
        }

    private:
        std::unique_ptr<AudioSystem> audio_;
    };

} // namespace RiftCore

#pragma warning(pop)

