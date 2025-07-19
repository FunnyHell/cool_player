#include <iostream>
#include "AudioPlayer.hpp"

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <audio_file>\n";
        return 1;
    }
    char *audioname = argv[1];
    const float volume = (argc > 2) 
            ? ((atof(argv[2]) >= 1 && atof(argv[2]) <= 100.0f) 
                ? atof(argv[2]) / 100.0f 
                : (atof(argv[2]) >= 0.0f 
                    ? atof(argv[2]) 
                    : 1.0f))
            :1.0f;
    //Логируем только ошибки
    av_log_set_level(AV_LOG_ERROR);

    AVFormatContext *fmtCtx = nullptr;
    if (avformat_open_input(&fmtCtx, audioname, nullptr, nullptr) < 0) {
        std::cerr << "Не удалось открыть файл " << audioname << "\n";
        return 1;
    }
    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        std::cerr << "Не удалось получить информацию о потоках\n";
        return 1;
    }

    const AVCodec *codec = nullptr;
    int audioStream = av_find_best_stream(
        fmtCtx, 
        AVMEDIA_TYPE_AUDIO, 
        -1, -1, &codec, 0
    );
    if (audioStream < 0) {
        std::cerr << "Аудиопоток не найден\n";
        return 1;
    }
    AVStream *stream = fmtCtx->streams[audioStream];
    AVCodecParameters *codecPar = stream->codecpar;


    AVCodecContext *decCtx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(decCtx, codecPar);
    if (avcodec_open2(decCtx, codec, nullptr) < 0) {
        std::cerr << "Не удалось открыть декодер\n";
        return 1;
    }
    AVChannelLayout chlt = decCtx->ch_layout;
    if (chlt.u.mask == 0) {
        av_channel_layout_default(&chlt, decCtx->ch_layout.nb_channels);
    }
    AVChannelLayout outputChannelLayout = AV_CHANNEL_LAYOUT_STEREO;
    SwrContext *swr = nullptr;
    swr_alloc_set_opts2(
        &swr,
        &outputChannelLayout,
        AV_SAMPLE_FMT_S16,
        decCtx->sample_rate,
        &chlt,
        decCtx->sample_fmt,
        decCtx->sample_rate,
        0, nullptr);
      swr_init(swr);

    if (Pa_Initialize() != paNoError) {
        std::cerr << "portaudio: failed intialization\n";
        return 1;
    }
    PaStreamParameters outParams{};
    outParams.device = Pa_GetDefaultOutputDevice();
    if (outParams.device == paNoDevice) {
        std::cerr << "portaudio: audiodevice not found\n";
        return 1;
    }
    outParams.channelCount = 2;
    outParams.sampleFormat = paInt16;
    outParams.suggestedLatency = Pa_GetDeviceInfo(outParams.device)->defaultLowOutputLatency;
    outParams.hostApiSpecificStreamInfo = nullptr;

    double streamSampleRate = decCtx->sample_rate;
    PaStream *paStream;
    if (Pa_OpenStream(&paStream, nullptr, &outParams, streamSampleRate,
                      paFramesPerBufferUnspecified, paClipOff, nullptr,
                      nullptr) != paNoError) {
        std::cerr << "portaudio: failed while opening output stream\n";
        return 1;
    }
    Pa_StartStream(paStream);

    
    AVPacket *pkt = av_packet_alloc(); // Сжатые данные из контейнера
    AVFrame *frame = av_frame_alloc(); // Декодированные сырые семплы
    std::vector<uint8_t> outBuf;

    while (av_read_frame(fmtCtx, pkt) >= 0) {
        if (pkt->stream_index != audioStream) {
            av_packet_unref(pkt);
            continue;
        }

        if (avcodec_send_packet(decCtx, pkt) < 0) {
            av_packet_unref(pkt);
            continue;
        }
        av_packet_unref(pkt);

        while (avcodec_receive_frame(decCtx, frame) == 0) {
            int maxOutSamples = swr_get_out_samples(swr, frame->nb_samples);
            int outBufSize = av_samples_get_buffer_size(nullptr, 2, 
                                                         maxOutSamples, 
                                                         AV_SAMPLE_FMT_S16, 
                                                         1);
            outBuf.resize(outBufSize);
            uint8_t *outPtrs[1] = { outBuf.data() };
            int converted = swr_convert(swr, outPtrs, maxOutSamples,
                                   (const uint8_t **)frame->extended_data,
                                    frame->nb_samples);
            int bytesToWrite = converted * 2 * 
                               av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
            
            int16_t *samples = reinterpret_cast<int16_t*>(outBuf.data());
            int totalSamples = converted * 2;
            for (int i = 0; i < totalSamples; ++i) {
                int32_t v = static_cast<int32_t>(samples[i] * volume);
                if (v > 32767) v = 32767;
                if (v < -32768) v = -32768;

                samples[i] = static_cast<int16_t>(v);
            }

            if(Pa_WriteStream(paStream, outBuf.data(), converted)
                                                    != paNoError) {
               std::cerr << "portaudio: writing error (buffer underrun?)\n";
               return 1;
            }
            av_frame_unref(frame);
        }
    }


    return 0;
}
