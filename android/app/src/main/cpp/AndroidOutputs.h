#pragma once

#include "ump/native/NativeMediaEngine.h"

#include <aaudio/AAudio.h>
#include <android/native_window.h>
#include <cstdint>
#include <mutex>

namespace kaviro::android {

class AndroidAudioOutput final : public ump::native::AudioOutput {
public:
    AndroidAudioOutput() = default;
    ~AndroidAudioOutput() override;
    AndroidAudioOutput(const AndroidAudioOutput&) = delete;
    AndroidAudioOutput& operator=(const AndroidAudioOutput&) = delete;

    bool write(const ump::FfmpegDecodedFrame& frame) override;
    void close();

private:
    bool ensureStream(int sampleRate, int channels);
    void closeLocked();

    std::mutex mutex_;
    AAudioStream* stream_{nullptr};
    int sampleRate_{0};
    int channels_{0};
};

class AndroidVideoOutput final : public ump::native::VideoOutput {
public:
    AndroidVideoOutput() = default;
    ~AndroidVideoOutput() override;
    AndroidVideoOutput(const AndroidVideoOutput&) = delete;
    AndroidVideoOutput& operator=(const AndroidVideoOutput&) = delete;

    void setWindow(ANativeWindow* window);
    bool present(const ump::FfmpegDecodedFrame& frame) override;
    void close();

private:
    std::mutex mutex_;
    ANativeWindow* window_{nullptr};
};

} // namespace kaviro::android
