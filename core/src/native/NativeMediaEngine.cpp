#include "ump/native/NativeMediaEngine.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <utility>

namespace ump::native {

namespace {

constexpr std::int64_t kVideoEarlyToleranceUs = 20'000;
constexpr std::int64_t kVideoLateToleranceUs = 250'000;

class OutputSink final : public FfmpegFrameSink {
public:
    using VideoHandler = std::function<bool(const FfmpegDecodedFrame&)>;

    OutputSink(VideoOutput* video, AudioOutput* audio, VideoHandler handler)
        : video_(video), audio_(audio), videoHandler_(std::move(handler)) {}

    bool onFrame(const FfmpegDecodedFrame& frame) override {
        if (frame.type == FfmpegStreamType::Video) {
            if (video_ == nullptr) return true;
            return videoHandler_ ? videoHandler_(frame) : video_->present(frame);
        }
        if (frame.type == FfmpegStreamType::Audio) {
            if (audio_ == nullptr) return true;
            if (!audio_->write(frame)) { rejected_ = true; return false; }
            return true;
        }
        return true;
    }

    bool rejected() const noexcept { return rejected_; }

private:
    VideoOutput* video_;
    AudioOutput* audio_;
    VideoHandler videoHandler_;
    bool rejected_{false};
};

} // namespace

NativeMediaEngine::NativeMediaEngine() = default;

NativeMediaEngine::~NativeMediaEngine() {
    close();
}

bool NativeMediaEngine::open(const std::filesystem::path& path, bool recoveryMode) {
    close();
    error_.clear();

    FfmpegOpenOptions options;
    options.hardwareDecodePreferred =
        hardwareMode_ != HardwareDecodeMode::Disabled;
    options.recoveryMode = recoveryMode;

    if (!session_.open(path, options)) {
        error_ = session_.lastError();
        state_ = NativeEngineState::Error;
        return false;
    }

    state_ = NativeEngineState::Open;
    clock_ = {};
    clock_.speed = speed_;
    clock_.paused = true;
    clockWall_ = {};
    clockInitialized_ = false;
    pendingVideo_ = false;
    pendingVideoFrame_ = {};
    return true;
}

void NativeMediaEngine::close() {
    session_.close();
    error_.clear();
    state_ = NativeEngineState::Closed;
    clock_ = {};
    clockWall_ = {};
    clockInitialized_ = false;
    pendingVideo_ = false;
    pendingVideoFrame_ = {};
}

bool NativeMediaEngine::isOpen() const noexcept {
    return session_.isOpen();
}

bool NativeMediaEngine::play() {
    if (!session_.isOpen()) {
        error_ = "Media session is not open";
        state_ = NativeEngineState::Error;
        return false;
    }
    if (state_ == NativeEngineState::Error) {
        return false;
    }

    const auto now = std::chrono::steady_clock::now();
    clock_.speed = speed_;
    clock_.paused = false;
    clockWall_ = now;
    // Establish a valid monotonic wall-clock anchor immediately. This matters
    // for audio-only media, which may not produce a video frame to initialize
    // the playback clock later.
    if (!clockInitialized_) {
        clockInitialized_ = true;
        clock_.wallUs = 0;
    }
    state_ = NativeEngineState::Playing;
    return true;
}

bool NativeMediaEngine::pause() {
    if (!session_.isOpen()) {
        error_ = "Media session is not open";
        state_ = NativeEngineState::Error;
        return false;
    }

    if (state_ == NativeEngineState::Playing && clockInitialized_) {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            now - clockWall_).count();
        if (elapsed > 0) {
            clock_ = advancePlaybackClock(clock_, elapsed);
        }
    }
    clock_.paused = true;
    state_ = NativeEngineState::Paused;
    return true;
}

bool NativeMediaEngine::stop() {
    if (!session_.isOpen()) {
        error_ = "Media session is not open";
        state_ = NativeEngineState::Closed;
        return false;
    }
    if (!session_.seekMs(0)) {
        error_ = session_.lastError();
        state_ = NativeEngineState::Error;
        return false;
    }
    if (videoOutput_ != nullptr) videoOutput_->reset();
    if (audioOutput_ != nullptr) audioOutput_->reset();
    pendingVideo_ = false;
    pendingVideoFrame_ = {};
    clock_ = {};
    clock_.speed = speed_;
    clock_.paused = true;
    clockInitialized_ = true;
    clockWall_ = std::chrono::steady_clock::now();
    error_.clear();
    state_ = NativeEngineState::Paused;
    return true;
}

bool NativeMediaEngine::seekMs(std::int64_t positionMs) {
    if (!session_.isOpen() || positionMs < 0) {
        error_ = "Invalid seek request";
        state_ = session_.isOpen() ? NativeEngineState::Error
                                   : NativeEngineState::Closed;
        return false;
    }

    if (!session_.seekMs(positionMs)) {
        error_ = session_.lastError();
        state_ = NativeEngineState::Error;
        return false;
    }

    error_.clear();
    if (videoOutput_ != nullptr) videoOutput_->reset();
    if (audioOutput_ != nullptr) audioOutput_->reset();
    pendingVideo_ = false;
    pendingVideoFrame_ = {};
    clock_.mediaUs = positionMs > std::numeric_limits<std::int64_t>::max() / 1000
        ? std::numeric_limits<std::int64_t>::max()
        : positionMs * 1000;
    clock_.wallUs = 0;
    clock_.speed = speed_;
    clock_.paused = state_ != NativeEngineState::Playing;
    clockWall_ = std::chrono::steady_clock::now();
    clockInitialized_ = true;
    if (state_ == NativeEngineState::Error) {
        state_ = NativeEngineState::Paused;
    }
    return true;
}

bool NativeMediaEngine::setSpeed(double speedValue) {
    if (!std::isfinite(speedValue) || speedValue <= 0.0 ||
        speedValue > 16.0) {
        error_ = "Playback speed must be finite and in the range (0, 16]";
        state_ = session_.isOpen() ? NativeEngineState::Error
                                   : NativeEngineState::Closed;
        return false;
    }

    if (state_ == NativeEngineState::Playing && clockInitialized_) {
        const auto now = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            now - clockWall_).count();
        if (elapsed > 0) {
            clock_ = advancePlaybackClock(clock_, elapsed);
        }
        clockWall_ = now;
    }

    speed_ = speedValue;
    clock_.speed = speedValue;
    error_.clear();
    return true;
}

double NativeMediaEngine::speed() const noexcept {
    return speed_;
}

std::int64_t NativeMediaEngine::positionMs() const noexcept {
    if (!session_.isOpen()) return 0;
    const auto mediaUs = std::max<std::int64_t>(0, clock_.mediaUs);
    return mediaUs / 1000;
}

std::int64_t NativeMediaEngine::durationMs() const noexcept {
    if (!session_.isOpen()) return 0;
    std::int64_t duration = 0;
    for (const auto& stream : session_.streams()) {
        if (stream.type == FfmpegStreamType::Audio ||
            stream.type == FfmpegStreamType::Video) {
            duration = std::max(duration, stream.durationMs);
        }
    }
    return std::max<std::int64_t>(0, duration);
}


bool NativeMediaEngine::selectAudioTrack(int streamIndex) {
    if (!session_.selectAudioTrack(streamIndex)) {
        error_ = session_.lastError();
        if (session_.isOpen()) state_ = NativeEngineState::Error;
        return false;
    }
    if (audioOutput_ != nullptr) audioOutput_->reset();
    pendingVideo_ = false;
    pendingVideoFrame_ = {};
    error_.clear();
    return true;
}

bool NativeMediaEngine::selectVideoTrack(int streamIndex) {
    if (!session_.selectVideoTrack(streamIndex)) {
        error_ = session_.lastError();
        if (session_.isOpen()) state_ = NativeEngineState::Error;
        return false;
    }
    if (videoOutput_ != nullptr) videoOutput_->reset();
    pendingVideo_ = false;
    pendingVideoFrame_ = {};
    error_.clear();
    return true;
}

std::vector<NativeTrack> NativeMediaEngine::tracks() const {
    std::vector<NativeTrack> result;
    result.reserve(session_.streams().size());

    for (const auto& stream : session_.streams()) {
        NativeTrack track;
        track.streamIndex = stream.index;
        track.type = stream.type;
        track.codec = stream.codec;
        track.language = stream.language;
        track.title = stream.title;
        track.selected = stream.selected;
        result.push_back(std::move(track));
    }
    return result;
}

void NativeMediaEngine::setHardwareDecodeMode(HardwareDecodeMode mode) noexcept {
    hardwareMode_ = mode;
}

HardwareDecodeMode NativeMediaEngine::hardwareDecodeMode() const noexcept {
    return hardwareMode_;
}

bool NativeMediaEngine::hardwareDecodeActive() const noexcept {
    return false;
}

void NativeMediaEngine::attachVideoOutput(VideoOutput* output) noexcept {
    videoOutput_ = output;
}

void NativeMediaEngine::detachVideoOutput(VideoOutput* output) noexcept {
    if (videoOutput_ == output) {
        videoOutput_ = nullptr;
    }
}

void NativeMediaEngine::attachAudioOutput(AudioOutput* output) noexcept {
    audioOutput_ = output;
}

void NativeMediaEngine::detachAudioOutput(AudioOutput* output) noexcept {
    if (audioOutput_ == output) {
        audioOutput_ = nullptr;
    }
}

bool NativeMediaEngine::pump(std::size_t maxFrames) {
    if (!session_.isOpen()) {
        error_ = "Media session is not open";
        state_ = NativeEngineState::Closed;
        return false;
    }
    if (state_ != NativeEngineState::Playing) {
        return true;
    }

    const auto now = std::chrono::steady_clock::now();

    // Prefer the audio device clock when the output can provide one. This
    // makes audio the timing master while retaining the monotonic wall clock
    // fallback for outputs that do not expose hardware position.
    if (audioOutput_ != nullptr && speed_ == 1.0) {
        const auto audioUs = audioOutput_->clockPositionUs();
        if (audioUs >= 0) {
            clock_.mediaUs = audioUs;
            clock_.speed = speed_;
            clock_.paused = false;
            clockWall_ = now;
            clockInitialized_ = true;
        }
    }

    if (!clockInitialized_) {
        clock_.speed = speed_;
        clock_.paused = false;
        clockWall_ = now;
    } else {
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            now - clockWall_).count();
        if (elapsed > 0) {
            clock_ = advancePlaybackClock(clock_, elapsed);
        }
        clockWall_ = now;
    }

    if (pendingVideo_) {
        const auto delta = clockDeltaUs(clock_, pendingVideoFrame_.ptsUs);
        if (delta < -kVideoLateToleranceUs) {
            // The frame was held because it was early, but the caller did not
            // pump again soon enough. Never present a now-stale frame.
            pendingVideo_ = false;
            pendingVideoFrame_ = {};
        } else if (delta <= kVideoEarlyToleranceUs) {
            if (videoOutput_ != nullptr && !videoOutput_->present(pendingVideoFrame_)) {
                error_ = "Video output rejected a decoded frame";
                state_ = NativeEngineState::Error;
                return false;
            }
            pendingVideo_ = false;
            pendingVideoFrame_ = {};
        } else {
            return true;
        }
    }

    auto videoHandler = [this](const FfmpegDecodedFrame& frame) -> bool {
        if (!clockInitialized_) {
            clock_.mediaUs = frame.ptsUs;
            clock_.wallUs = 0;
            clock_.speed = speed_;
            clock_.paused = false;
            clockWall_ = std::chrono::steady_clock::now();
            clockInitialized_ = true;
        }

        if (audioOutput_ != nullptr && speed_ == 1.0) {
            const auto audioUs = audioOutput_->clockPositionUs();
            if (audioUs >= 0) {
                clock_.mediaUs = audioUs;
                clock_.speed = speed_;
                clock_.paused = false;
                clockWall_ = std::chrono::steady_clock::now();
                clockInitialized_ = true;
            }
        }

        const auto delta = clockDeltaUs(clock_, frame.ptsUs);
        if (delta > kVideoEarlyToleranceUs) {
            pendingVideoFrame_ = frame;
            pendingVideo_ = true;
            return false;
        }

        // A decoder can deliver frames after the wall-clock position has
        // already moved well beyond their presentation timestamp. Presenting
        // those stale frames makes playback visibly lag and can create a
        // backlog. Drop only frames outside the explicit late tolerance;
        // frames within tolerance are still presented to avoid unnecessary
        // cadence loss.
        if (delta < -kVideoLateToleranceUs) {
            return true;
        }

        if (videoOutput_ == nullptr) return true;
        if (!videoOutput_->present(frame)) {
            error_ = "Video output rejected a decoded frame";
            return false;
        }
        return true;
    };

    OutputSink sink(videoOutput_, audioOutput_, std::move(videoHandler));
    const bool decoded = session_.decodeToSink(sink, maxFrames);

    if (sink.rejected() && !pendingVideo_) {
        if (error_.empty()) error_ = "Playback output rejected a decoded frame";
        state_ = NativeEngineState::Error;
        return false;
    }

    if (!decoded) {
        if (pendingVideo_) {
            error_.clear();
            return true;
        }

        if (!error_.empty()) {
            state_ = NativeEngineState::Error;
            return false;
        }

        const auto sessionError = session_.lastError();
        if (!sessionError.empty()) {
            error_ = sessionError;
            state_ = NativeEngineState::Error;
            return false;
        }

        if (session_.ended()) {
            state_ = NativeEngineState::Ended;
            clock_.paused = true;
            return false;
        }

        state_ = NativeEngineState::Paused;
        clock_.paused = true;
        return false;
    }

    error_.clear();
    return true;
}

FfmpegRecoveryOutcome NativeMediaEngine::recoveryOutcome() const noexcept {
    return session_.recoveryOutcome();
}

bool NativeMediaEngine::ended() const noexcept {
    return state_ == NativeEngineState::Ended;
}

NativeEngineState NativeMediaEngine::state() const noexcept {
    return state_;
}

std::string NativeMediaEngine::lastError() const {
    return error_.empty() ? session_.lastError() : error_;
}

NativeEngineInfo NativeMediaEngine::info() const {
    NativeEngineInfo result;
    result.version = "9.0.2";
    result.buildId = "kaviro-ffmpeg-9.0.2";
    result.license = "FFmpeg 9.0.2 (license depends on build configuration)";
    result.upstreamSource = "https://ffmpeg.org/";
    result.standalone = session_.standaloneReady();
    result.available = session_.isOpen();
    result.hardwareDecode = hardwareDecodeActive();
    return result;
}

bool NativeMediaEngine::standaloneReady() const noexcept {
    return session_.standaloneReady();
}

} // namespace ump::native
