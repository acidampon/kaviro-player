#include "ump/FfmpegMediaSession.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>

#if defined(KAVIRO_FFMPEG_NATIVE)
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}
#endif

namespace ump {

#if defined(KAVIRO_FFMPEG_NATIVE)
struct FfmpegMediaSession::Impl {
    AVFormatContext* format{};
    std::vector<AVCodecContext*> decoders;
    std::vector<int> streamToDecoder;
    bool demuxEof{false};
    std::int64_t maxFrameBytes{FfmpegMediaSession::kDefaultMaxFrameBytes};

    void reset() noexcept {
        for (auto* decoder : decoders) {
            if (decoder) avcodec_free_context(&decoder);
        }
        decoders.clear();
        streamToDecoder.clear();
        if (format) avformat_close_input(&format);
        demuxEof = false;
    }
};
#else
struct FfmpegMediaSession::Impl {};
#endif

FfmpegMediaSession::FfmpegMediaSession() : impl_(new Impl) {}
FfmpegMediaSession::~FfmpegMediaSession() { close(); delete impl_; }

#if defined(KAVIRO_FFMPEG_NATIVE)

namespace {

std::string ffError(int code, const char* operation) {
    char buffer[AV_ERROR_MAX_STRING_SIZE]{};
    av_strerror(code, buffer, sizeof(buffer));
    return std::string(operation) + ": " + buffer;
}

bool selectedStream(const std::vector<FfmpegStreamInfo>& streams, int index) {
    for (const auto& stream : streams) {
        if (stream.index == index)
            return stream.selected;
    }
    return false;
}

bool appendPlane(FfmpegDecodedFrame& output,
                 const std::uint8_t* source,
                 int sourceLineSize,
                 int rows,
                 std::int64_t maxBytes) {
    if (!source || sourceLineSize == 0 || rows <= 0) return true;
    const std::size_t rowBytes = static_cast<std::size_t>(std::abs(sourceLineSize));
    const std::size_t rowsCount = static_cast<std::size_t>(rows);
    if (rowBytes > static_cast<std::size_t>(maxBytes) ||
        rowsCount > static_cast<std::size_t>(maxBytes) / rowBytes) {
        return false;
    }
    const std::size_t oldSize = output.ownedData.size();
    const std::size_t bytes = rowBytes * rowsCount;
    if (bytes > static_cast<std::size_t>(maxBytes) - oldSize) return false;
    output.ownedData.resize(oldSize + bytes);
    auto* destination = output.ownedData.data() + oldSize;
    for (int row = 0; row < rows; ++row) {
        const auto* src = source + static_cast<std::ptrdiff_t>(row) * sourceLineSize;
        std::memcpy(destination + static_cast<std::size_t>(row) * rowBytes, src, rowBytes);
    }
    output.planes.push_back({destination, static_cast<int>(rowBytes), 0, rows});
    return true;
}

bool copyDecodedFrame(const AVFrame* frame,
                      FfmpegStreamType type,
                      std::int64_t ptsUs,
                      std::int64_t maxBytes,
                      FfmpegDecodedFrame& output) {
    output = {};
    output.type = type;
    output.ptsUs = ptsUs;

    if (!frame || maxBytes <= 0) return false;

    if (type == FfmpegStreamType::Audio) {
        const int channels = frame->ch_layout.nb_channels;
        if (frame->sample_rate <= 0 || channels <= 0 || frame->nb_samples <= 0)
            return false;

        AVChannelLayout outputLayout{};
        if (av_channel_layout_copy(&outputLayout, &frame->ch_layout) < 0)
            return false;

        SwrContext* swr = nullptr;
        const int initRc = swr_alloc_set_opts2(
            &swr,
            &outputLayout,
            AV_SAMPLE_FMT_S16,
            frame->sample_rate,
            &frame->ch_layout,
            static_cast<AVSampleFormat>(frame->format),
            frame->sample_rate,
            0,
            nullptr);
        av_channel_layout_uninit(&outputLayout);
        if (initRc < 0 || !swr || swr_init(swr) < 0) {
            swr_free(&swr);
            return false;
        }

        const int outputSamples = static_cast<int>(av_rescale_rnd(
            swr_get_delay(swr, frame->sample_rate) + frame->nb_samples,
            frame->sample_rate,
            frame->sample_rate,
            AV_ROUND_UP));
        if (outputSamples <= 0) {
            swr_free(&swr);
            return false;
        }

        const std::size_t bytesPerSample = sizeof(std::int16_t);
        const auto maxBytesSize = static_cast<std::size_t>(maxBytes);
        const auto sampleCount = static_cast<std::size_t>(outputSamples);
        const auto channelCount = static_cast<std::size_t>(channels);
        if (channelCount != 0 &&
            sampleCount > maxBytesSize / channelCount) {
            swr_free(&swr);
            return false;
        }
        const auto samplesTimesChannels = sampleCount * channelCount;
        if (bytesPerSample != 0 &&
            samplesTimesChannels > maxBytesSize / bytesPerSample) {
            swr_free(&swr);
            return false;
        }
        const std::size_t bytes = samplesTimesChannels * bytesPerSample;
        if (bytes > maxBytesSize) {
            swr_free(&swr);
            return false;
        }

        output.ownedData.resize(bytes);
        std::uint8_t* outPlanes[] = { output.ownedData.data() };
        const int converted = swr_convert(
            swr,
            outPlanes,
            outputSamples,
            const_cast<const std::uint8_t**>(frame->extended_data),
            frame->nb_samples);
        swr_free(&swr);

        if (converted <= 0) {
            output.ownedData.clear();
            return false;
        }

        output.sampleRate = frame->sample_rate;
        output.channels = channels;
        output.samples = converted;
        output.format = AV_SAMPLE_FMT_S16;
        output.normalized = true;
        output.ownedData.resize(
            static_cast<std::size_t>(converted) *
            static_cast<std::size_t>(channels) * bytesPerSample);
        output.planes.push_back({
            output.ownedData.data(),
            channels * static_cast<int>(bytesPerSample),
            converted,
            1
        });
        return true;
    }

    if (type != FfmpegStreamType::Video ||
        frame->width <= 0 || frame->height <= 0 ||
        frame->format < 0) {
        return false;
    }

    const auto width = static_cast<std::size_t>(frame->width);
    const auto height = static_cast<std::size_t>(frame->height);
    const auto maxBytesSize = static_cast<std::size_t>(maxBytes);
    if (height != 0 && width > maxBytesSize / height) return false;
    const auto pixelCount = width * height;
    if (4U != 0 && pixelCount > maxBytesSize / 4U) return false;
    const std::size_t rgbaBytes = pixelCount * 4U;
    if (rgbaBytes > maxBytesSize) return false;
    if (width > static_cast<std::size_t>(std::numeric_limits<int>::max()) / 4U) {
        return false;
    }

    SwsContext* sws = sws_getContext(
        frame->width,
        frame->height,
        static_cast<AVPixelFormat>(frame->format),
        frame->width,
        frame->height,
        AV_PIX_FMT_RGBA,
        SWS_BILINEAR,
        nullptr,
        nullptr,
        nullptr);
    if (!sws) return false;

    output.ownedData.resize(rgbaBytes);
    std::uint8_t* destination[] = { output.ownedData.data() };
    const int destinationLinesize[] = { frame->width * 4 };
    const int scaled = sws_scale(
        sws,
        frame->data,
        frame->linesize,
        0,
        frame->height,
        destination,
        destinationLinesize);
    sws_freeContext(sws);

    if (scaled != frame->height) {
        output.ownedData.clear();
        return false;
    }

    output.width = frame->width;
    output.height = frame->height;
    output.format = AV_PIX_FMT_RGBA;
    output.normalized = true;
    output.planes.push_back({
        output.ownedData.data(),
        frame->width * 4,
        frame->width,
        frame->height
    });
    return true;
}

} // namespace

bool FfmpegMediaSession::open(const std::filesystem::path& path,
                              const FfmpegOpenOptions& options) {
    close();
    if (path.empty()) {
        error_ = "empty media path";
        return false;
    }

    AVFormatContext* format = nullptr;
    int rc = avformat_open_input(&format, path.string().c_str(), nullptr, nullptr);
    if (rc < 0) {
        error_ = ffError(rc, "avformat_open_input");
        return false;
    }
    rc = avformat_find_stream_info(format, nullptr);
    if (rc < 0) {
        avformat_close_input(&format);
        error_ = ffError(rc, "avformat_find_stream_info");
        return false;
    }

    impl_->format = format;
    impl_->maxFrameBytes =
        options.maxFrameBytes > 0 ? options.maxFrameBytes : kDefaultMaxFrameBytes;
    impl_->streamToDecoder.assign(format->nb_streams, -1);
    streams_.clear();

    bool hasDecoder = false;
    bool decoderFailure = false;

    for (unsigned i = 0; i < format->nb_streams; ++i) {
        AVStream* stream = format->streams[i];
        AVCodecParameters* parameters = stream->codecpar;
        FfmpegStreamInfo info;
        info.index = static_cast<int>(i);

        if (parameters->codec_type == AVMEDIA_TYPE_VIDEO)
            info.type = FfmpegStreamType::Video;
        else if (parameters->codec_type == AVMEDIA_TYPE_AUDIO)
            info.type = FfmpegStreamType::Audio;
        else if (parameters->codec_type == AVMEDIA_TYPE_SUBTITLE)
            info.type = FfmpegStreamType::Subtitle;
        else
            continue;

        const AVCodec* codec = avcodec_find_decoder(parameters->codec_id);
        if (codec) info.codec = codec->name;
        if (stream->metadata) {
            AVDictionaryEntry* language =
                av_dict_get(stream->metadata, "language", nullptr, 0);
            AVDictionaryEntry* title =
                av_dict_get(stream->metadata, "title", nullptr, 0);
            if (language) info.language = language->value;
            if (title) info.title = title->value;
        }
        if (stream->duration != AV_NOPTS_VALUE && stream->time_base.den) {
            info.durationMs = av_rescale_q(
                stream->duration, stream->time_base, AVRational{1000, 1});
        }

        if (codec && (parameters->codec_type == AVMEDIA_TYPE_AUDIO ||
                      parameters->codec_type == AVMEDIA_TYPE_VIDEO)) {
            AVCodecContext* decoder = avcodec_alloc_context3(codec);
            if (!decoder || avcodec_parameters_to_context(decoder, parameters) < 0 ||
                avcodec_open2(decoder, codec, nullptr) < 0) {
                if (decoder) avcodec_free_context(&decoder);
                decoderFailure = true;
            } else {
                const int decoderIndex = static_cast<int>(impl_->decoders.size());
                impl_->decoders.push_back(decoder);
                impl_->streamToDecoder[i] = decoderIndex;
                info.selected = true;
                hasDecoder = true;
            }
        }
        streams_.push_back(std::move(info));
    }

    if (!hasDecoder) {
        impl_->reset();
        streams_.clear();
        error_ = "no supported audio/video decoder could be opened";
        recovery_ = FfmpegRecoveryOutcome::Unrecoverable;
        return false;
    }

    open_ = true;
    recovery_ = decoderFailure
        ? FfmpegRecoveryOutcome::PartiallyRecovered
        : FfmpegRecoveryOutcome::Recovered;
    error_.clear();

    // Prefer the first decoded audio and video stream as the initial selection.
    bool audioChosen = false;
    bool videoChosen = false;
    for (auto& stream : streams_) {
        if (stream.type == FfmpegStreamType::Audio && !audioChosen && stream.selected)
            audioChosen = true;
        else if (stream.type == FfmpegStreamType::Video && !videoChosen && stream.selected)
            videoChosen = true;
        else if (stream.type == FfmpegStreamType::Audio && stream.selected)
            stream.selected = false;
        else if (stream.type == FfmpegStreamType::Video && stream.selected)
            stream.selected = false;
    }
    (void)options.hardwareDecodePreferred; // Hardware paths are a later platform-specific gate.
    return true;
}

#else

bool FfmpegMediaSession::open(const std::filesystem::path&,
                              const FfmpegOpenOptions&) {
    close();
    error_ = "native FFmpeg development libraries are not installed in this build";
    return false;
}

#endif

void FfmpegMediaSession::close() {
#if defined(KAVIRO_FFMPEG_NATIVE)
    if (impl_) impl_->reset();
#endif
    open_ = false;
    streams_.clear();
    error_.clear();
    recovery_ = FfmpegRecoveryOutcome::NotAttempted;
}

bool FfmpegMediaSession::isOpen() const { return open_; }
bool FfmpegMediaSession::ended() const noexcept {
#if defined(KAVIRO_FFMPEG_NATIVE)
    return open_ && impl_ && impl_->demuxEof;
#else
    return false;
#endif
}
std::string FfmpegMediaSession::lastError() const { return error_; }
const std::vector<FfmpegStreamInfo>& FfmpegMediaSession::streams() const { return streams_; }

bool FfmpegMediaSession::selectAudioTrack(int index) {
    if (!open_) { error_ = "session not open"; return false; }

    const auto candidate = std::find_if(
        streams_.begin(), streams_.end(),
        [index](const FfmpegStreamInfo& stream) {
            return stream.type == FfmpegStreamType::Audio &&
                   stream.index == index;
        });
    if (candidate == streams_.end()) {
        error_ = "audio track not found";
        return false;
    }
#if defined(KAVIRO_FFMPEG_NATIVE)
    if (index < 0 ||
        index >= static_cast<int>(impl_->streamToDecoder.size()) ||
        impl_->streamToDecoder[index] < 0) {
        error_ = "audio track decoder unavailable";
        return false;
    }
#endif

    for (auto& stream : streams_) {
        if (stream.type == FfmpegStreamType::Audio)
            stream.selected = stream.index == index;
    }

#if defined(KAVIRO_FFMPEG_NATIVE)
    if (impl_) {
        for (std::size_t i = 0; i < impl_->streamToDecoder.size(); ++i) {
            const int decoderIndex = impl_->streamToDecoder[i];
            if (decoderIndex < 0 || decoderIndex >= static_cast<int>(impl_->decoders.size()))
                continue;
            const auto type = std::find_if(
                streams_.begin(), streams_.end(),
                [i](const FfmpegStreamInfo& stream) {
                    return stream.index == static_cast<int>(i);
                });
            if (type != streams_.end() && type->type == FfmpegStreamType::Audio)
                avcodec_flush_buffers(impl_->decoders[decoderIndex]);
        }
    }
#endif

    error_.clear();
    return true;
}

bool FfmpegMediaSession::selectVideoTrack(int index) {
    if (!open_) { error_ = "session not open"; return false; }

    const auto candidate = std::find_if(
        streams_.begin(), streams_.end(),
        [index](const FfmpegStreamInfo& stream) {
            return stream.type == FfmpegStreamType::Video &&
                   stream.index == index;
        });
    if (candidate == streams_.end()) {
        error_ = "video track not found";
        return false;
    }
#if defined(KAVIRO_FFMPEG_NATIVE)
    if (index < 0 ||
        index >= static_cast<int>(impl_->streamToDecoder.size()) ||
        impl_->streamToDecoder[index] < 0) {
        error_ = "video track decoder unavailable";
        return false;
    }
#endif

    for (auto& stream : streams_) {
        if (stream.type == FfmpegStreamType::Video)
            stream.selected = stream.index == index;
    }

#if defined(KAVIRO_FFMPEG_NATIVE)
    if (impl_) {
        for (std::size_t i = 0; i < impl_->streamToDecoder.size(); ++i) {
            const int decoderIndex = impl_->streamToDecoder[i];
            if (decoderIndex < 0 || decoderIndex >= static_cast<int>(impl_->decoders.size()))
                continue;
            const auto type = std::find_if(
                streams_.begin(), streams_.end(),
                [i](const FfmpegStreamInfo& stream) {
                    return stream.index == static_cast<int>(i);
                });
            if (type != streams_.end() && type->type == FfmpegStreamType::Video)
                avcodec_flush_buffers(impl_->decoders[decoderIndex]);
        }
    }
#endif

    error_.clear();
    return true;
}

#if defined(KAVIRO_FFMPEG_NATIVE)

bool FfmpegMediaSession::seekMs(std::int64_t positionMs) {
    if (!open_ || !impl_->format || positionMs < 0) {
        error_ = "invalid seek";
        return false;
    }
    const std::int64_t timestamp = av_rescale_q(
        positionMs, AVRational{1, 1000}, AV_TIME_BASE_Q);
    const int rc = avformat_seek_file(impl_->format, -1, INT64_MIN, timestamp,
                                      INT64_MAX, AVSEEK_FLAG_BACKWARD);
    if (rc < 0) {
        error_ = ffError(rc, "avformat_seek_file");
        return false;
    }
    for (auto* decoder : impl_->decoders) {
        if (decoder) avcodec_flush_buffers(decoder);
    }
    impl_->demuxEof = false;
    error_.clear();
    return true;
}

bool FfmpegMediaSession::decodeToSink(FfmpegFrameSink& sink, std::size_t maxFrames) {
    if (!open_ || !impl_->format) {
        error_ = "session not open";
        return false;
    }

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    if (!packet || !frame) {
        av_packet_free(&packet);
        av_frame_free(&frame);
        error_ = "failed to allocate FFmpeg packet/frame";
        return false;
    }

    std::size_t emitted = 0;
    bool decoderError = false;

    auto receive = [&](int decoderIndex, int streamIndex) {
        AVCodecContext* decoder = impl_->decoders[decoderIndex];
        AVStream* stream = impl_->format->streams[streamIndex];
        while (maxFrames == 0 || emitted < maxFrames) {
            const int rc = avcodec_receive_frame(decoder, frame);
            if (rc == AVERROR(EAGAIN) || rc == AVERROR_EOF) return;
            if (rc < 0) {
                decoderError = true;
                error_ = ffError(rc, "avcodec_receive_frame");
                return;
            }

            FfmpegStreamType type = FfmpegStreamType::Video;
            for (const auto& info : streams_) {
                if (info.index == streamIndex) { type = info.type; break; }
            }

            const std::int64_t ptsUs = frame->best_effort_timestamp == AV_NOPTS_VALUE
                ? 0
                : av_rescale_q(frame->best_effort_timestamp, stream->time_base,
                               AVRational{1, 1000000});
            FfmpegDecodedFrame decoded;
            if (!copyDecodedFrame(frame, type, ptsUs, impl_->maxFrameBytes, decoded)) {
                decoderError = true;
                error_ = "decoded frame exceeds configured memory boundary or has unsupported layout";
                return;
            }
            if (!sink.onFrame(decoded)) {
                av_frame_unref(frame);
                return;
            }
            ++emitted;
            av_frame_unref(frame);
        }
    };

    while (maxFrames == 0 || emitted < maxFrames) {
        if (impl_->demuxEof) break;
        const int readRc = av_read_frame(impl_->format, packet);
        if (readRc == AVERROR_EOF) {
            impl_->demuxEof = true;
            for (std::size_t i = 0; i < impl_->decoders.size(); ++i) {
                if (!impl_->decoders[i]) continue;
                const int streamIndex = static_cast<int>(
                    std::find(impl_->streamToDecoder.begin(),
                              impl_->streamToDecoder.end(),
                              static_cast<int>(i)) - impl_->streamToDecoder.begin());
                if (streamIndex < 0 ||
                    streamIndex >= static_cast<int>(impl_->streamToDecoder.size()) ||
                    !selectedStream(streams_, streamIndex))
                    continue;
                const int rc = avcodec_send_packet(impl_->decoders[i], nullptr);
                if (rc < 0 && rc != AVERROR_EOF) {
                    decoderError = true;
                    error_ = ffError(rc, "avcodec_send_packet(flush)");
                    continue;
                }
                receive(static_cast<int>(i), streamIndex);
            }
            break;
        }
        if (readRc < 0) {
            error_ = ffError(readRc, "av_read_frame");
            av_packet_free(&packet);
            av_frame_free(&frame);
            return emitted > 0;
        }

        const int streamIndex = packet->stream_index;
        if (streamIndex < 0 ||
            streamIndex >= static_cast<int>(impl_->streamToDecoder.size()) ||
            !selectedStream(streams_, streamIndex)) {
            av_packet_unref(packet);
            continue;
        }
        const int decoderIndex = impl_->streamToDecoder[streamIndex];
        if (decoderIndex < 0) {
            av_packet_unref(packet);
            continue;
        }

        AVCodecContext* decoder = impl_->decoders[decoderIndex];
        const int sendRc = avcodec_send_packet(decoder, packet);
        av_packet_unref(packet);
        if (sendRc < 0 && sendRc != AVERROR(EAGAIN)) {
            decoderError = true;
            error_ = ffError(sendRc, "avcodec_send_packet");
            continue;
        }
        receive(decoderIndex, streamIndex);
    }

    av_packet_free(&packet);
    av_frame_free(&frame);

    if (decoderError && emitted == 0) {
        recovery_ = FfmpegRecoveryOutcome::PartiallyRecovered;
        return false;
    }
    if (emitted > 0) error_.clear();
    return emitted > 0;
}

#else

bool FfmpegMediaSession::seekMs(std::int64_t) {
    if (!open_) error_ = "session not open";
    else error_ = "native FFmpeg seek unavailable in this build";
    return false;
}

bool FfmpegMediaSession::decodeToSink(FfmpegFrameSink&, std::size_t) {
    if (!open_) error_ = "session not open";
    else error_ = "native FFmpeg decode unavailable in this build";
    return false;
}

#endif

FfmpegRecoveryOutcome FfmpegMediaSession::recoveryOutcome() const { return recovery_; }
bool FfmpegMediaSession::standaloneReady() const {
#if defined(KAVIRO_FFMPEG_NATIVE)
    return open_ && impl_ && impl_->format != nullptr;
#else
    return false;
#endif
}

PlaybackClock advancePlaybackClock(PlaybackClock c, std::int64_t elapsedUs) {
    if (elapsedUs <= 0 || c.paused) return c;
    if (!std::isfinite(c.speed)) c.speed = 1.0;
    c.speed = std::clamp(c.speed, 0.25, 16.0);
    const long double next =
        static_cast<long double>(c.mediaUs) +
        static_cast<long double>(elapsedUs) * c.speed;
    const auto hi = static_cast<long double>(std::numeric_limits<std::int64_t>::max());
    const auto lo = static_cast<long double>(std::numeric_limits<std::int64_t>::min());
    c.mediaUs = next >= hi ? std::numeric_limits<std::int64_t>::max()
                           : next <= lo ? std::numeric_limits<std::int64_t>::min()
                                        : static_cast<std::int64_t>(next);
    c.wallUs = c.wallUs > std::numeric_limits<std::int64_t>::max() - elapsedUs
        ? std::numeric_limits<std::int64_t>::max()
        : c.wallUs + elapsedUs;
    return c;
}

std::int64_t clockDeltaUs(const PlaybackClock& c, std::int64_t positionUs) {
    if (positionUs >= c.mediaUs) {
        const auto delta = positionUs - c.mediaUs;
        return delta < 0 ? std::numeric_limits<std::int64_t>::max() : delta;
    }
    const auto delta = c.mediaUs - positionUs;
    return delta < 0 ? std::numeric_limits<std::int64_t>::min() : -delta;
}

} // namespace ump
