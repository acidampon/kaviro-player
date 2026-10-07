#pragma once
#include "ump/FfmpegMediaSession.h"
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace ump::native {
enum class HardwareDecodeMode { Disabled, Preferred, Required };
enum class NativeEngineState { Closed, Open, Playing, Paused, Error };

struct NativeTrack {
    int streamIndex{-1};
    FfmpegStreamType type{FfmpegStreamType::Video};
    std::string codec, language, title;
    bool selected{false};
};

struct NativeEngineInfo {
    std::string name{"KAVIRO Bundled Native FFmpeg Engine"};
    std::string version, buildId, license, upstreamSource;
    bool standalone{false}, available{false}, hardwareDecode{false};
};

class VideoOutput { public: virtual ~VideoOutput() = default; virtual bool present(const FfmpegDecodedFrame&) = 0; virtual void reset() {} };
class AudioOutput { public: virtual ~AudioOutput() = default; virtual bool write(const FfmpegDecodedFrame&) = 0; virtual void reset() {};
    // Returns the media position currently represented by the audio device clock,
    // or a negative value when the output cannot provide one.
    virtual std::int64_t clockPositionUs() const { return -1; }
};

class NativeMediaEngine final {
public:
    NativeMediaEngine();
    ~NativeMediaEngine();
    NativeMediaEngine(const NativeMediaEngine&) = delete;
    NativeMediaEngine& operator=(const NativeMediaEngine&) = delete;
    bool open(const std::filesystem::path&, bool recoveryMode=false);
    void close();
    bool isOpen() const noexcept;
    bool play();
    bool pause();
    bool seekMs(std::int64_t);
    std::int64_t positionMs() const noexcept;
    std::int64_t durationMs() const noexcept;
    bool setSpeed(double);
    double speed() const noexcept;
    bool selectAudioTrack(int);
    bool selectVideoTrack(int);
    std::vector<NativeTrack> tracks() const;
    void setHardwareDecodeMode(HardwareDecodeMode) noexcept;
    HardwareDecodeMode hardwareDecodeMode() const noexcept;
    bool hardwareDecodeActive() const noexcept;
    void attachVideoOutput(VideoOutput*) noexcept;
    void detachVideoOutput(VideoOutput*) noexcept;
    void attachAudioOutput(AudioOutput*) noexcept;
    void detachAudioOutput(AudioOutput*) noexcept;
    bool pump(std::size_t maxFrames=0);
    FfmpegRecoveryOutcome recoveryOutcome() const noexcept;
    NativeEngineState state() const noexcept;
    std::string lastError() const;
    NativeEngineInfo info() const;
    bool standaloneReady() const noexcept;
private:
    FfmpegMediaSession session_;
    VideoOutput* videoOutput_{nullptr};
    AudioOutput* audioOutput_{nullptr};
    HardwareDecodeMode hardwareMode_{HardwareDecodeMode::Preferred};
    NativeEngineState state_{NativeEngineState::Closed};
    double speed_{1.0};
    std::string error_;

    PlaybackClock clock_{};
    std::chrono::steady_clock::time_point clockWall_{};
    bool clockInitialized_{false};
    bool pendingVideo_{false};
    FfmpegDecodedFrame pendingVideoFrame_{};
};
} // namespace ump::native
