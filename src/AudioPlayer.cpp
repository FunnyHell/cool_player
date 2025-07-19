#include "AudioPlayer.hpp"

AudioPlayer::AudioPlayer() {
    av_log_set_level(AV_LOG_ERROR);
    avformat_network_init();

    if (Pa_Initialize() != paNoError) {
        std::cerr << "[PortAudio] Initialization failed\n";
    }
}

AudioPlayer::~AudioPlayer() {
    cleanup();
    Pa_Terminate();
    avformat_network_deinit();
}

bool AudioPlayer::open(const std::string &path) {
    if (avformat_open_input(&fmtCtx_, path.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "[FFmpeg] Failed while opening file " << path << "\n";
        return false;
    }
    if (avformat_find_stream_info(fmtCtx_, nullptr) < 0) {
        std::cerr << "[FFmpeg Failed initialization stream parameters\n";
        return false;
    }

    const AVCodec *codec = nullptr;
    audioStream_ = av_find_best_stream(fmtCtx_, AVMEDIA_TYPE_AUDIO, -1, -1,
                                        &codec, 0);
    if (audioStream_ < 0) {
        std::cerr << "[FFmpeg] Audiostream not found\n";
        return false;
    }

    AVChannelLayout channelLayout = decCtx_->ch_layout;
    if (channelLayout.u.mask == 0) {
        av_channel_layout_default(&channelLayout, 
                                    decCtx_->ch_layout.nb_channels);
    }

    sampleRate_ = decCtx_->sample_rate;
    swr_alloc_set_opts2(&swrCtx_, &outputChannelLayout_, AV_SAMPLE_FMT_S16,
                        decCtx_->sample_rate, &channelLayout, 
                        decCtx_->sample_fmt, decCtx_->sample_rate, 0, nullptr);
    if (!swrCtx_ || swr_init(swrCtx_) < 0) {
        std::cerr << "[SwResample] Failed Initialization\n";
        return false;
    }

    return initPortAudio(2);
}

bool AudioPlayer::initPortAudio(int channels) {
    PaStreamParameters out{};
    out.device = Pa_GetDefaultInputDevice();
    if (out.device == paNoDevice) {
        std::cerr << "[PortAudio] Output device not found\n";
        return false;
    }
    out.channelCount = 2;
    out.sampleFormat = paInt16;
    out.suggestedLatency = Pa_GetDeviceInfo(out.device)->defaultLowOutputLatency;
    out.hostApiSpecificStreamInfo = nullptr;

    double streamSampleRate = decCtx_->sample_rate;
    if (Pa_OpenStream(&paStream_, nullptr, &out, streamSampleRate,
                      paFramesPerBufferUnspecified, paClipOff, nullptr,
                      nullptr) != paNoError) {
        std::cerr << "[PortAudio] Failed while opening output stream\n";
        return false;
    }
    Pa_StartStream(paStream_);
    return true;
}

bool AudioPlayer::play() {
    if (!fmtCtx_ || !decCtx_ || !paStream_) {
        std::cerr << "[AudioPlayer] Not Initialized\n";
        return false;
    }

    AVPacket *pkt = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();

    while (av_read_frame(fmtCtx_, pkt) >= 0) {
        if (pkt->stream_index != audioStream_) {
            av_packet_unref(pkt);
            continue;
        }

        if (avcodec_send_packet(decCtx_, pkt) < 0) {
            av_packet_unref(pkt);
            continue;
        }
        av_packet_unref(pkt);

        while (avcodec_receive_frame(decCtx_, frame) == 0) {
            int maxOut = swr_get_out_samples(swrCtx_, frame->nb_samples);
            int size = av_samples_get_buffer_size(nullptr, 2, maxOut,
                                                  AV_SAMPLE_FMT_S16, 1);
            outBuf_.resize(size);
            uint8_t *outPtr[1] = { outBuf_.data() };
            int converted = swr_convert(swrCtx_, outPtr, maxOut,
                                        (const uint8_t **)frame->extended_data,
                                        frame->nb_samples);
            if (Pa_WriteStream(paStream_, outBuf_.data(), converted) != paNoError) {
                std::cerr << "[PortAudio] Ошибка записи (underrun)\n";
                av_frame_unref(frame);
                av_packet_free(&pkt);
                av_frame_free(&frame);
                return false;
            }
            av_frame_unref(frame);
        }
    }

    avcodec_send_packet(decCtx_, nullptr);
    while (avcodec_receive_frame(decCtx_, frame) == 0) {
        int maxOut = swr_get_out_samples(swrCtx_, frame->nb_samples);
        int size   = av_samples_get_buffer_size(nullptr, 2, maxOut, AV_SAMPLE_FMT_S16, 1);
        outBuf_.resize(size);
        uint8_t *outPtr[1] = { outBuf_.data() };
        int converted = swr_convert(swrCtx_, outPtr, maxOut,
                                    (const uint8_t **)frame->extended_data, frame->nb_samples);
        Pa_WriteStream(paStream_, outBuf_.data(), converted);
        av_frame_unref(frame);
    }

    av_packet_free(&pkt);
    av_frame_free(&frame);

    return true;
}

void AudioPlayer::cleanup() {
    if (paStream_) {
        Pa_StopStream(paStream_);
        Pa_CloseStream(paStream_);
        paStream_ = nullptr;
    }
    if (swrCtx_) {
        swr_free(&swrCtx_);
    }
    if (decCtx_) {
        avcodec_free_context(&decCtx_);
    }
    if (fmtCtx_) {
        avformat_close_input(&fmtCtx_);
    }
}

