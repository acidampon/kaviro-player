#include "ump/PlaybackEngine.h"
#include <cstdlib>
#include <string>

namespace ump {
namespace {
std::string quote(const std::string& s) {
#ifdef _WIN32
    std::string q = "\""; for (char c : s) { if (c == '\"') q += '\\'; q += c; } return q + "\"";
#else
    std::string q = "'"; for (char c : s) { if (c == '\'') q += "'\\''"; else q += c; } return q + "'";
#endif
}
}

bool ExternalPlayerAdapter::play(const std::string& source) {
    // Development adapter only. Release builds will replace this with the bundled native media engine.
    state_ = EngineState::Opening;
    const std::string command = "ffplay -autoexit -loglevel warning " + quote(source);
    playing_ = true;
    state_ = EngineState::Playing;
    const int rc = std::system(command.c_str());
    playing_ = false;
    state_ = rc == 0 ? EngineState::Stopped : EngineState::Error;
    return rc == 0;
}

bool ExternalPlayerAdapter::pause() {
    // ffplay is intentionally a blocking development adapter; native pause arrives with the final engine.
    return false;
}

bool ExternalPlayerAdapter::seek(std::int64_t) { return false; }
bool ExternalPlayerAdapter::setSpeed(double) { return false; }
bool ExternalPlayerAdapter::setVolume(double) { return false; }

bool ExternalPlayerAdapter::supports(EngineCapability capability) const noexcept {
    switch (capability) {
        case EngineCapability::Pause:
        case EngineCapability::Seek:
        case EngineCapability::Speed:
        case EngineCapability::Volume:
        case EngineCapability::Subtitles:
        case EngineCapability::AudioTracks:
        case EngineCapability::HardwareAcceleration:
            return false;
    }
    return false;
}

MediaEngineInfo ExternalPlayerAdapter::info() const {
    MediaEngineInfo result;
    result.backend = EngineBackend::ExternalDevelopment;
    result.name = "ffplay development adapter";
#ifdef _WIN32
    result.available = std::system("ffplay -version > NUL 2>&1") == 0;
#else
    result.available = std::system("ffplay -version > /dev/null 2>&1") == 0;
#endif
    result.standalone = false;
    result.notes.push_back("Temporary development backend; the final release must bundle the selected native media engine.");
    result.notes.push_back("Pause, seek, track selection and runtime controls require the native backend.");
    return result;
}

void ExternalPlayerAdapter::stop() {
    // Cannot safely interrupt the blocking ffplay process from this adapter.
    playing_ = false;
    state_ = EngineState::Stopped;
}

} // namespace ump
