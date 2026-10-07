#include "AndroidOutputs.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <time.h>

namespace kaviro::android {

AndroidAudioOutput::~AndroidAudioOutput() {
    close();
}

bool AndroidAudioOutput::ensureStream(int sampleRate, int channels) {
    if (sampleRate <= 0 || channels <= 0 || channels > 8) return false;
    if (stream_ != nullptr && sampleRate_ == sampleRate && channels_ == channels) return true;

    closeLocked();

    AAudioStreamBuilder* builder = nullptr;
    if (AAudio_createStreamBuilder(&builder) != AAUDIO_OK || builder == nullptr) return false;

    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setPerformanceMode(builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_I16);
    AAudioStreamBuilder_setSampleRate(builder, sampleRate);
    AAudioStreamBuilder_setChannelCount(builder, channels);

    const aaudio_result_t result = AAudioStreamBuilder_openStream(builder, &stream_);
    AAudioStreamBuilder_delete(builder);
    if (result != AAUDIO_OK || stream_ == nullptr) {
        stream_ = nullptr;
        return false;
    }

    sampleRate_ = sampleRate;
    channels_ = channels;

    if (AAudioStream_requestStart(stream_) != AAUDIO_OK) {
        closeLocked();
        return false;
    }
    return true;
}

bool AndroidAudioOutput::appendPendingLocked(const std::uint8_t* data, std::size_t bytes,
                                               int sampleRate, int channels,
                                               std::int64_t ptsUs) {
    if (data == nullptr || bytes == 0 || sampleRate <= 0 || channels <= 0) return false;
    if (pendingAudio_.size() - pendingOffset_ + bytes > kMaxPendingAudioBytes) return false;
    if (pendingOffset_ > 0) {
        pendingAudio_.erase(pendingAudio_.begin(),
                            pendingAudio_.begin() + static_cast<std::ptrdiff_t>(pendingOffset_));
        pendingOffset_ = 0;
    }
    if (pendingAudio_.empty()) pendingPtsUs_ = ptsUs;
    pendingAudio_.insert(pendingAudio_.end(), data, data + bytes);
    return true;
}

bool AndroidAudioOutput::flushPendingLocked() {
    if (stream_ == nullptr || sampleRate_ <= 0 || channels_ <= 0) return false;
    const std::size_t bytesPerFrame =
        static_cast<std::size_t>(channels_) * sizeof(std::int16_t);

    while (pendingAudio_.size() > pendingOffset_ + bytesPerFrame) {
        const std::size_t remainingBytes = pendingAudio_.size() - pendingOffset_;
        const int32_t availableFrames =
            static_cast<int32_t>(remainingBytes / bytesPerFrame);
        const auto written = AAudioStream_write(
            stream_,
            pendingAudio_.data() + pendingOffset_,
            availableFrames,
            0);
        if (written == AAUDIO_ERROR_WOULD_BLOCK || written == 0) return true;
        if (written < 0) return false;
        pendingOffset_ += static_cast<std::size_t>(written) * bytesPerFrame;
        if (pendingOffset_ >= pendingAudio_.size()) {
            pendingAudio_.clear();
            pendingOffset_ = 0;
            pendingPtsUs_ = -1;
            break;
        }
    }
    return true;
}

bool AndroidAudioOutput::write(const ump::FfmpegDecodedFrame& frame) {
    if (frame.type != ump::FfmpegStreamType::Audio ||
        !frame.normalized || frame.samples <= 0 || frame.channels <= 0 ||
        frame.sampleRate <= 0 || frame.ownedData.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (!ensureStream(frame.sampleRate, frame.channels)) return false;

    if (!flushPendingLocked()) return false;

    if (!appendPendingLocked(frame.ownedData.data(), frame.ownedData.size(),
                             frame.sampleRate, frame.channels, frame.ptsUs)) {
        return false;
    }

    if (!flushPendingLocked()) return false;

    if (mediaBaseUs_ < 0) {
        std::int64_t framePosition = 0;
        std::int64_t timeNanos = 0;
        const auto timestampResult = AAudioStream_getTimestamp(
            stream_, CLOCK_MONOTONIC, &framePosition, &timeNanos);
        if (timestampResult == AAUDIO_OK && framePosition >= 0) {
            const auto frameDurationUs =
                static_cast<long double>(frame.samples) * 1'000'000.0L /
                static_cast<long double>(frame.sampleRate);
            const auto positionUs =
                static_cast<long double>(framePosition) * 1'000'000.0L /
                static_cast<long double>(frame.sampleRate);
            const auto base =
                static_cast<long double>(frame.ptsUs) + frameDurationUs - positionUs;
            if (base >= 0.0L &&
                base <= static_cast<long double>(std::numeric_limits<std::int64_t>::max())) {
                mediaBaseUs_ = static_cast<std::int64_t>(base);
                streamBaseFrame_ = 0;
            }
        }
    }
    return true;
}

void AndroidAudioOutput::closeLocked() {
    if (stream_ != nullptr) {
        AAudioStream_requestStop(stream_);
        AAudioStream_close(stream_);
        stream_ = nullptr;
    }
    sampleRate_ = 0;
    channels_ = 0;
    mediaBaseUs_ = -1;
    streamBaseFrame_ = -1;
    pendingAudio_.clear();
    pendingOffset_ = 0;
    pendingPtsUs_ = -1;
}

void AndroidAudioOutput::reset() { close(); }

std::int64_t AndroidAudioOutput::clockPositionUs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stream_ == nullptr || sampleRate_ <= 0 ||
        mediaBaseUs_ < 0 || streamBaseFrame_ < 0) {
        return -1;
    }

    std::int64_t framePosition = 0;
    std::int64_t timeNanos = 0;
    const auto result = AAudioStream_getTimestamp(
        stream_, CLOCK_MONOTONIC, &framePosition, &timeNanos);
    if (result != AAUDIO_OK || framePosition < streamBaseFrame_) return -1;

    const auto deltaFrames = framePosition - streamBaseFrame_;
    const auto scaled = static_cast<long double>(deltaFrames) *
                        1'000'000.0L /
                        static_cast<long double>(sampleRate_);
    if (scaled > static_cast<long double>(std::numeric_limits<std::int64_t>::max())) {
        return std::numeric_limits<std::int64_t>::max();
    }
    return mediaBaseUs_ + static_cast<std::int64_t>(scaled);
}

void AndroidAudioOutput::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    closeLocked();
}

AndroidVideoOutput::~AndroidVideoOutput() {
    close();
}

void AndroidVideoOutput::setWindow(ANativeWindow* window) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (window == window_) return;
    if (window_ != nullptr) ANativeWindow_release(window_);
    window_ = window;
    if (window_ != nullptr) ANativeWindow_acquire(window_);
}

bool AndroidVideoOutput::present(const ump::FfmpegDecodedFrame& frame) {
    if (frame.type != ump::FfmpegStreamType::Video ||
        !frame.normalized || frame.width <= 0 || frame.height <= 0 ||
        frame.ownedData.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (window_ == nullptr) return true;

    ANativeWindow_setBuffersGeometry(
        window_, frame.width, frame.height, WINDOW_FORMAT_RGBA_8888);

    ANativeWindow_Buffer buffer{};
    if (ANativeWindow_lock(window_, &buffer, nullptr) != 0) return false;

    const auto* src = frame.ownedData.data();
    const std::size_t srcStride = static_cast<std::size_t>(frame.width) * 4;
    const std::size_t dstStride = static_cast<std::size_t>(buffer.stride) * 4;
    const std::size_t rowBytes = std::min(srcStride, dstStride);
    const int rows = std::min(frame.height, buffer.height);

    for (int y = 0; y < rows; ++y) {
        std::memcpy(
            static_cast<std::uint8_t*>(buffer.bits) +
                static_cast<std::size_t>(y) * dstStride,
            src + static_cast<std::size_t>(y) * srcStride,
            rowBytes);
    }

    return ANativeWindow_unlockAndPost(window_) == 0;
}

void AndroidVideoOutput::reset() { }

void AndroidVideoOutput::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (window_ != nullptr) {
        ANativeWindow_release(window_);
        window_ = nullptr;
    }
}

} // namespace kaviro::android
