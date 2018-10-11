//
// Created by qq798 on 2018/8/7.
//

#include "OnCallJava.h"

OnCallJava::OnCallJava(JavaVM * jvm, JNIEnv * jenv,jobject jobj) {
    this->jvm = jvm;
    this->jenv = jenv;
    this->jobj = jenv->NewGlobalRef(jobj);
    this->jclz = jenv->GetObjectClass(jobj);
    if(!jclz){
        if(ISDEBUG)
        {
            LOGE("get jclass wrong");
        }
        return;
    }

    jmid_onCallPrepared = jenv->GetMethodID(jclz,"onCallPrepared","()V");
    jmid_onCallFinished = jenv->GetMethodID(jclz,"onCallFinished","()V");
    jmid_onCallSchedule = jenv->GetMethodID(jclz,"onCallSchedule","(II)V");
    jmid_onCallPcmToAAc = jenv->GetMethodID(jclz,"onCallPcmToAAc","(I[B)V");
}

OnCallJava::~OnCallJava() {
    this->jvm = NULL;
    this->jenv = NULL;
    this->jobj = NULL;
}

void OnCallJava::callPrepared(int threadType) {
    if(threadType == CHILDTHREAD){
        JNIEnv * jniEnv = NULL;
        if(jvm->AttachCurrentThread(&jniEnv,NULL) != JNI_OK){
            if(ISDEBUG)
            {
                LOGE("get child thread jnienv worng");
            }
            return;
        }
        jniEnv->CallVoidMethod(jobj,jmid_onCallPrepared);
        jvm->DetachCurrentThread();
    }else if(threadType == MAINTHREAD){
        jenv->CallVoidMethod(jobj,jmid_onCallPrepared);
    }
}

void OnCallJava::callFinished(int threadType) {
    // 这个方法是销毁回调方法，有点特殊

    if(threadType == CHILDTHREAD){
        JNIEnv * jniEnv = NULL;
        if(jvm->AttachCurrentThread(&jniEnv,NULL) != JNI_OK){
            if(ISDEBUG)
            {
                LOGE("get child thread jnienv worng");
            }
            return;
        }
        jniEnv->CallVoidMethod(jobj,jmid_onCallFinished);
//        jvm->DetachCurrentThread();
    }else if(threadType == MAINTHREAD){
        jenv->CallVoidMethod(jobj,jmid_onCallFinished);
    }
}

// 这个必须是子线程调用
void OnCallJava::callSchedule(int64_t currentFrameTime, int64_t duration) {
    JNIEnv * jniEnv = NULL;
    if(jvm->AttachCurrentThread(&jniEnv,NULL) != JNI_OK){
        if(ISDEBUG)
        {
            LOGE("get child thread jnienv worng");
        }
        return;
    }
    jniEnv->CallVoidMethod(jobj,jmid_onCallSchedule,(jint)currentFrameTime,(jint)duration);
    jvm->DetachCurrentThread();
}

void OnCallJava::callPcmToAAc(int threadType,int size, void * bytes) {


    // 方法主体
    void (*method)(void *,int,JNIEnv *,jobject,jmethodID) = [](void * bytes,int size,JNIEnv * jniEnv,jobject jobj,jmethodID jmid_onCallPcmToAAc){
        jbyteArray jbyteArr = jniEnv->NewByteArray(size);
        // 将c bytes转存为java byte数组
        jniEnv->SetByteArrayRegion(jbyteArr, 0, size, static_cast<jbyte *>(bytes));

        jniEnv->CallVoidMethod(jobj,jmid_onCallPcmToAAc,size,jbyteArr);

        jniEnv->DeleteLocalRef(jbyteArr);
    };

    if(threadType == CHILDTHREAD){
        JNIEnv * jniEnv = NULL;
        if(jvm->AttachCurrentThread(&jniEnv,NULL) != JNI_OK){
            if(ISDEBUG)
            {
                LOGE("get child thread jnienv worng");
            }
            return;
        }

        (*method)(bytes,size,jniEnv,jobj,jmid_onCallPcmToAAc);

        jvm->DetachCurrentThread();
    }else if(threadType == MAINTHREAD){
        method(bytes,size,jenv,jobj,jmid_onCallPcmToAAc);
    }
}
