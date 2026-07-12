#include "player.h"
#include <cstring>
#include <limits>

// ── FFmpeg memory-IO callbacks ────────────────────────────────────────────

static int mem_read(void* opaque, uint8_t* buf, int buf_size) {
    auto* mb = static_cast<VideoPlayer::MemBuf*>(opaque);
    std::size_t avail = mb->size - mb->pos;
    if (avail == 0) return AVERROR_EOF;
    int n = static_cast<int>(avail < static_cast<std::size_t>(buf_size) ? avail : buf_size);
    std::memcpy(buf, mb->data + mb->pos, n);
    mb->pos += n;
    return n;
}

static int64_t mem_seek(void* opaque, int64_t offset, int whence) {
    auto* mb = static_cast<VideoPlayer::MemBuf*>(opaque);
    if (whence == AVSEEK_SIZE)  return static_cast<int64_t>(mb->size);
    if (whence == SEEK_SET)     mb->pos = static_cast<std::size_t>(offset);
    else if (whence == SEEK_CUR) mb->pos = static_cast<std::size_t>(static_cast<int64_t>(mb->pos) + offset);
    else if (whence == SEEK_END) mb->pos = static_cast<std::size_t>(static_cast<int64_t>(mb->size) + offset);
    else return -1;
    return static_cast<int64_t>(mb->pos);
}

// ── VideoPlayer ───────────────────────────────────────────────────────────

VideoPlayer::~VideoPlayer() {
    av_frame_free(&frame_);
    av_frame_free(&frame_rgb_);
    av_frame_free(&audio_frame_);
    av_packet_free(&packet_);
    if (sws_ctx_) sws_freeContext(sws_ctx_);
    if (swr_ctx_) swr_free(&swr_ctx_);
    if (audio_dev_ != 0) SDL_CloseAudioDevice(audio_dev_);
    avcodec_free_context(&codec_ctx_);
    avcodec_free_context(&audio_codec_ctx_);
    if (fmt_ctx_) avformat_close_input(&fmt_ctx_);
    if (avio_ctx_) {
        av_freep(&avio_ctx_->buffer);
        avio_context_free(&avio_ctx_);
    }
}

bool VideoPlayer::open(const uint8_t* data, std::size_t size) {
    membuf_ = {data, size, 0};

    // Allocate AVIO buffer (FFmpeg will free it via avio_ctx_)
    constexpr int kBufSize = 64 * 1024;
    avio_buf_ = static_cast<uint8_t*>(av_malloc(kBufSize));
    if (!avio_buf_) return false;

    avio_ctx_ = avio_alloc_context(avio_buf_, kBufSize, 0,
                                   &membuf_, mem_read, nullptr, mem_seek);
    if (!avio_ctx_) { av_free(avio_buf_); return false; }

    fmt_ctx_ = avformat_alloc_context();
    if (!fmt_ctx_) return false;
    fmt_ctx_->pb = avio_ctx_;

    if (avformat_open_input(&fmt_ctx_, nullptr, nullptr, nullptr) < 0) return false;
    if (avformat_find_stream_info(fmt_ctx_, nullptr) < 0) return false;

    // Find streams
    for (unsigned i = 0; i < fmt_ctx_->nb_streams; ++i) {
        AVMediaType type = fmt_ctx_->streams[i]->codecpar->codec_type;
        if (type == AVMEDIA_TYPE_VIDEO && video_stream_ < 0) {
            video_stream_ = static_cast<int>(i);
        } else if (type == AVMEDIA_TYPE_AUDIO && audio_stream_ < 0) {
            audio_stream_ = static_cast<int>(i);
        }
    }
    if (video_stream_ < 0) return false;

    AVCodecParameters* par = fmt_ctx_->streams[video_stream_]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(par->codec_id);
    if (!codec) return false;

    codec_ctx_ = avcodec_alloc_context3(codec);
    if (!codec_ctx_) return false;
    if (avcodec_parameters_to_context(codec_ctx_, par) < 0) return false;
    if (avcodec_open2(codec_ctx_, codec, nullptr) < 0) return false;

    width_  = codec_ctx_->width;
    height_ = codec_ctx_->height;

    sws_ctx_ = sws_getContext(width_, height_, codec_ctx_->pix_fmt,
                               width_, height_, AV_PIX_FMT_YUV420P,
                               SWS_BILINEAR, nullptr, nullptr, nullptr);
    if (!sws_ctx_) return false;

    frame_     = av_frame_alloc();
    frame_rgb_ = av_frame_alloc();
    audio_frame_ = av_frame_alloc();
    packet_    = av_packet_alloc();
    if (!frame_ || !frame_rgb_ || !audio_frame_ || !packet_) return false;

    int buf_size = av_image_get_buffer_size(AV_PIX_FMT_YUV420P, width_, height_, 1);
    uint8_t* buf = static_cast<uint8_t*>(av_malloc(buf_size));
    if (!buf) return false;
    av_image_fill_arrays(frame_rgb_->data, frame_rgb_->linesize,
                         buf, AV_PIX_FMT_YUV420P, width_, height_, 1);

    if (!initAudio()) return false;

    return true;
}

bool VideoPlayer::initAudio() {
    if (audio_stream_ < 0) return true; // no audio stream

    AVCodecParameters* par = fmt_ctx_->streams[audio_stream_]->codecpar;
    const AVCodec* codec = avcodec_find_decoder(par->codec_id);
    if (!codec) return false;

    audio_codec_ctx_ = avcodec_alloc_context3(codec);
    if (!audio_codec_ctx_) return false;
    if (avcodec_parameters_to_context(audio_codec_ctx_, par) < 0) return false;
    if (avcodec_open2(audio_codec_ctx_, codec, nullptr) < 0) return false;

    SDL_AudioSpec desired{};
    desired.freq = audio_freq_;
    desired.format = AUDIO_S16SYS;
    desired.channels = static_cast<Uint8>(audio_channels_);
    desired.samples = 2048;
    desired.callback = nullptr;

    SDL_AudioSpec obtained{};
    audio_dev_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained, 0);
    if (audio_dev_ == 0) return false;

    audio_freq_ = obtained.freq;
    audio_channels_ = obtained.channels;

    AVChannelLayout out_layout;
    av_channel_layout_default(&out_layout, audio_channels_);

    if (swr_alloc_set_opts2(
            &swr_ctx_,
            &out_layout, AV_SAMPLE_FMT_S16, audio_freq_,
            &audio_codec_ctx_->ch_layout, audio_codec_ctx_->sample_fmt, audio_codec_ctx_->sample_rate,
            0, nullptr) < 0) {
        av_channel_layout_uninit(&out_layout);
        return false;
    }
    av_channel_layout_uninit(&out_layout);
    if (swr_init(swr_ctx_) < 0) return false;

    SDL_PauseAudioDevice(audio_dev_, 0);
    return true;
}

void VideoPlayer::seekToBegin() {
    membuf_.pos = 0;
    avformat_seek_file(fmt_ctx_, -1, std::numeric_limits<int64_t>::min(), 0, std::numeric_limits<int64_t>::max(), AVSEEK_FLAG_FRAME);
    avcodec_flush_buffers(codec_ctx_);
    if (audio_codec_ctx_) {
        avcodec_flush_buffers(audio_codec_ctx_);
    }
}

bool VideoPlayer::handleAudioPacket() {
    if (packet_->stream_index != audio_stream_ || !audio_codec_ctx_ || !swr_ctx_ || audio_dev_ == 0) {
        return true;
    }

    int ret = avcodec_send_packet(audio_codec_ctx_, packet_);
    if (ret < 0) return false;

    while (ret >= 0) {
        ret = avcodec_receive_frame(audio_codec_ctx_, audio_frame_);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) return true;
        if (ret < 0) return false;

        int out_samples = av_rescale_rnd(
            swr_get_delay(swr_ctx_, audio_codec_ctx_->sample_rate) + audio_frame_->nb_samples,
            audio_freq_, audio_codec_ctx_->sample_rate, AV_ROUND_UP);
        if (out_samples <= 0) {
            av_frame_unref(audio_frame_);
            continue;
        }

        uint8_t* out_buf = nullptr;
        int out_linesize = 0;
        if (av_samples_alloc(&out_buf, &out_linesize, audio_channels_, out_samples, AV_SAMPLE_FMT_S16, 0) < 0) {
            av_frame_unref(audio_frame_);
            return false;
        }

        int converted = swr_convert(
            swr_ctx_, &out_buf, out_samples,
            const_cast<const uint8_t**>(audio_frame_->data), audio_frame_->nb_samples);
        if (converted < 0) {
            av_freep(&out_buf);
            av_frame_unref(audio_frame_);
            return false;
        }

        int bytes = converted * audio_channels_ * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
        if (bytes > 0) {
            if (SDL_QueueAudio(audio_dev_, out_buf, static_cast<Uint32>(bytes)) != 0) {
                av_freep(&out_buf);
                av_frame_unref(audio_frame_);
                return false;
            }
        }
        av_freep(&out_buf);
        av_frame_unref(audio_frame_);
    }

    return true;
}

bool VideoPlayer::pumpAudio() {
    if (audio_stream_ < 0 || audio_dev_ == 0) return true;

    const Uint32 targetQueuedBytes = static_cast<Uint32>(audio_freq_ * audio_channels_ * sizeof(int16_t) / 2);
    int packetsBudget = 48;
    while (SDL_GetQueuedAudioSize(audio_dev_) < targetQueuedBytes && packetsBudget-- > 0) {
        int ret = av_read_frame(fmt_ctx_, packet_);
        if (ret == AVERROR_EOF) {
            seekToBegin();
            av_packet_unref(packet_);
            continue;
        }
        if (ret < 0) return false;
        bool ok = true;
        if (packet_->stream_index == audio_stream_) {
            ok = handleAudioPacket();
        }
        av_packet_unref(packet_);
        if (!ok) return false;
    }
    return true;
}

bool VideoPlayer::nextFrame(SDL_Renderer* renderer, SDL_Texture*& texture) {
    // Create texture on first call or if destroyed
    if (!texture) {
        texture = SDL_CreateTexture(renderer,
                                    SDL_PIXELFORMAT_IYUV,
                                    SDL_TEXTUREACCESS_STREAMING,
                                    width_, height_);
        if (!texture) return false;
    }

    while (true) {
        int ret = av_read_frame(fmt_ctx_, packet_);
        if (ret == AVERROR_EOF) {
            // Loop: seek back to start and retry
            seekToBegin();
            av_packet_unref(packet_);
            continue;
        }
        if (ret < 0) return false;

        if (packet_->stream_index == audio_stream_) {
            bool ok = handleAudioPacket();
            av_packet_unref(packet_);
            if (!ok) return false;
            continue;
        }

        if (packet_->stream_index != video_stream_) {
            av_packet_unref(packet_);
            continue;
        }

        ret = avcodec_send_packet(codec_ctx_, packet_);
        av_packet_unref(packet_);
        if (ret < 0) continue;

        ret = avcodec_receive_frame(codec_ctx_, frame_);
        if (ret == AVERROR(EAGAIN)) continue;
        if (ret < 0) return false;

        sws_scale(sws_ctx_,
                  frame_->data,     frame_->linesize,     0, height_,
                  frame_rgb_->data, frame_rgb_->linesize);

        SDL_UpdateYUVTexture(texture, nullptr,
            frame_rgb_->data[0], frame_rgb_->linesize[0],
            frame_rgb_->data[1], frame_rgb_->linesize[1],
            frame_rgb_->data[2], frame_rgb_->linesize[2]);

        av_frame_unref(frame_);
        return true;
    }
}
