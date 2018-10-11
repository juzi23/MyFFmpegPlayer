//
// Created by qq798 on 2018/8/7.
//

#ifndef MYFFMPEGPLAYER_WLXAUDIO_H
#define MYFFMPEGPLAYER_WLXAUDIO_H

#include "WlxQueue.h"
#include "WlxPlayerState.h"
#include "OnCallJava.h"
#include <unistd.h>

extern "C"{
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"

#include <SLES/OpenSLES.h>
#include <SLES/OpenSLES_Android.h>
}

class WlxAudio {
public:
    WlxPlayerState * playerState = NULL;
    OnCallJava *onCallJava = NULL;

    int audioIndex = -1;
    AVRational time_base;
    AVCodecParameters * codecpar = NULL;
    AVCodec * codec = NULL;
    AVCodecContext * codecContext = NULL;
    WlxQueue * queue=NULL;

    jboolean isRecoding = false;

    // 音频流时长（s）
    int64_t duration = 0;

    // 当前播放帧时间戳（s）
    int64_t currentFrameTime;

    /**
     * 进行实际播放的线程
     */
    pthread_t thread_play;

    // 重采样上下文
    SwrContext * swrContext= NULL;
    /**
     * 实际可播放缓冲区
     */
    uint8_t * buffer = NULL;
    // 重采样的缓冲区大小
    int dataSize = 0;

    // 引擎
    SLObjectItf engineObject = NULL;
    SLEngineItf engineEngine = NULL;

    // 混音器
    SLObjectItf outputMixObject = NULL;
    SLEnvironmentalReverbItf outputMixEnvironmentalReverb = NULL;
    SLEnvironmentalReverbSettings reverbSettings = SL_I3DL2_ENVIRONMENT_PRESET_STONECORRIDOR;

    //pcm 播放器
    SLObjectItf pcmPlayerObject = NULL;
    SLPlayItf pcmPlayerPlay = NULL;

    //缓冲器队列接口
    SLAndroidSimpleBufferQueueItf pcmBufferQueue = NULL;
public:
    WlxAudio(WlxPlayerState * playerState,OnCallJava * onCallJava,int index,AVCodecParameters * codecpar);
    ~WlxAudio();

    void asyncPlay();
    // 对avframe进行重采样，返回占用buffer大小
    int resampleAudio();

    // 初始化openSLES
    void initOpenSLES();
    // 销毁openSLES,回收资源
    void destoryOpenSLES();

    void pause();

    void continue1();

    SLuint32 getCurrentSampleRateForOpensles(int sample_rate);

    void setDuration(int64_t i);

    void setTimeBase(AVRational time_base);

    void setCurrentFrameTime(double d);


    void seek(uint64_t seconds, AVFormatContext *formatContext,
                  void (*pFunction)(uint64_t, AVFormatContext *));

    void setIsRecoding(jboolean isRecoding);
};


#endif //MYFFMPEGPLAYER_WLXAUDIO_H
