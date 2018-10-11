//
// Created by qq798 on 2018/8/7.
//

#include "WlxFFmpeg.h"

WlxFFmpeg::WlxFFmpeg() {
    mainThreadID = gettid();

    pthread_mutex_init(&prepareMutex,NULL);
    pthread_mutex_init(&unparkMutex,NULL);
    //pthread_cond_init(&unparkCond,NULL);
}

WlxFFmpeg::~WlxFFmpeg() {
    stop();

    this->url = NULL;
    this->onCallJava = NULL;
    this->playerState = NULL;


    pthread_mutex_destroy(&prepareMutex);
    pthread_mutex_destroy(&unparkMutex);
    //pthread_cond_destroy(&unparkCond);
}

void WlxFFmpeg::setOnCallJava(OnCallJava *onCallJava) {
    this->onCallJava = onCallJava;
}

void WlxFFmpeg::setPlayerState(WlxPlayerState * playerState1) {
    this->playerState = playerState1;
}

int formatContextCallback(void * ctx){
    WlxFFmpeg *fFmpeg = (WlxFFmpeg *)ctx;

    if(fFmpeg->playerState->currentPlayState == EnumCurrentPlayState::EXIT){
        return AVERROR_EOF;
    }

    return 0;
}

// 这个方法与stop()方法互斥
void WlxFFmpeg::prepare() {
    pthread_mutex_lock(&prepareMutex);
    isPrepareThreadExit = false;

    int ret = -1;

    av_register_all();
    avformat_network_init();

    // 打开源文件，获取格式上下文
    formatContext = avformat_alloc_context();

    // 设置连接时回调方法
    formatContext->interrupt_callback.callback = formatContextCallback;
    formatContext->interrupt_callback.opaque = this;

    ret = avformat_open_input(&formatContext,url,NULL,NULL);
    if(ret!=0){
        if(ISDEBUG){
            LOGE("不能生成格式上下文，请检查 %s 地址是否有效！",url);
        }

        isPrepareThreadExit = true;
        pthread_mutex_unlock(&prepareMutex);
        return;
    }
    ret = -1;

    // 查询流信息，获取音频流
    ret = avformat_find_stream_info(formatContext,NULL);
    if(ret!=0){
        if(ISDEBUG){
            LOGE("该url地址不包含有效的音视频流！");
        }

        isPrepareThreadExit = true;
        pthread_mutex_unlock(&prepareMutex);
        return;
    }
    ret = -1;
    int index = -1;
    for(int i=0;i<formatContext->nb_streams;i++){
        if(formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
            index = i;
            break;
        }
    }
    if (index == -1) {
        if(ISDEBUG){
            LOGE("该url地址不包含有效的音频流！");
        }

        isPrepareThreadExit = true;
        pthread_mutex_unlock(&prepareMutex);
        return;
    }
    audio = new WlxAudio(playerState,onCallJava,index,formatContext->streams[index]->codecpar);

    audio->setDuration(formatContext->duration / AV_TIME_BASE);
    audio->setTimeBase(formatContext->streams[audio->audioIndex]->time_base);

    // 获取音频流解码器，解码器上下文
    audio->codec = avcodec_find_decoder(audio->codecpar->codec_id);
    if(!audio->codec){
        if(ISDEBUG){
            LOGE("没有找到相应解码器，解码器ID为 %d ！",audio->codecpar->codec_id);
        }
        delete(audio);

        isPrepareThreadExit = true;
        pthread_mutex_unlock(&prepareMutex);
        return;
    }
    audio->codecContext = avcodec_alloc_context3(audio->codec);
    if(!audio->codecContext){
        if(ISDEBUG){
            LOGE("解码器上下文创建失败");
        }
        delete(audio);

        isPrepareThreadExit = true;
        pthread_mutex_unlock(&prepareMutex);
        return;
    }
    ret = avcodec_parameters_to_context(audio->codecContext,audio->codecpar);
    if(ret!=0){
        if(ISDEBUG){
            LOGE("解码器参数拷贝失败");
        }
        delete(audio);

        isPrepareThreadExit = true;
        pthread_mutex_unlock(&prepareMutex);
        return;
    }
    ret = -1;

    // 打开解码器
    ret = avcodec_open2(audio->codecContext,audio->codec,NULL);
    if(ret!=0){
        if(ISDEBUG){
            LOGE("解码器打开失败");
        }
        delete(audio);

        isPrepareThreadExit = true;
        pthread_mutex_unlock(&prepareMutex);
        return;
    }
    ret = -1;

    if(gettid() == mainThreadID)
        onCallJava->callPrepared(MAINTHREAD);
    else
        onCallJava->callPrepared(CHILDTHREAD);

    isPrepareThreadExit = true;
    pthread_mutex_unlock(&prepareMutex);
}

// 这个方法与prepare()方法互斥
void WlxFFmpeg::stop() {
    int count = 0;

    // 轮训等待prepare线程执行完，如果等待超时，强制退出
    while (1) {
        audio->queue->activeAllThread();
        if (isPrepareThreadExit && isUnpackThreadExit) {
            pthread_mutex_lock(&prepareMutex);
            pthread_mutex_lock(&unparkMutex);

            if(audio != NULL){
                delete(audio);
                audio = NULL;
            }

            if (this->formatContext!=NULL) {
                // 文件一个开发对应一个关闭，一定要加上，不然重复打开会报错
                avformat_close_input(&formatContext);
                avformat_free_context(this->formatContext);
                av_free(formatContext);
                formatContext = NULL;
            }

            pthread_mutex_unlock(&unparkMutex);
            pthread_mutex_unlock(&prepareMutex);
            break;
        }else if(count <20){
            count ++;
            usleep(100*1000);
        }else{
            isPrepareThreadExit = true;
            isUnpackThreadExit = true;
            pthread_exit(&prepareThread);
            pthread_exit(&unparkThread);
        }
    }

//    if(gettid() == mainThreadID)
//        onCallJava->callFinished(MAINTHREAD);
//    else
        onCallJava->callFinished(CHILDTHREAD);
}

void WlxFFmpeg::setSource(const char * url1) {
    this->url = (char *)malloc(strlen(url1)+1);
    strcpy(this->url,url1);
}

void * prepareThreadMethod(void * data){
    WlxFFmpeg * fFmpeg = (WlxFFmpeg *)(data);

    fFmpeg->prepare();

    if(ISDEBUG){
        LOGD("WlxFFmpeg结束了一个线程，进行prepare工作");
    }
    pthread_exit(&fFmpeg->prepareThread);
}
void WlxFFmpeg::asyncPrepare() {
    if(ISDEBUG){
        LOGD("WlxFFmpeg开启了一个线程，进行prepare工作");
    }

    pthread_create(&prepareThread,NULL,prepareThreadMethod,this);
}



void WlxFFmpeg::unpackPacket(){
    pthread_mutex_lock(&unparkMutex);
    isUnpackThreadExit = false;

    int tid = gettid();
    if(tid == mainThreadID){
        if(ISDEBUG){
            LOGE("请将void WlxFFmpeg::unpackPacket(WlxFFmpeg * fFmpeg)方法放在子线程中执行");
        }

        isUnpackThreadExit = true;
        pthread_mutex_unlock(&unparkMutex);
        return;
    }

    while (this->playerState!=NULL && this->playerState->currentPlayState!=EnumCurrentPlayState::EXIT) {
        //region 循环获取源数据包，送入audio缓存队列中
        int ret = -1;
        AVPacket * packet = av_packet_alloc();
        // 停止线程可能会销毁fFmpeg
        ret = av_read_frame(this->formatContext,packet);

        if(ret != 0 || packet == NULL){
            if(ISDEBUG){
                LOGD("读取完毕");
            }
            av_packet_free(&packet);
            av_free(packet);
            packet = NULL;

            break;
        }

        if(packet->stream_index == this->audio->audioIndex){
            this->audio->queue->push(packet);
            packet = NULL;
        }
        //endregion
    }

    isUnpackThreadExit = true;
    pthread_mutex_unlock(&unparkMutex);
}

void * unpackPacketCallback(void * data){
    WlxFFmpeg * fFmpeg = (WlxFFmpeg *)(data);

    fFmpeg->unpackPacket();

    if(ISDEBUG){
        LOGD("WlxFFmpeg结束了一个线程，进行unpack工作");
    }
    pthread_exit(&fFmpeg->unparkThread);
}

void WlxFFmpeg::asyncUnpackPacket(){
    // 开线程进行解包
    if(ISDEBUG){
        LOGD("WlxFFmpeg开启了一个线程，进行unpack工作");
    }
    pthread_create(&unparkThread,NULL,unpackPacketCallback,this);
}

void WlxFFmpeg::play() {
    if(audio == NULL){
        if(ISDEBUG){
            LOGE("缺少音频文件，无法播放");
        }
        return;
    }

//    int count = 0;
    this->asyncUnpackPacket();

    audio->asyncPlay();

//    if(ISDEBUG){
//        LOGD("当前audio队列缓存中存在 %d 个包。",audio->queue->size());
//    } // 上面开了两个线程，这一步必然快于上面两个线程，此时队列缓冲没有数据，所以得到的值永远等于0

}

void WlxFFmpeg::pause() {
    if(audio!=NULL){
        audio->pause();
    }
}

void WlxFFmpeg::continue1() {
    if(audio!=NULL){
        audio->continue1();
    }
}

void WlxFFmpeg::seek(uint64_t seconds) {
    EnumCurrentPlayState lastState = playerState->currentPlayState;
    playerState->currentPlayState = EnumCurrentPlayState::SEEKING;

    if(audio!=NULL){
        // 在audio内部执行
        audio->seek(seconds, this->formatContext,
                    [](uint64_t seconds, AVFormatContext * formatContext1) {
                        int64_t rel = seconds * AV_TIME_BASE;
                        avformat_seek_file(formatContext1, -1, INT64_MIN, rel, INT64_MAX, 0);
                    });
    }

    playerState->currentPlayState = lastState;
}


