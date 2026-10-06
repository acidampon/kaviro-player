#pragma once

#include <memory>
#include "ump/PlaybackEngine.h"

namespace ump {

// Creates the best engine available for the current build/runtime.
// The final product should resolve to the bundled native backend; the
// external adapter exists only for development and compatibility testing.
std::unique_ptr<PlaybackEngine> createDevelopmentEngine();
std::unique_ptr<PlaybackEngine> createBundledEngine();
std::unique_ptr<PlaybackEngine> createBestAvailableEngine();

} // namespace ump
