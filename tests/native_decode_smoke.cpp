#include "ump/FfmpegMediaSession.h"
#include <iostream>
#include <string>

class CountingSink final : public ump::FfmpegFrameSink {
public:
    std::size_t frames{};
    bool valid{true};

    bool onFrame(const ump::FfmpegDecodedFrame& frame) override {
        ++frames;
        if (frame.planes.empty() || frame.ownedData.empty()) valid = false;
        for (const auto& plane : frame.planes) {
            if (!plane.data || plane.linesize <= 0 || plane.width < 0 || plane.height <= 0)
                valid = false;
        }
        return valid;
    }
};

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: kaviro_native_decode_smoke <media-file>\n";
        return 2;
    }

    ump::FfmpegMediaSession session;
    if (!session.open(argv[1])) {
        std::cerr << "open failed: " << session.lastError() << "\n";
        return 3;
    }
    if (!session.standaloneReady()) return 4;

    CountingSink sink;
    if (!session.decodeToSink(sink, 3)) {
        std::cerr << "decode failed: " << session.lastError() << "\n";
        return 5;
    }
    if (sink.frames == 0 || !sink.valid) return 6;

    if (!session.seekMs(0)) {
        std::cerr << "seek failed: " << session.lastError() << "\n";
        return 7;
    }
    std::cout << "KAVIRO native FFmpeg decode smoke: PASS (" << sink.frames
              << " frames)\n";
    return 0;
}
