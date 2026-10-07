#include "ump/FfmpegMediaSession.h"
#include <algorithm>
#include <cmath>
#include <limits>
#if defined(KAVIRO_FFMPEG_NATIVE)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}
#endif
namespace ump {
struct FfmpegMediaSession::Impl {};
FfmpegMediaSession::FfmpegMediaSession():impl_(new Impl){} FfmpegMediaSession::~FfmpegMediaSession(){close();delete impl_;}
#if defined(KAVIRO_FFMPEG_NATIVE)
bool FfmpegMediaSession::open(const std::filesystem::path&p,const FfmpegOpenOptions&o){
 close(); if(p.empty()){error_="empty media path";return false;}
 AVFormatContext*f=nullptr;
 if(avformat_open_input(&f,p.string().c_str(),nullptr,nullptr)<0){error_="avformat_open_input failed";return false;}
 if(avformat_find_stream_info(f,nullptr)<0){avformat_close_input(&f);error_="avformat_find_stream_info failed";return false;}
 streams_.clear();
 for(unsigned i=0;i<f->nb_streams;++i){
  auto*s=f->streams[i]; auto*c=s->codecpar; FfmpegStreamInfo x; x.index=(int)i;
  if(c->codec_type==AVMEDIA_TYPE_VIDEO)x.type=FfmpegStreamType::Video;
  else if(c->codec_type==AVMEDIA_TYPE_AUDIO)x.type=FfmpegStreamType::Audio;
  else if(c->codec_type==AVMEDIA_TYPE_SUBTITLE)x.type=FfmpegStreamType::Subtitle;
  else continue;
  if(auto*d=avcodec_find_decoder(c->codec_id))x.codec=d->name;
  if(s->duration!=AV_NOPTS_VALUE&&s->time_base.den)x.durationMs=std::max<std::int64_t>(0,s->duration*1000LL*s->time_base.num/s->time_base.den);
  streams_.push_back(std::move(x));
 }
 avformat_close_input(&f);
 if(streams_.empty()){error_="no supported media streams";return false;}
 open_=true; recovery_=o.recoveryMode?FfmpegRecoveryOutcome::NotAttempted:FfmpegRecoveryOutcome::NotAttempted; return true;
}
#else
bool FfmpegMediaSession::open(const std::filesystem::path&,const FfmpegOpenOptions&){close();error_="native FFmpeg development libraries are not installed in this build";return false;}
#endif
void FfmpegMediaSession::close(){open_=false;streams_.clear();error_.clear();recovery_=FfmpegRecoveryOutcome::NotAttempted;}
bool FfmpegMediaSession::isOpen()const{return open_;}
std::string FfmpegMediaSession::lastError()const{return error_;}
const std::vector<FfmpegStreamInfo>&FfmpegMediaSession::streams()const{return streams_;}
bool FfmpegMediaSession::selectAudioTrack(int i){if(!open_)return false;bool found=false;for(auto&s:streams_)if(s.type==FfmpegStreamType::Audio){s.selected=(s.index==i);found|=s.selected;}if(!found)error_="audio track not found";return found;}
bool FfmpegMediaSession::selectVideoTrack(int i){if(!open_)return false;bool found=false;for(auto&s:streams_)if(s.type==FfmpegStreamType::Video){s.selected=(s.index==i);found|=s.selected;}if(!found)error_="video track not found";return found;}
bool FfmpegMediaSession::seekMs(std::int64_t p){if(!open_||p<0){error_="invalid seek";return false;}error_="native seek decoder implementation pending";return false;}
bool FfmpegMediaSession::decodeToSink(FfmpegFrameSink&,std::size_t){if(!open_){error_="session not open";return false;}error_="native decode pipeline pending";return false;}
FfmpegRecoveryOutcome FfmpegMediaSession::recoveryOutcome()const{return recovery_;}
bool FfmpegMediaSession::standaloneReady()const{return false;}
PlaybackClock advancePlaybackClock(PlaybackClock c,std::int64_t e){if(e<=0||c.paused)return c;if(!std::isfinite(c.speed))c.speed=1;c.speed=std::clamp(c.speed,.25,4.0);long double n=(long double)c.mediaUs+(long double)e*c.speed;auto hi=(long double)std::numeric_limits<std::int64_t>::max();auto lo=(long double)std::numeric_limits<std::int64_t>::min();c.mediaUs=n>=hi?std::numeric_limits<std::int64_t>::max():n<=lo?std::numeric_limits<std::int64_t>::min():(std::int64_t)n;c.wallUs=c.wallUs>std::numeric_limits<std::int64_t>::max()-e?std::numeric_limits<std::int64_t>::max():c.wallUs+e;return c;}
std::int64_t clockDeltaUs(const PlaybackClock&c,std::int64_t p){if(p>=c.mediaUs){auto d=p-c.mediaUs;return d<0?std::numeric_limits<std::int64_t>::max():d;}auto d=c.mediaUs-p;return d<0?std::numeric_limits<std::int64_t>::min():-d;}
}