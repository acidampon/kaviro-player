#include "ump/FfmpegMediaSession.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>

namespace {

class CountingSink final : public ump::FfmpegFrameSink {
public:
    bool onFrame(const ump::FfmpegDecodedFrame& frame) override {
        ++frames;
        if (!frame.normalized) {
            error = "decoded frame is not normalized";
            return false;
        }
        if (frame.type == ump::FfmpegStreamType::Video) ++videoFrames;
        if (frame.type == ump::FfmpegStreamType::Audio) ++audioFrames;
        if (frame.ownedData.empty() || frame.planes.empty()) {
            error = "decoded frame has no owned payload";
            return false;
        }
        for (const auto& plane : frame.planes) {
            if (plane.data == nullptr || plane.linesize <= 0 ||
                plane.width <= 0 || plane.height <= 0) {
                error = "decoded frame contains an invalid plane";
                return false;
            }
        }
        return true;
    }

    std::size_t frames{0};
    std::size_t videoFrames{0};
    std::size_t audioFrames{0};
    std::string error;
};

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: kaviro_native_decode_smoke <media-file>\n";
        return 2;
    }

    ump::FfmpegMediaSession session;
    ump::FfmpegOpenOptions options;
    options.hardwareDecodePreferred = false;

    if (!session.open(argv[1], options)) {
        std::cerr << "open failed: " << session.lastError() << "\n";
        return 1;
    }

    if (!session.standaloneReady()) {
        std::cerr << "session is not standalone-ready\n";
        return 1;
    }

    CountingSink sink;
    if (!session.decodeToSink(sink, 3)) {
        std::cerr << "decode failed: " << session.lastError() << "\n";
        return 1;
    }

    if (sink.frames == 0 || sink.videoFrames == 0 || sink.audioFrames == 0 || !sink.error.empty()) {
        std::cerr << "invalid decoded output: " << sink.error << "\n";
        return 1;
    }

    int firstAudio = -1;
    int secondAudio = -1;
    for (const auto& stream : session.streams()) {
        if (stream.type == ump::FfmpegStreamType::Audio) {
            if (firstAudio < 0) firstAudio = stream.index;
            else if (secondAudio < 0) secondAudio = stream.index;
        }
    }

    if (secondAudio >= 0) {
        if (!session.selectAudioTrack(secondAudio)) {
            std::cerr << "second audio track selection failed: "
                      << session.lastError() << "\n";
            return 1;
        }
        if (!session.seekMs(0)) {
            std::cerr << "track-switch seek failed: " << session.lastError() << "\n";
            return 1;
        }

        CountingSink switchedSink;
        if (!session.decodeToSink(switchedSink, 3)) {
            std::cerr << "second-track decode failed: "
                      << session.lastError() << "\n";
            return 1;
        }
        if (switchedSink.audioFrames == 0 || !switchedSink.error.empty()) {
            std::cerr << "second-track output invalid: "
                      << switchedSink.error << "\n";
            return 1;
        }

        if (session.selectAudioTrack(999999)) {
            std::cerr << "invalid audio track was accepted\n";
            return 1;
        }
        bool secondStillSelected = false;
        for (const auto& stream : session.streams()) {
            if (stream.index == secondAudio &&
                stream.type == ump::FfmpegStreamType::Audio) {
                secondStillSelected = stream.selected;
            }
        }
        if (!secondStillSelected) {
            std::cerr << "invalid track selection cleared the active track\n";
            return 1;
        }
    }

    if (!session.seekMs(0)) {
        std::cerr << "seek failed: " << session.lastError() << "\n";
        return 1;
    }

    std::cout << "KAVIRO native decode smoke: PASS (" << sink.frames
              << " frames";
    if (firstAudio >= 0 && secondAudio >= 0) std::cout << ", track switching verified";
    std::cout << ")\n";
    return 0;
}
