#pragma once
#include <SDL2/SDL.h>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/imgutils.h>
#include <libavutil/samplefmt.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
#include <cstdint>
#include <cstddef>

// Opaque video player. Call open() once, then pump() each frame.
class VideoPlayer {
public:
    VideoPlayer() = default;
    ~VideoPlayer();

    // Memory buffer used by FFmpeg custom IO (must be public for static callbacks).
    struct MemBuf { const uint8_t* data; std::size_t size; std::size_t pos; };

    // Open the video from an in-memory buffer.
    // The buffer must remain valid for the lifetime of the player.
    bool open(const uint8_t* data, std::size_t size);

    // Decode the next frame into 'texture' (IYUV / YUV420 format).
    // Returns false only on an unrecoverable error; loops automatically.
    bool nextFrame(SDL_Renderer* renderer, SDL_Texture*& texture);
    // Keep the audio queue filled. Safe to call every frame.
    bool pumpAudio();

    int videoWidth()  const { return width_;  }
    int videoHeight() const { return height_; }

private:
    void seekToBegin();
    bool initAudio();
    bool handleAudioPacket();

    // Memory IO
    MemBuf membuf_{};
    AVIOContext*    avio_ctx_    = nullptr;
    uint8_t*        avio_buf_    = nullptr;

    // FFmpeg objects
    AVFormatContext* fmt_ctx_    = nullptr;
    AVCodecContext*  codec_ctx_  = nullptr;
    AVCodecContext*  audio_codec_ctx_ = nullptr;
    SwsContext*      sws_ctx_    = nullptr;
    SwrContext*      swr_ctx_    = nullptr;
    AVFrame*         frame_      = nullptr;
    AVFrame*         frame_rgb_  = nullptr;
    AVFrame*         audio_frame_ = nullptr;
    AVPacket*        packet_     = nullptr;

    int              video_stream_ = -1;
    int              audio_stream_ = -1;
    int              width_        = 0;
    int              height_       = 0;
    SDL_AudioDeviceID audio_dev_ = 0;
    int              audio_freq_ = 48000;
    int              audio_channels_ = 2;
};
