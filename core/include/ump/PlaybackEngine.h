#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include "ump/MediaEngineInfo.h"

namespace ump {

enum class EngineCapability {
    Pause,
    Seek,
    Speed,
    Volume,
    Subtitles,
    AudioTracks,
    HardwareAcceleration
};

enum class TrackType { Audio, Video, Subtitle };

struct MediaTrack {
    std::string id;
    TrackType type{TrackType::Audio};
    std::string language;
    std::string title;
    std::string codec;
    bool selected{false};
};

enum class EngineState { Idle, Opening, Playing, Paused, Stopped, Error };

class PlaybackEngine {
public:
    virtual ~PlaybackEngine() = default;
    virtual bool play(const std::string& source) = 0;
    virtual bool pause() { return false; }
    virtual bool seek(std::int64_t /*positionMs*/) { return false; }
    virtual bool setSpeed(double /*speed*/) { return false; }
    virtual bool setVolume(double /*volume*/) { return false; }
    virtual bool selectTrack(const std::string& /*trackId*/) { return false; }
    virtual std::vector<MediaTrack> tracks() const { return {}; }
    virtual void stop() = 0;
    virtual bool isPlaying() const noexcept = 0;
    virtual EngineState state() const noexcept { return isPlaying() ? EngineState::Playing : EngineState::Idle; }
    virtual bool supports(EngineCapability /*capability*/) const noexcept { return false; }
    virtual MediaEngineInfo info() const { return {}; }
};

class ExternalPlayerAdapter final : public PlaybackEngine {
public:
    bool play(const std::string& source) override;
    bool pause() override;
    bool seek(std::int64_t positionMs) override;
    bool setSpeed(double speed) override;
    bool setVolume(double volume) override;
    void stop() override;
    bool isPlaying() const noexcept override { return playing_; }
    EngineState state() const noexcept override { return state_; }
    bool supports(EngineCapability capability) const noexcept override;
    MediaEngineInfo info() const override;
private:
    bool playing_{false};
    EngineState state_{EngineState::Idle};
};

} // namespace ump
