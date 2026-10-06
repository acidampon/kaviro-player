#pragma once
#include <string>
#include <vector>

namespace ump {

enum class EngineBackend { BundledNative, ExternalDevelopment, Unavailable };

struct MediaEngineInfo {
    EngineBackend backend{EngineBackend::Unavailable};
    std::string name;
    std::string version;
    bool available{false};
    bool standalone{false};
    std::vector<std::string> notes;
};

} // namespace ump
