#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace ump {
enum class FfmpegStreamType { Audio, Video, Subtitle };
enum class FfmpegRecoveryOutcome { NotAttempted, Recovered, PartiallyRecovered, Unrecoverable };
struct FfmpegStreamInfo { int index{-1}; FfmpegStreamType type{FfmpegStreamType::Audio}; std::string codec,language,title; std::int64_t durationMs{0}; bool selected{false}; };
struct FfmpegFramePlane { const std::uint8_t* data{}; int linesize{}; int width{}; int height{}; };
struct FfmpegDecodedFrame { FfmpegStreamType type{FfmpegStreamType::Video}; std::int64_t ptsUs{}; int width{},height{},sampleRate{},channels{},samples{},format{-1}; std::vector<std::uint8_t> ownedData; std::vector<FfmpegFramePlane> planes; };
class FfmpegFrameSink { public: virtual ~FfmpegFrameSink()=default; virtual bool onFrame(const FfmpegDecodedFrame&)=0; };
struct FfmpegOpenOptions { bool hardwareDecodePreferred{true}; bool recoveryMode{false}; std::int64_t maxFrameBytes{256LL*1024*1024}; };
class FfmpegMediaSession {
public:
 FfmpegMediaSession(); ~FfmpegMediaSession(); FfmpegMediaSession(const FfmpegMediaSession&)=delete; FfmpegMediaSession& operator=(const FfmpegMediaSession&)=delete;
 bool open(const std::filesystem::path& path, const FfmpegOpenOptions& options = {}); void close(); bool isOpen()const; std::string lastError()const;
 const std::vector<FfmpegStreamInfo>& streams()const; bool selectAudioTrack(int); bool selectVideoTrack(int); bool seekMs(std::int64_t);
 bool decodeToSink(FfmpegFrameSink&,std::size_t maxFrames=0); FfmpegRecoveryOutcome recoveryOutcome()const; bool standaloneReady()const;
 static constexpr std::int64_t kDefaultMaxFrameBytes=256LL*1024*1024;
private: struct Impl; Impl* impl_{}; std::vector<FfmpegStreamInfo> streams_; std::string error_; FfmpegRecoveryOutcome recovery_{FfmpegRecoveryOutcome::NotAttempted}; bool open_{false};
};
struct PlaybackClock { std::int64_t mediaUs{},wallUs{}; double speed{1.0}; bool paused{true}; };
PlaybackClock advancePlaybackClock(PlaybackClock,std::int64_t); std::int64_t clockDeltaUs(const PlaybackClock&,std::int64_t);
}