#include "ump/FfmpegMediaSession.h"

#include <cassert>
#include <cstdint>
#include <iostream>

int main() {
    ump::PlaybackClock clock;
    clock.mediaUs = 1'000'000;
    clock.wallUs = 0;
    clock.speed = 8.0;
    clock.paused = false;

    const auto advanced = ump::advancePlaybackClock(clock, 500'000);
    assert(advanced.mediaUs == 5'000'000);
    assert(advanced.wallUs == 500'000);

    advanced.speed = 16.0;
    const auto fast = ump::advancePlaybackClock(advanced, 250'000);
    assert(fast.mediaUs == 9'000'000);
    assert(fast.wallUs == 750'000);

    const auto ahead = ump::clockDeltaUs(fast, 9'500'000);
    const auto behind = ump::clockDeltaUs(fast, 8'500'000);
    assert(ahead == 500'000);
    assert(behind == -500'000);

    auto paused = fast;
    paused.paused = true;
    const auto unchanged = ump::advancePlaybackClock(paused, 1'000'000);
    assert(unchanged.mediaUs == paused.mediaUs);
    assert(unchanged.wallUs == paused.wallUs);

    std::cout << "KAVIRO playback clock tests: PASS\n";
    return 0;
}
