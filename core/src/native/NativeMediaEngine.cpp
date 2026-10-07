#include "ump/native/NativeMediaEngine.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace ump::native {

namespace {

class OutputSink final : public FfmpegFrameSink {
public:
    OutputSink(VideoOutput* video, AudioOutput* audio)
        : video_(video), audio_(audio) {}

    bool onFrame(const FfmpegDecodedFrame& frame) override {
        if (frame.type == FfmpegStreamType::Video) {
            return video_ == nullptr || video_->present(frame);
        }
        if (frame.type == FfmpegStreamType::Audio) {
            return audio_ == nullptr || audio_->write(frame);
        }
        // Subtitle output is deliberately not consumed by the audio/video
        // output boundary yet. The demux/decode layer remains responsible for
        // exposing subtitle streams without silently dropping the contract.
        return true;
    }

private:
    VideoOutput* video_;
    AudioOutput* audio_;
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
    return true;
}

void NativeMediaEngine::close() {
    session_.close();
    error_.clear();
    state_ = NativeEngineState::Closed;
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
    state_ = NativeEngineState::Playing;
    return true;
}

bool NativeMediaEngine::pause() {
    if (!session_.isOpen()) {
        error_ = "Media session is not open";
        state_ = NativeEngineState::Error;
        return false;
    }
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
    speed_ = speedValue;
    return true;
}

double NativeMediaEngine::speed() const noexcept {
    return speed_;
}

bool NativeMediaEngine::selectAudioTrack(int streamIndex) {
    if (!session_.selectAudioTrack(streamIndex)) {
        error_ = session_.lastError();
        if (session_.isOpen()) {
            state_ = NativeEngineState::Error;
        }
        return false;
    }
    error_.clear();
    return true;
}

bool NativeMediaEngine::selectVideoTrack(int streamIndex) {
    if (!session_.selectVideoTrack(streamIndex)) {
        error_ = session_.lastError();
        if (session_.isOpen()) {
            state_ = NativeEngineState::Error;
        }
        return false;
    }
    error_.clear();
    return true;
}

std::vector<NativeTrack> NativeMediaEngine::tracks() const {
    std::vector<NativeTrack> result;
    result.reserve(session_.streams().size());

    for (const auto& stream : session_.streams()) {
        if (stream.type == FfmpegStreamType::Subtitle) {
            // Subtitle tracks are discoverable at the session boundary, but
            // have no NativeMediaEngine output selector yet.
        }
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
    // Hardware selection is intentionally reported false until a platform
    // decoder has been created and its frames are proven to cross the output
    // boundary. Merely requesting hardware acceleration is not evidence that
    // it is active.
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

    OutputSink sink(videoOutput_, audioOutput_);
    const bool decoded = session_.decodeToSink(sink, maxFrames);

    if (!decoded) {
        const auto sessionError = session_.lastError();
        if (!sessionError.empty()) {
            error_ = sessionError;
            state_ = NativeEngineState::Error;
            return false;
        }
        // A clean decode stop (for example end-of-file or a sink declining
        // more work) is not a synthetic playback error.
        state_ = NativeEngineState::Paused;
        return false;
    }

    error_.clear();
    return true;
}

FfmpegRecoveryOutcome NativeMediaEngine::recoveryOutcome() const noexcept {
    return session_.recoveryOutcome();
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
