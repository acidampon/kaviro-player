#include "AndroidOutputs.h"

#include <algorithm>
#include <cstring>

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

bool AndroidAudioOutput::write(const ump::FfmpegDecodedFrame& frame) {
    if (frame.type != ump::FfmpegStreamType::Audio ||
        !frame.normalized || frame.samples <= 0 || frame.channels <= 0 ||
        frame.sampleRate <= 0 || frame.ownedData.empty()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    if (!ensureStream(frame.sampleRate, frame.channels)) return false;

    const int32_t frames = static_cast<int32_t>(frame.samples);
    const auto* data = frame.ownedData.data();
    std::size_t remaining = frame.ownedData.size();
    int32_t offsetFrames = 0;
    const std::size_t bytesPerFrame = static_cast<std::size_t>(frame.channels) * sizeof(std::int16_t);

    while (remaining >= bytesPerFrame) {
        const int32_t availableFrames = static_cast<int32_t>(remaining / bytesPerFrame);
        const aaudio_result_t written = AAudioStream_write(
            stream_,
            data + static_cast<std::size_t>(offsetFrames) * bytesPerFrame,
            std::min(frames - offsetFrames, availableFrames),
            20000);
        if (written < 0) return false;
        if (written == 0) return false;
        offsetFrames += static_cast<int32_t>(written);
        remaining -= static_cast<std::size_t>(written) * bytesPerFrame;
        if (offsetFrames >= frames) break;
    }
    return offsetFrames == frames;
}

void AndroidAudioOutput::closeLocked() {
    if (stream_ != nullptr) {
        AAudioStream_requestStop(stream_);
        AAudioStream_close(stream_);
        stream_ = nullptr;
    }
    sampleRate_ = 0;
    channels_ = 0;
}

void AndroidAudioOutput::reset() { close(); }

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
