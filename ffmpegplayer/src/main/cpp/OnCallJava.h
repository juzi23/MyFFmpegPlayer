//
// Created by qq798 on 2018/8/7.
//

#ifndef MYFFMPEGPLAYER_ONCALLJAVA_H
#define MYFFMPEGPLAYER_ONCALLJAVA_H

#include <jni.h>
#include "AndroidLog.h"

#define MAINTHREAD 0
#define CHILDTHREAD 1

class OnCallJava{
public :
    JavaVM * jvm = NULL;
    JNIEnv * jenv = NULL;
    jobject jobj = NULL;
    jclass jclz = NULL;

private:
    jmethodID jmid_onCallPrepared;
    jmethodID jmid_onCallFinished;
    jmethodID jmid_onCallSchedule;
    jmethodID jmid_onCallPcmToAAc;
public :
    OnCallJava(JavaVM *,JNIEnv *,jobject);
    ~OnCallJava();

    void callPrepared(int threadType);
    void callFinished(int threadType);

    void callSchedule(int64_t currentFrameTime, int64_t duration);

    void callPcmToAAc(int threadType,int size, void * bytes);
};

#endif //MYFFMPEGPLAYER_ONCALLJAVA_H
