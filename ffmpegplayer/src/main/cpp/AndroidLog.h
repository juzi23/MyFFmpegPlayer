//
// Created by qq798 on 2018/8/7.
//

#ifndef MYFFMPEGPLAYER_ANDROIDLOG_H
#define MYFFMPEGPLAYER_ANDROIDLOG_H

#include <android/log.h>

#define ISDEBUG true

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "System.out", __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, "System.out", __VA_ARGS__)

#endif //MYFFMPEGPLAYER_ANDROIDLOG_H
