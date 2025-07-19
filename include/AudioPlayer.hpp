#pragma once

#include <portaudio.h>
#include <iostream>
#include <vector>
#include <string>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

class AudioPlayer{
public:
    AudioPlayer();
    ~AudioPlayer();

    bool open(const std::string &path);
    bool play();

private:
    AVFormatContext *fmtCtx_              = nullptr;
    AVCodecContext  *decCtx_              = nullptr;
    SwrContext      *swrCtx_              = nullptr;
    int             audioStream_          = -1;
    AVChannelLayout outputChannelLayout_ = AV_CHANNEL_LAYOUT_STEREO;
    PaStream *paStream_ = nullptr;

    double sampleRate_ = 0.0;
    std::vector<uint8_t> outBuf_;

    bool initPortAudio(int channels);
    void cleanup();

};
