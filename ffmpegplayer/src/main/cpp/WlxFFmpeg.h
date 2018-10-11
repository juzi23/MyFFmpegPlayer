//
// Created by qq798 on 2018/8/7.
//

#ifndef MYFFMPEGPLAYER_WLXFFMPEG_H
#define MYFFMPEGPLAYER_WLXFFMPEG_H

#include <string>
#include "OnCallJava.h"
#include "WlxPlayerState.h"
#include "AndroidLog.h"
#include "WlxAudio.h"
#include <pthread.h>
#include <unistd.h>

extern "C"{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

class WlxFFmpeg {
public :
    char * url = NULL;
    OnCallJava * onCallJava = NULL;
    WlxPlayerState * playerState = NULL;

    AVFormatContext * formatContext = NULL;
    WlxAudio * audio = NULL;

    int mainThreadID = -1;

    // 进行准备工作的线程
    pthread_t prepareThread;
    pthread_mutex_t prepareMutex;
    bool isPrepareThreadExit = true;

    // 解包线程
    pthread_t unparkThread;
    pthread_mutex_t unparkMutex;
    //pthread_cond_t unparkCond;
    bool isUnpackThreadExit = true;

public:
    WlxFFmpeg();
    ~WlxFFmpeg();

    void setSource(const char *);

    void setOnCallJava(OnCallJava * onCallJava);

    void setPlayerState(WlxPlayerState *);

    void prepare();

    void stop();

    void asyncPrepare();

    void play();

    void pause();

    void continue1();

    void seek(uint64_t seconds);

    // 进行循环解包,请放在子线程中进行
    void unpackPacket();

    void asyncUnpackPacket();
};


#endif //MYFFMPEGPLAYER_WLXFFMPEG_H
