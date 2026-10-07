#include "ump/FfmpegMediaSession.h"
#include "ump/native/NativeMediaEngine.h"
#include "ump/native/NativeMediaEnginePackage.h"
#include <cassert>
#include <iostream>
namespace {
class Sink final : public ump::FfmpegFrameSink {
public: bool onFrame(const ump::FfmpegDecodedFrame& f) override {
  if (f.ownedData.size() > static_cast<std::size_t>(ump::FfmpegMediaSession::kDefaultMaxFrameBytes)) return false;
  for (const auto& p : f.planes) {
    if (p.linesize < 0 || p.width < 0 || p.height < 0) return false;
  }
  return true;
}};
}
int main() {
 using namespace ump;
 PlaybackClock c{100,0,2,false}; c=advancePlaybackClock(c,50);
 if(c.mediaUs!=200||clockDeltaUs(c,150)!=-50)return 1;
 FfmpegMediaSession s; if(s.standaloneReady())return 2;
 FfmpegDecodedFrame f; f.ownedData.resize(1024); f.planes.push_back({f.ownedData.data(),16,4,4});
 Sink sink; if(!sink.onFrame(f))return 3;
 native::NativeEnginePackageManifest m; m.engineName="KAVIRO Bundled Native FFmpeg Engine";m.version="1";m.buildId="b";m.license="FFmpeg";m.binaryChecksum="sha256:x";m.standalone=true;
 native::NativeEnginePackageExpectation e; e.engineName=m.engineName;e.version=m.version;e.buildId=m.buildId;e.license=m.license;e.binaryChecksum=m.binaryChecksum;
 std::string err; if(!native::NativeEnginePackageValidator::validate(m,e,err))return 4;
 std::cout<<"KAVIRO native boundary tests: PASS\n";
}