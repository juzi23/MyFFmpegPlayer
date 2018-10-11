#ifndef MYFFMPEGPLAYER_NATIVE_LIB_CPP
#define MYFFMPEGPLAYER_NATIVE_LIB_CPP


#include <jni.h>
#include <string>
#include "AndroidLog.h"
#include "WlxFFmpeg.h"
#include "WlxPlayerState.h"
#include <pthread.h>

extern "C"{
#include "libavformat/avformat.h"
#include "libavcodec/avcodec.h"
}

// 代表当前java虚拟机
JavaVM * jvm;

WlxFFmpeg * fFmpeg = NULL;
OnCallJava * onCallJava = NULL;
WlxPlayerState * playerState = NULL;

// 获取当前java虚拟机
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void* reserved)
{
    JNIEnv *env;
    jvm = vm;
    if(vm->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK)
    {
        return -1;
    }
    return JNI_VERSION_1_6;
}

extern "C"
JNIEXPORT jstring JNICALL
Java_com_wlx_ffmpegplayer_FFmpegPlayer_n_1version(JNIEnv *env, jobject instance) {
    return env->NewStringUTF("version: 1.0");
}

// 打印支持的所有编解码器
extern "C"
JNIEXPORT void JNICALL
Java_com_wlx_ffmpegplayer_FFmpegPlayer_n_1logAllCodec(JNIEnv *env, jobject instance) {
    av_register_all();
    AVCodec *c_temp = av_codec_next(NULL);
    while (c_temp != NULL)
    {
        switch (c_temp->type)
        {
            case AVMEDIA_TYPE_VIDEO:
                LOGD("[Video]:%s", c_temp->name);
                break;
            case AVMEDIA_TYPE_AUDIO:
                LOGD("[Audio]:%s", c_temp->name);
                break;
            default:
                LOGD("[Other]:%s", c_temp->name);
                break;
        }
        c_temp = c_temp->next;
    }
    return ;
}

extern "C"
JNIEXPORT void JNICALL
Java_com_wlx_ffmpegplayer_FFmpegPlayer_n_1prepare(JNIEnv *env, jobject instance) {
    // 防止重复播放
    if (playerState != NULL&&playerState->currentPlayState == EnumCurrentPlayState::EXIT) {
        playerState->currentPlayState = EnumCurrentPlayState::PLAYING;
        fFmpeg->asyncPrepare();
    }
}

// 结束播放器功能，释放所有资源
extern "C"
JNIEXPORT void JNICALL
Java_com_wlx_ffmpegplayer_FFmpegPlayer_n_1stop(JNIEnv *env, jobject instance) {
    // 没退出的时候进行退出
    if(playerState != NULL && playerState->currentPlayState != EnumCurrentPlayState::EXIT){
        playerState->currentPlayState = EnumCurrentPlayState::EXIT;

        if(fFmpeg != NULL){
            delete(fFmpeg);
            fFmpeg = NULL;
        }

        if(ISDEBUG){
            LOGD("fFmpeg退出成功");
        }

        if(onCallJava != NULL){
            delete(onCallJava);
            onCallJava = NULL;
        }

        if(playerState != NULL){
            delete(playerState);
            playerState = NULL;
        }
    }
}

// 初始化fFmpeg,设置源数据
extern "C"
JNIEXPORT void JNICALL
Java_com_wlx_ffmpegplayer_FFmpegPlayer_n_1setSource(JNIEnv *env, jobject instance, jstring url_jstr) {
    const char * url_cstr = (env->GetStringUTFChars(url_jstr, NULL));

    if(url_cstr == NULL||strlen(url_cstr)==0){
        if(ISDEBUG){
            LOGE("没有播放源，请设置播放源！");
        }
        return;
    }

    if(onCallJava == NULL){
        onCallJava = new OnCallJava(jvm,env,instance);
    }

    if(playerState == NULL){
        playerState = new WlxPlayerState();
    }

    if(fFmpeg == NULL){
        fFmpeg = new WlxFFmpeg();
    }


    fFmpeg->setSource(url_cstr);
    fFmpeg->setOnCallJava(onCallJava);
    fFmpeg->setPlayerState(playerState);

    env->ReleaseStringUTFChars(url_jstr, url_cstr);
}

extern "C"
JNIEXPORT void JNICALL
Java_com_wlx_ffmpegplayer_FFmpegPlayer_n_1play(JNIEnv *env, jobject instance) {

    if(fFmpeg != NULL){
        fFmpeg->play();
    }

}
extern "C"
JNIEXPORT void JNICALL
Java_com_wlx_ffmpegplayer_FFmpegPlayer_n_1pause(JNIEnv *env, jobject instance) {

    if(fFmpeg!=NULL){
        fFmpeg->pause();
    }

}

extern "C"
JNIEXPORT void JNICALL
Java_com_wlx_ffmpegplayer_FFmpegPlayer_n_1continue(JNIEnv *env, jobject instance) {

    if(fFmpeg!=NULL){
        fFmpeg->continue1();
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_com_wlx_ffmpegplayer_FFmpegPlayer_n_1seek(JNIEnv *env, jobject instance, jint progress) {

    if(fFmpeg!=NULL){
        fFmpeg->seek(progress);
    }

}
extern "C"
JNIEXPORT jint JNICALL
Java_com_wlx_ffmpegplayer_FFmpegPlayer_n_1getSampleRate(JNIEnv *env, jobject instance) {

    if(fFmpeg!=NULL){
        return fFmpeg->audio->codecpar->sample_rate;
    }
    return 0;
}
#endif //MYFFMPEGPLAYER_NATIVE_LIB_CPP
extern "C"
JNIEXPORT void JNICALL
Java_com_wlx_ffmpegplayer_FFmpegPlayer_n_1setIsRecoding(JNIEnv *env, jobject instance,
                                                        jboolean isRecoding) {

    if(fFmpeg!=NULL){
        fFmpeg->audio->setIsRecoding(isRecoding);
    }

}