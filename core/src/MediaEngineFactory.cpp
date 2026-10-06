#include "ump/MediaEngineFactory.h"

namespace ump {

std::unique_ptr<PlaybackEngine> createDevelopmentEngine() {
    return std::make_unique<ExternalPlayerAdapter>();
}

std::unique_ptr<PlaybackEngine> createBundledEngine() {
    // Deliberately unavailable until a selected native engine is linked.
    return nullptr;
}

std::unique_ptr<PlaybackEngine> createBestAvailableEngine() {
    if (auto bundled = createBundledEngine()) return bundled;
    return createDevelopmentEngine();
}

} // namespace ump
