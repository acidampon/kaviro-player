#pragma once
#include <sqlite3.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>
namespace ump {
enum class MediaType{Unknown,Audio,Video}; struct MediaItem{std::string id;std::filesystem::path path;std::string title,mimeType;MediaType type{MediaType::Unknown};std::int64_t durationMs{0};};
std::string mediaTypeName(MediaType); std::string titleFromPath(const std::filesystem::path&); std::string stableMediaId(const std::filesystem::path&);
struct MediaProbeResult{bool success{};MediaType type{MediaType::Unknown};std::string format,codec,error;std::int64_t durationMs{},bitrate{};int width{},height{};};
class MediaProbe{public:static MediaProbeResult inspect(const std::string&);};
struct ScanDiagnostics{std::size_t filesVisited{},candidates{},detectedMedia{},traversalErrors{},inspectionFailures{};std::vector<std::string> errors;};
class MediaLibrary{public:using Progress=std::function<void(const std::filesystem::path&,std::size_t)>;std::vector<MediaItem> scan(const std::filesystem::path&,const Progress& = Progress{});std::vector<MediaItem> scanDetailed(const std::filesystem::path&,ScanDiagnostics&,const Progress& = Progress{});void cancel();bool cancelled()const;const std::vector<MediaItem>&items()const;private:std::vector<MediaItem>items_;std::atomic_bool cancelled_{};};
enum class EngineCapability{Pause,Seek,Speed,Volume,Subtitles,AudioTracks,HardwareAcceleration,Recovery};enum class EngineState{Idle,Opening,Playing,Paused,Stopped,Error};enum class EngineBackend{BundledNative,ExternalDevelopment,Unavailable};
struct MediaTrack{std::string id,language,title,codec;bool selected{};};struct MediaEngineInfo{EngineBackend backend{EngineBackend::Unavailable};std::string name,version,buildId,license;bool hardwareDecode{},available{},standalone{};std::vector<std::string>notes;};
class PlaybackEngine{public:virtual~PlaybackEngine()=default;virtual bool play(const std::string&)=0;virtual bool pause()=0;virtual bool seek(std::int64_t)=0;virtual bool setSpeed(double)=0;virtual bool setVolume(int)=0;virtual bool stop()=0;virtual bool isPlaying()const=0;virtual EngineState state()const=0;virtual bool supports(EngineCapability)const=0;virtual MediaEngineInfo info()const=0;};
class ExternalPlayerAdapter final:public PlaybackEngine{EngineState state_{EngineState::Idle};public:bool play(const std::string&)override;bool pause()override;bool seek(std::int64_t)override;bool setSpeed(double)override;bool setVolume(int)override;bool stop()override;bool isPlaying()const override;EngineState state()const override;bool supports(EngineCapability)const override;MediaEngineInfo info()const override;};
class MediaEngineFactory{public:static std::unique_ptr<PlaybackEngine>createDevelopmentEngine();static std::unique_ptr<PlaybackEngine>createBundledEngine();static std::unique_ptr<PlaybackEngine>createBestAvailableEngine();};
struct PlaybackState{std::string mediaId;std::int64_t positionMs{};double speed{1.0};int volume{100};bool paused{true};};PlaybackState sanitizePlaybackState(PlaybackState,std::int64_t=0);
class PlaybackController{std::string path_;std::int64_t durationMs_{},positionMs_{};double speed_{1};int volume_{100};bool playing_{};public:bool load(const std::string&,std::int64_t);bool play();bool pause();bool stop();bool seek(std::int64_t);bool setSpeed(double);bool setVolume(int);const std::string&path()const;std::int64_t positionMs()const;double speed()const;int volume()const;bool playing()const;};
class PlaybackStore{sqlite3*db_{};std::string path_,error_;public:explicit PlaybackStore(const std::string&);~PlaybackStore();bool open();bool save(const PlaybackState&);PlaybackState load(const std::string&)const;std::string lastError()const;};
class MediaQueue{std::vector<MediaItem>items_;std::size_t current_{};bool hasCurrent_{};public:void setItems(std::vector<MediaItem>);void append(const MediaItem&);void clear();const MediaItem*current()const;const MediaItem*next();const MediaItem*previous();bool moveTo(std::size_t);bool contains(const std::string&)const;std::size_t size()const;};
struct HistoryEntry{std::string id,title;std::filesystem::path path;std::int64_t positionMs{};int playCount{};};class HistoryStore{sqlite3*db_{};std::string path_,error_;public:explicit HistoryStore(const std::string&);~HistoryStore();bool open();bool record(const MediaItem&,std::int64_t);std::vector<HistoryEntry>recent(std::size_t=50)const;std::string lastError()const;};
struct Playlist{std::string id,name;std::vector<std::string>mediaIds;};class PlaylistStore{sqlite3*db_{};std::string path_,error_;public:explicit PlaylistStore(const std::string&);~PlaylistStore();bool open();bool save(const Playlist&);bool addItem(const std::string&,const std::string&);std::vector<Playlist>list()const;};
class SettingsStore{sqlite3*db_{};std::string path_,error_;public:explicit SettingsStore(const std::string&);~SettingsStore();bool open();bool set(const std::string&,const std::string&);std::string get(const std::string&,const std::string& = "")const;};
class PlayerSession{std::unique_ptr<PlaybackEngine>engine_;PlaybackStore&playback_;HistoryStore&history_;PlaybackController controller_;MediaQueue queue_;MediaItem current_;bool hasCurrent_{};bool saveCurrent();public:PlayerSession(std::unique_ptr<PlaybackEngine>,PlaybackStore&,HistoryStore&);bool open(const MediaItem&);bool play();bool pause();bool stop();bool seek(std::int64_t);bool setSpeed(double);bool setVolume(int);const MediaItem*current()const;const MediaItem*next();const MediaItem*previous();bool saveState();bool recordHistory();MediaQueue&queue();};
}