//
// Created by qq798 on 2018/8/7.
//

#include "WlxAudio.h"


WlxAudio::WlxAudio(WlxPlayerState *playerState, OnCallJava * onCallJava,int index, AVCodecParameters *codecpar) {
    this->playerState = playerState;
    this->onCallJava = onCallJava;
    this->audioIndex = index;
    this->codecpar = codecpar;

    queue = new WlxQueue(playerState);
    buffer = static_cast<uint8_t *>(malloc(codecpar->sample_rate * codecpar->channels * av_get_bytes_per_sample(
                (AVSampleFormat)(codecpar->format))));
}

WlxAudio::~WlxAudio() {
    destoryOpenSLES();

    this->playerState = NULL;
    delete(queue);

    audioIndex = -1;
    if(codecContext !=NULL){
        avcodec_close(codecContext);
        avcodec_free_context(&codecContext);
        codecContext = NULL;
    }
    codecpar = NULL;
    codec = NULL;

    free(buffer);
    buffer = NULL;

    if (swrContext != NULL) {
        swr_free(&swrContext);
        swrContext = NULL;
    }

    onCallJava = NULL;
}

// 实际播放的子线程内方法
void * play(void * data){
    WlxAudio * audio = static_cast<WlxAudio *>(data);

    //audio->resampleAudio();

    // 将重采样后数据放入opensl缓冲队列，进行播放
    // 等缓冲区中有足够数据，才进行播放
    while(audio->playerState!=NULL
          && audio->playerState->currentPlayState!=EnumCurrentPlayState::EXIT
          && audio->queue->size()<audio->queue->queueMaxSize/2) {
        // 睡0.1秒

        if(ISDEBUG){
            LOGD("小睡一会，等等缓存");
        }

        usleep(1000*100);
    }

    audio->initOpenSLES();

    if(ISDEBUG){
        LOGD("WlxAduio结束了一个线程，进行paly工作");
    }
    pthread_exit(&audio->thread_play);
}

// 开启子线程，进行播放
void WlxAudio::asyncPlay() {
    if(ISDEBUG){
        LOGD("WlxAduio开启了一个线程，进行paly工作");
    }
    pthread_create(&thread_play,NULL,play,this);
}

int WlxAudio::resampleAudio() {
    // 重采样的缓冲区大小
    dataSize = 0;

    while (playerState!=NULL && playerState->currentPlayState!=EnumCurrentPlayState::EXIT) {
        AVPacket * packet = av_packet_alloc();

        int ret = -1;
        ret = queue->pop(packet);

        if(ret != 0){
            if(ISDEBUG){
                LOGD("解码完成！");
            }

            av_packet_free(&packet);
            av_free(packet);
            packet = NULL;
            continue;
        }
        ret = -1;

        // packet -> frame

        // 将待解码的包发送到解码器上下文中
        ret = avcodec_send_packet(codecContext,packet);
        if(ret !=0){
            av_packet_free(&packet);
            av_free(packet);
            packet = NULL;
            continue;
        }
        ret = -1;

        // 解码，并将解码后数据保存到frame中
        AVFrame * frame = av_frame_alloc();
        ret = avcodec_receive_frame(codecContext,frame);
        if(ret != 0){
            av_frame_free(&frame);
            av_free(frame);
            frame = NULL;

            av_packet_free(&packet);
            av_free(packet);
            packet = NULL;

            continue;
        }
        ret = -1;

        // 校正声道数和声道布局
        if(frame->channels > 0 && frame->channel_layout ==0){
            // 声道布局有问题时
            frame->channel_layout = av_get_default_channel_layout(frame->channels);
        }else if(frame->channel_layout>0 && frame->channels == 0){
            // 声道数有问题
            frame->channels = av_get_channel_layout_nb_channels(frame->channel_layout);
        }

        // 获取当前播放帧时间
        this->setCurrentFrameTime(frame->pts * av_q2d(time_base));

        // 重采样，将重采样数据保存至buffer中
        if (swrContext == NULL) {
            swrContext = swr_alloc_set_opts(
                    NULL,
                    AV_CH_LAYOUT_STEREO,
                    AV_SAMPLE_FMT_S16,
                    frame->sample_rate,
                    frame->channel_layout,
                    (AVSampleFormat)(frame->format),
                    frame->sample_rate,
                    NULL,NULL
            );
        }

        if(swrContext == NULL || swr_init(swrContext) < 0){
            swr_free(&swrContext);

            av_frame_free(&frame);
            av_free(frame);
            frame = NULL;

            av_packet_free(&packet);
            av_free(packet);
            packet = NULL;

            continue;
        }

        dataSize = swr_convert(swrContext,
                    &buffer,
                    frame->nb_samples,
                    (const uint8_t **)(frame->data),
                    frame->nb_samples);

        dataSize *= av_get_channel_layout_nb_channels(AV_CH_LAYOUT_STEREO) * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

        av_frame_free(&frame);
        av_free(frame);
        frame = NULL;

        av_packet_free(&packet);
        av_free(packet);
        packet = NULL;

        // 此时得到了想要的数据，跳出循环
        break;
    }

    return dataSize;
}

void pcmBufferCallBack(SLAndroidSimpleBufferQueueItf bf, void * context)
{
    WlxAudio *wlxAudio = (WlxAudio *) context;
    if(wlxAudio == NULL){
        return;
    }
    if(wlxAudio->playerState==NULL || wlxAudio->playerState->currentPlayState==EnumCurrentPlayState::EXIT){
        return;
    }

    int buffersize = wlxAudio->resampleAudio();
    if(buffersize <= 0)
    {
        return;
    }
    // 将pcm数据编码写入本地文件中
    if(wlxAudio->isRecoding && wlxAudio->onCallJava!=NULL){
        wlxAudio->onCallJava->callPcmToAAc(CHILDTHREAD,buffersize,wlxAudio->buffer);
    }
    (* wlxAudio-> pcmBufferQueue)->Enqueue( wlxAudio->pcmBufferQueue, (char *) wlxAudio-> buffer, buffersize);
}

void WlxAudio::initOpenSLES() {

    SLresult result;
    result = slCreateEngine(&engineObject, 0, 0, 0, 0, 0);
    result = (*engineObject)->Realize(engineObject, SL_BOOLEAN_FALSE);
    result = (*engineObject)->GetInterface(engineObject, SL_IID_ENGINE, &engineEngine);

    //第二步，创建混音器
    const SLInterfaceID mids[1] = {SL_IID_ENVIRONMENTALREVERB};
    const SLboolean mreq[1] = {SL_BOOLEAN_FALSE};
    result = (*engineEngine)->CreateOutputMix(engineEngine, &outputMixObject, 1, mids, mreq);
    (void)result;
    result = (*outputMixObject)->Realize(outputMixObject, SL_BOOLEAN_FALSE);
    (void)result;
    result = (*outputMixObject)->GetInterface(outputMixObject, SL_IID_ENVIRONMENTALREVERB, &outputMixEnvironmentalReverb);
    if (SL_RESULT_SUCCESS == result) {
        result = (*outputMixEnvironmentalReverb)->SetEnvironmentalReverbProperties(
                outputMixEnvironmentalReverb, &reverbSettings);
        (void)result;
    }
    SLDataLocator_OutputMix outputMix = {SL_DATALOCATOR_OUTPUTMIX, outputMixObject};
    SLDataSink audioSnk = {&outputMix, 0};


    // 第三步，配置PCM格式信息
    SLDataLocator_AndroidSimpleBufferQueue android_queue={SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,2};

    SLDataFormat_PCM pcm={
            SL_DATAFORMAT_PCM,//播放pcm格式的数据
            2,//2个声道（立体声）
            getCurrentSampleRateForOpensles(codecpar->sample_rate),//44100hz的频率
            SL_PCMSAMPLEFORMAT_FIXED_16,//位数 16位
            SL_PCMSAMPLEFORMAT_FIXED_16,//和位数一致就行
            SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT,//立体声（前左前右）
            SL_BYTEORDER_LITTLEENDIAN//结束标志
    };
    SLDataSource slDataSource = {&android_queue, &pcm};


    const SLInterfaceID ids[1] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[1] = {SL_BOOLEAN_TRUE};

    (*engineEngine)->CreateAudioPlayer(engineEngine, &pcmPlayerObject, &slDataSource, &audioSnk, 1, ids, req);
    //初始化播放器
    (*pcmPlayerObject)->Realize(pcmPlayerObject, SL_BOOLEAN_FALSE);

//    得到接口后调用  获取Player接口
    (*pcmPlayerObject)->GetInterface(pcmPlayerObject, SL_IID_PLAY, &pcmPlayerPlay);

//    注册回调缓冲区 获取缓冲队列接口
    (*pcmPlayerObject)->GetInterface(pcmPlayerObject, SL_IID_BUFFERQUEUE, &pcmBufferQueue);
    //缓冲接口回调
    (*pcmBufferQueue)->RegisterCallback(pcmBufferQueue, pcmBufferCallBack, this);
//    获取播放状态接口
    (*pcmPlayerPlay)->SetPlayState(pcmPlayerPlay, SL_PLAYSTATE_PLAYING);
    pcmBufferCallBack(pcmBufferQueue, this);
}

void WlxAudio::destoryOpenSLES() {
    (*pcmPlayerPlay)->SetPlayState(pcmPlayerPlay,SL_PLAYSTATE_STOPPED);

    if(pcmPlayerObject != NULL){
        (*pcmPlayerObject)->Destroy(pcmPlayerObject);
        pcmPlayerPlay = NULL;
        pcmPlayerObject = NULL;
        pcmBufferQueue = NULL;
    }


    if(outputMixObject != NULL){
        (*outputMixObject)->Destroy(outputMixObject);
        outputMixObject = NULL;
        outputMixEnvironmentalReverb = NULL;
    }



    if(engineObject != NULL){
        (*engineObject)->Destroy(engineObject);
        engineObject = NULL;
        engineEngine = NULL;
    }

}

SLuint32 WlxAudio::getCurrentSampleRateForOpensles(int sample_rate) {
    SLuint32 rate = 0;
    switch (sample_rate)
    {
        case 8000:
            rate = SL_SAMPLINGRATE_8;
            break;
        case 11025:
            rate = SL_SAMPLINGRATE_11_025;
            break;
        case 12000:
            rate = SL_SAMPLINGRATE_12;
            break;
        case 16000:
            rate = SL_SAMPLINGRATE_16;
            break;
        case 22050:
            rate = SL_SAMPLINGRATE_22_05;
            break;
        case 24000:
            rate = SL_SAMPLINGRATE_24;
            break;
        case 32000:
            rate = SL_SAMPLINGRATE_32;
            break;
        case 44100:
            rate = SL_SAMPLINGRATE_44_1;
            break;
        case 48000:
            rate = SL_SAMPLINGRATE_48;
            break;
        case 64000:
            rate = SL_SAMPLINGRATE_64;
            break;
        case 88200:
            rate = SL_SAMPLINGRATE_88_2;
            break;
        case 96000:
            rate = SL_SAMPLINGRATE_96;
            break;
        case 192000:
            rate = SL_SAMPLINGRATE_192;
            break;
        default:
            rate =  SL_SAMPLINGRATE_44_1;
    }
    return rate;
}

void WlxAudio::pause() {
    (*pcmPlayerPlay)->SetPlayState(pcmPlayerPlay,SL_PLAYSTATE_PAUSED);
}

void WlxAudio::continue1() {
    (*pcmPlayerPlay)->SetPlayState(pcmPlayerPlay,SL_PLAYSTATE_PLAYING);
}

void WlxAudio::setDuration(int64_t i) {
    this->duration = i;
}

void WlxAudio::setTimeBase(AVRational time_base) {
    this->time_base = time_base;
}

void WlxAudio::setCurrentFrameTime(double d) {
    int64_t currentTime = (int64_t)d;
    if (currentTime != this->currentFrameTime) {
        this->currentFrameTime = currentTime;
        this->onCallJava->callSchedule(currentFrameTime,duration);
    }
}

void WlxAudio::seek(uint64_t seconds, AVFormatContext *formatContext,
                    void (*pFunction)(uint64_t, AVFormatContext *)) {
    if(queue!=NULL){
        queue->seek(seconds, formatContext, pFunction);
    }
}

void WlxAudio::setIsRecoding(jboolean isRecoding) {
    this->isRecoding = isRecoding;
}


