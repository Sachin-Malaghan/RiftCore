#pragma warning(disable: 4190)
#include <Audio/AudioSystem.h>

// Include miniaudio implementation
// Only include the header here (impl is in miniaudio_impl.cpp)
#include <miniaudio.h>

#include <RiftCore/Common/EngineContext.h>
#include <RiftCore/Core/ILogger.h>

#include <iostream>
#include <cstring>
#include <cmath>

namespace RiftCore {

    // -- AudioSystem -------------------------------------------
    AudioSystem::AudioSystem()  = default;
    AudioSystem::~AudioSystem() { Shutdown(); }

    VoidResult AudioSystem::Initialize() {
        if (initialized_) return VoidResult::Ok();

        engine_ = new ma_engine{};

        ma_engine_config cfg = ma_engine_config_init();
        cfg.listenerCount    = 1;

        ma_result r = ma_engine_init(&cfg, engine_);
        if (r != MA_SUCCESS) {
            delete engine_;
            engine_ = nullptr;
            return VoidResult::Err(
                "miniaudio engine init failed: " +
                std::to_string(r));
        }

        initialized_ = true;
        std::cout
            << "[Audio] miniaudio engine initialized.\n";

        return VoidResult::Ok();
    }

    void AudioSystem::Shutdown() {
        if (!initialized_) return;

        std::lock_guard<std::mutex> lock(mutex_);

        // Stop and destroy all sources
        for (auto& [id, src] : sources_) {
            if (src.sound) {
                ma_sound_stop(src.sound);
                ma_sound_uninit(src.sound);
                delete src.sound;
                src.sound = nullptr;
            }
        }
        sources_.clear();
        clips_.clear();

        if (engine_) {
            ma_engine_uninit(engine_);
            delete engine_;
            engine_ = nullptr;
        }

        initialized_ = false;
        std::cout << "[Audio] Shutdown complete.\n";
    }

    void AudioSystem::Update(f32 deltaTime) {
        RIFTCORE_UNUSED(deltaTime);
        if (!initialized_) return;

        std::lock_guard<std::mutex> lock(mutex_);

        // Clean up finished one-shot sources
        std::vector<AudioSourceID> toRemove;
        for (auto& [id, src] : sources_) {
            if (src.oneShot && src.sound) {
                if (!ma_sound_is_playing(src.sound)) {
                    toRemove.push_back(id);
                }
            }
        }

        for (AudioSourceID id : toRemove) {
            auto& src = sources_[id];
            if (src.sound) {
                ma_sound_uninit(src.sound);
                delete src.sound;
                src.sound = nullptr;
            }
            sources_.erase(id);
        }
    }

    // -- Clip management ---------------------------------------
    Result<AudioClipID> AudioSystem::LoadClip(
        const AudioClipDesc& desc
    ) {
        if (!initialized_) {
            return Result<AudioClipID>::Err(
                "Audio not initialized");
        }
        if (desc.filePath.empty()) {
            return Result<AudioClipID>::Err(
                "Empty file path");
        }

        std::lock_guard<std::mutex> lock(mutex_);

        AudioClipID id = nextClipID_.fetch_add(1);

        AudioClipData clip;
        clip.id         = id;
        clip.filePath   = desc.filePath;
        clip.is3D       = desc.is3D;
        clip.stream     = desc.stream;
        clip.baseVolume = desc.defaultVolume;
        clip.basePitch  = desc.defaultPitch;
        clip.loaded     = true;

        clips_[id] = std::move(clip);

        std::cout << "[Audio] Clip loaded: "
                  << desc.filePath
                  << " (id=" << id << ")\n";

        return Result<AudioClipID>::Ok(id);
    }

    void AudioSystem::UnloadClip(AudioClipID id) {
        std::lock_guard<std::mutex> lock(mutex_);

        // Stop any sources using this clip
        for (auto& [sid, src] : sources_) {
            if (src.clipID == id && src.sound) {
                ma_sound_stop(src.sound);
                ma_sound_uninit(src.sound);
                delete src.sound;
                src.sound  = nullptr;
                src.active = false;
            }
        }

        clips_.erase(id);
    }

    void AudioSystem::UnloadAll() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& [id, src] : sources_) {
            if (src.sound) {
                ma_sound_uninit(src.sound);
                delete src.sound;
                src.sound = nullptr;
            }
        }
        sources_.clear();
        clips_.clear();
    }

    // -- Source management -------------------------------------
    Result<AudioSourceID> AudioSystem::CreateSource(
        const AudioSourceDesc& desc
    ) {
        if (!initialized_) {
            return Result<AudioSourceID>::Err(
                "Audio not initialized");
        }

        std::lock_guard<std::mutex> lock(mutex_);

        auto clipIt = clips_.find(desc.clipID);
        if (clipIt == clips_.end()) {
            return Result<AudioSourceID>::Err(
                "Clip not found: " +
                std::to_string(desc.clipID));
        }

        AudioSourceID srcID = nextSourceID_.fetch_add(1);

        AudioSourceData src;
        src.id       = srcID;
        src.clipID   = desc.clipID;
        src.position = desc.position;
        src.volume   = desc.volume;
        src.pitch    = desc.pitch;
        src.minDist  = desc.minDistance;
        src.maxDist  = desc.maxDistance;
        src.is3D     = desc.is3D;
        src.looping  = desc.looping;
        src.active   = true;

        // Create miniaudio sound
        src.sound = new ma_sound{};

        u32 flags = 0;
        if (clipIt->second.stream) {
            flags |= MA_SOUND_FLAG_STREAM;
        }

        ma_result r = ma_sound_init_from_file(
            engine_,
            clipIt->second.filePath.c_str(),
            flags,
            nullptr, nullptr,
            src.sound
        );

        if (r != MA_SUCCESS) {
            delete src.sound;
            src.sound = nullptr;
            return Result<AudioSourceID>::Err(
                "Failed to load sound file: " +
                clipIt->second.filePath +
                " (error=" + std::to_string(r) + ")"
            );
        }

        // Configure sound
        ma_sound_set_volume(src.sound, desc.volume);
        ma_sound_set_pitch (src.sound, desc.pitch);
        ma_sound_set_looping(src.sound, desc.looping);

        if (desc.is3D) {
            ma_sound_set_spatialization_enabled(
                src.sound, MA_TRUE);
            ma_sound_set_min_distance(
                src.sound, desc.minDistance);
            ma_sound_set_max_distance(
                src.sound, desc.maxDistance);
            ma_sound_set_position(
                src.sound,
                desc.position.x,
                desc.position.y,
                desc.position.z);
        } else {
            ma_sound_set_spatialization_enabled(
                src.sound, MA_FALSE);
        }

        sources_[srcID] = std::move(src);

        if (desc.playOnCreate) {
            ma_sound_start(sources_[srcID].sound);
        }

        return Result<AudioSourceID>::Ok(srcID);
    }

    void AudioSystem::DestroySource(AudioSourceID id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = sources_.find(id);
        if (it == sources_.end()) return;
        if (it->second.sound) {
            ma_sound_stop  (it->second.sound);
            ma_sound_uninit(it->second.sound);
            delete it->second.sound;
            it->second.sound = nullptr;
        }
        sources_.erase(it);
    }

    // -- Playback ----------------------------------------------
    void AudioSystem::Play(AudioSourceID id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* src = GetSource(id);
        if (src && src->sound) {
            ma_sound_start(src->sound);
        }
    }

    void AudioSystem::Pause(AudioSourceID id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* src = GetSource(id);
        if (src && src->sound) {
            ma_sound_stop(src->sound);
        }
    }

    void AudioSystem::Stop(AudioSourceID id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* src = GetSource(id);
        if (src && src->sound) {
            ma_sound_stop(src->sound);
            ma_sound_seek_to_pcm_frame(src->sound, 0);
        }
    }

    void AudioSystem::Restart(AudioSourceID id) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* src = GetSource(id);
        if (src && src->sound) {
            ma_sound_seek_to_pcm_frame(src->sound, 0);
            ma_sound_start(src->sound);
        }
    }

    bool AudioSystem::IsPlaying(AudioSourceID id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto* src = GetSource(id);
        if (!src || !src->sound) return false;
        return ma_sound_is_playing(
            const_cast<ma_sound*>(src->sound)) == MA_TRUE;
    }

    bool AudioSystem::IsLooping(AudioSourceID id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto* src = GetSource(id);
        if (!src) return false;
        return src->looping;
    }

    void AudioSystem::SetLooping(AudioSourceID id, bool loop) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* src = GetSource(id);
        if (src && src->sound) {
            src->looping = loop;
            ma_sound_set_looping(src->sound, loop);
        }
    }

    // -- Volume + Pitch ----------------------------------------
    void AudioSystem::SetVolume(AudioSourceID id, f32 vol) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* src = GetSource(id);
        if (src && src->sound) {
            src->volume = vol;
            ma_sound_set_volume(src->sound, vol);
        }
    }

    f32 AudioSystem::GetVolume(AudioSourceID id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto* src = GetSource(id);
        return src ? src->volume : 0.0f;
    }

    void AudioSystem::SetPitch(AudioSourceID id, f32 pitch) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* src = GetSource(id);
        if (src && src->sound) {
            src->pitch = pitch;
            ma_sound_set_pitch(src->sound, pitch);
        }
    }

    f32 AudioSystem::GetPitch(AudioSourceID id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        const auto* src = GetSource(id);
        return src ? src->pitch : 1.0f;
    }

    // -- 3D Audio ----------------------------------------------
    void AudioSystem::SetSourcePosition(
        AudioSourceID id, const Vec3& pos
    ) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto* src = GetSource(id);
        if (src && src->sound && src->is3D) {
            src->position = pos;
            ma_sound_set_position(
                src->sound, pos.x, pos.y, pos.z);
        }
    }

    void AudioSystem::SetListenerPosition(const Vec3& pos) {
        if (!initialized_) return;
        ma_engine_listener_set_position(
            engine_, 0, pos.x, pos.y, pos.z);
    }

    void AudioSystem::SetListenerOrientation(
        const Vec3& forward, const Vec3& up
    ) {
        if (!initialized_) return;
        ma_engine_listener_set_direction(
            engine_, 0,
            forward.x, forward.y, forward.z);
        ma_engine_listener_set_world_up(
            engine_, 0,
            up.x, up.y, up.z);
    }

    // -- Master volume -----------------------------------------
    void AudioSystem::SetMasterVolume(f32 volume) {
        masterVolume_ = volume;
        if (initialized_) {
            ma_engine_set_volume(engine_, volume);
        }
    }

    f32 AudioSystem::GetMasterVolume() const {
        return masterVolume_;
    }

    // -- PlayOneShot -------------------------------------------
    AudioSourceID AudioSystem::PlayOneShot(
        AudioClipID clipID, f32 volume, f32 pitch
    ) {
        AudioSourceDesc desc;
        desc.clipID      = clipID;
        desc.volume      = volume;
        desc.pitch       = pitch;
        desc.looping     = false;
        desc.playOnCreate= true;
        desc.is3D        = false;

        auto result = CreateSource(desc);
        if (result.IsErr()) return INVALID_SOURCE;

        AudioSourceID srcID = result.Value();

        // Mark as oneShot so Update() cleans it up
        std::lock_guard<std::mutex> lock(mutex_);
        auto* src = GetSource(srcID);
        if (src) src->oneShot = true;

        return srcID;
    }

    // -- Stats -------------------------------------------------
    u32 AudioSystem::GetActiveSourceCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        u32 count = 0;
        for (auto& [id, src] : sources_) {
            if (src.sound &&
                ma_sound_is_playing(src.sound) == MA_TRUE) {
                count++;
            }
        }
        return count;
    }

    u32 AudioSystem::GetLoadedClipCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<u32>(clips_.size());
    }

    // -- Private helpers ---------------------------------------
    AudioClipData* AudioSystem::GetClip(AudioClipID id) {
        auto it = clips_.find(id);
        return it != clips_.end() ? &it->second : nullptr;
    }

    AudioSourceData* AudioSystem::GetSource(
        AudioSourceID id
    ) {
        auto it = sources_.find(id);
        return it != sources_.end()
            ? &it->second : nullptr;
    }

    const AudioSourceData* AudioSystem::GetSource(
        AudioSourceID id
    ) const {
        auto it = sources_.find(id);
        return it != sources_.end()
            ? &it->second
            : nullptr;
    }

    // -- AudioModule -------------------------------------------
    AudioModule::AudioModule()  = default;
    AudioModule::~AudioModule() = default;

    VoidResult AudioModule::Initialize(
        const ModuleInitParams& params
    ) {
        ILogger* log = nullptr;
        if (params.context) {
            log = params.context->Logger();
        }

        if (log) log->Info("Audio","Initializing...");

        audio_ = std::make_unique<AudioSystem>();
        auto r = audio_->Initialize();
        if (r.IsErr()) {
            if (log) log->Error("Audio",
                "Failed: " + r.Error().message);
            return r;
        }

        if (params.context) {
            params.context->Register<IAudio>(audio_.get());
        }

        if (log) log->Info("Audio","Audio system ready.");

        return VoidResult::Ok();
    }

    void AudioModule::OnUpdate(f32 dt) {
        if (audio_) audio_->Update(dt);
    }

    void AudioModule::Shutdown() {
        std::cout << "[Audio] Shutting down...\n";
        if (audio_) audio_->Shutdown();
        audio_.reset();
        std::cout << "[Audio] Shutdown complete.\n";
    }

    ModuleDescriptor AudioModule::GetDescriptor() const {
        ModuleDescriptor d;
        d.name        = "Audio";
        d.version     = "0.1.0";
        d.apiVersion  = RIFTCORE_API_VERSION;
        d.description = "miniaudio sound system";
        return d;
    }

    RIFTCORE_IMPLEMENT_MODULE(AudioModule)

} // namespace RiftCore

