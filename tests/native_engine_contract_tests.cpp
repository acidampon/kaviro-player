#include "ump/native/NativeMediaEngine.h"
#include "ump/native/NativeMediaEnginePackage.h"

#include <cassert>
#include <iostream>

int main() {
    using namespace ump::native;

    NativeEnginePackageManifest m;
    m.engineName = "KAVIRO Bundled Native FFmpeg Engine";
    m.version = "1.0.0";
    m.buildId = "build-1";
    m.license = "FFmpeg";
    m.upstreamSource = "https://ffmpeg.org/";
    m.upstreamChecksum = "sha256:upstream";
    m.platform = "windows";
    m.architecture = "x64";
    m.binaryChecksum = "sha256:binary";
    m.standalone = true;

    NativeEnginePackageExpectation e{
        m.engineName, m.version, m.buildId, m.license, m.upstreamSource,
        m.upstreamChecksum, m.platform, m.architecture, m.binaryChecksum};

    std::string error;
    assert(NativeEnginePackageValidator::validate(m, e, error));

    e.binaryChecksum = "sha256:wrong";
    assert(!NativeEnginePackageValidator::validate(m, e, error));

    struct ClocklessAudio final : AudioOutput {
        bool write(const ump::FfmpegDecodedFrame&) override { return true; }
    };
    ClocklessAudio audio;
    assert(audio.clockPositionUs() == -1);

    NativeMediaEngine engine;
    assert(!engine.isOpen());
    assert(!engine.standaloneReady());
    assert(engine.state() == NativeEngineState::Closed);
    assert(engine.setSpeed(1.25));
    assert(engine.speed() == 1.25);
    assert(!engine.setSpeed(0.0));
    assert(engine.state() == NativeEngineState::Closed);
    assert(!engine.play());
    assert(engine.state() == NativeEngineState::Error);

    engine.close();
    assert(engine.state() == NativeEngineState::Closed);

    std::cout << "KAVIRO native engine contract tests: PASS\n";
}
