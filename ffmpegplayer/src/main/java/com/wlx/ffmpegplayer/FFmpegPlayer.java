package com.wlx.ffmpegplayer;

import android.media.MediaCodec;
import android.media.MediaCodecInfo;
import android.media.MediaFormat;

import com.wlx.ffmpegplayer.listener.OnFinishedListener;
import com.wlx.ffmpegplayer.listener.OnPreparedListener;
import com.wlx.ffmpegplayer.listener.OnScheduleChangedListener;
import com.wlx.ffmpegplayer.util.MyLog;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.nio.ByteBuffer;

public class FFmpegPlayer {
    static{
        System.loadLibrary("native-lib");
    }

    //region 回调接口区..............................................................................
    private OnPreparedListener onPreparedListener;


    public void setOnPreparedListener(OnPreparedListener onPreparedListener) {
        this.onPreparedListener = onPreparedListener;
    }

    private OnFinishedListener onFinishedListener;
    public void setOnFinishedListener(OnFinishedListener onFinishedListener){
        this.onFinishedListener = onFinishedListener;
    }

    private OnScheduleChangedListener onScheduleChangedListener;
    public void setOnScheduleChangedListener(OnScheduleChangedListener onScheduleChangedListener){
        this.onScheduleChangedListener = onScheduleChangedListener;
    }


    //endregion


    //region 包装了一系列方法 供上层调用...............................................................
    /**
     * 获取版本
     */
    public String getVersion(){
        return n_version();
    }

    /**
     * 打印所有支持的编解码器
     */
    public void logAllCodec(){
        n_logAllCodec();
    }

    /**
     * 初始化播放环境，当播放环境搭建完成，将回调onPreparedListener.OnPrepared()方法
     */
    public void prepare(){
        n_prepare();
    }

    /**
     * 停止播放
     */
    public void stop(){
        n_stop();
    }

    /**
     * 异步停止播放
     */
    public void asyncStop(){
        new Thread(new Runnable() {
            @Override
            public void run() {
                stop();
            }
        }).start();
    }

    /**
     * 设置数据源
     * @param url
     */
    public void setSource(String url){
        n_setSource(url);
    }

    /**
     * 开始播放
     */
    public void play() {
        n_play();
    }

    public void pause() {
        n_pause();
    }

    public void continue1() {
        n_continue();
    }

    public void seek(int progress) {
        n_seek(progress);
    }

    private boolean isMediaCodecInited = false;
    /**
     * 开始录制
     */
    public void startRecord(File file) {
        // 1.如果MediaCodec没有初始化过，就初始化一次
        if(!isMediaCodecInited){
            int sampleRate = 0;
            if((sampleRate=n_getSampleRate())>0){
                initMediaCodec(sampleRate,file);
            }

            isMediaCodecInited = true;
        }
        // 2.打开底层录制开关
        n_setIsRecoding(true);
    }

    /**
     * 结束录制
     */
    public void stopRecord() {
        // 1.关闭底层录制开关
        n_setIsRecoding(false);
        // 2.如果MediaCodec没有初始化过，就初始化一次
        if(isMediaCodecInited){
            // 回收资源
            releaseMediaCodec();

            isMediaCodecInited = false;
        }
    }

    /**
     * 停止录制
     */
    public void pauseRecord(){
        n_setIsRecoding(false);
    }

    /**
     * 继续录制
     */
    public void resumeRecord(){
        n_setIsRecoding(true);
    }

    //endregion

    //region 私有方法
    private MediaFormat mediaFormat;
    private MediaCodec encoder;
    private MediaCodec.BufferInfo info;
    private int aacSampleRateIndex = 0;
    private FileOutputStream fileOutputStream;
    private void initMediaCodec(int sampleRate, File file) {
        try {
            aacSampleRateIndex = getADTSsamplerateIndex(sampleRate);
            mediaFormat = MediaFormat.createAudioFormat(MediaFormat.MIMETYPE_AUDIO_AAC,sampleRate,2);
            // 设置比特率
            mediaFormat.setInteger(MediaFormat.KEY_BIT_RATE,96000);
            // 设置AAC的级别
            mediaFormat.setInteger(MediaFormat.KEY_AAC_PROFILE, MediaCodecInfo.CodecProfileLevel.AACObjectERLC);
            // 设置最大输入大小
            mediaFormat.setInteger(MediaFormat.KEY_MAX_INPUT_SIZE,5120);
            // 创建AAC编解码器
            encoder = MediaCodec.createEncoderByType(MediaFormat.MIMETYPE_AUDIO_AAC);
            if(encoder == null){
                MyLog.e("com.wlx.ffmpegplayer.FFmpegPlayer.initMediaCodec：AAC编码器创建失败!");
                return;
            }
            info = new MediaCodec.BufferInfo();
            // 配置encoder
            encoder.configure(mediaFormat,null,null,MediaCodec.CONFIGURE_FLAG_ENCODE);
            // 创建文件输出流
            fileOutputStream = new FileOutputStream(file);
            encoder.start();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    private void releaseMediaCodec(){
        try {
            encoder.stop();
            fileOutputStream.close();
            fileOutputStream = null;
            encoder.release();
            encoder = null;
            mediaFormat = null;

        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    /**
     * 每个aac packet实际大小
     */
    private int perpcmsize = 0;
    /**
     * 输出字节数据
     */
    private byte[] outByteBuffer;

    /**
     * 将pcm原始数据编码成aac frame
     * @param size 原始数据大小
     * @param buffer 原始数据
     */
    private void encodecPcmToAAc(int size, byte[] buffer)
    {
        if(buffer != null && encoder != null)
        {
            // 获取输入缓冲区索引
            int inputBufferindex = encoder.dequeueInputBuffer(0);
            if(inputBufferindex >= 0)
            {
                // 获取输入缓冲区
                ByteBuffer byteBuffer = encoder.getInputBuffers()[inputBufferindex];
                // 输入缓冲区中放入待转码数据
                byteBuffer.clear();
                byteBuffer.put(buffer);
                // 通知encoder，0号缓冲区中有待转码数据，长度为size,size大小会影响输出缓冲区大小
                encoder.queueInputBuffer(inputBufferindex, 0, size, 0, 0);
            }

            // 获取输出缓冲区索引，同时输出缓冲区的信息会写入info中
            int index = encoder.dequeueOutputBuffer(info, 0);
            while(index >= 0)
            {
                try {
                    perpcmsize = info.size + 7;
                    outByteBuffer = new byte[perpcmsize];
                    // 获取输出缓冲区
                    ByteBuffer byteBuffer = encoder.getOutputBuffers()[index];
                    // 将byteBuffer的开始写入位置定位到info.offset的位置
                    byteBuffer.position(info.offset);
                    // 将byteBuffer的结束写入位置定位到info.offset+info.size的位置
                    byteBuffer.limit(info.offset + info.size);
                    // 写入ADTS头部信息
                    addADtsHeader(outByteBuffer, perpcmsize, aacSampleRateIndex);
                    // 将byteBuffer的数据写入到outByteBuffer中
                    byteBuffer.get(outByteBuffer, 7, info.size);
                    // 将byteBuffer的开始写入位置重新定位到info.offset的位置
                    byteBuffer.position(info.offset);
                    // 将outByteBuffer数据写入到文件中
                    fileOutputStream.write(outByteBuffer, 0, perpcmsize);

                    encoder.releaseOutputBuffer(index, false);
                    index = encoder.dequeueOutputBuffer(info, 0);
                    outByteBuffer = null;
                } catch (IOException e) {
                    e.printStackTrace();
                }
            }
        }
    }

    /**
     * 写入ADTS头部信息
     * @param packet 待输出字节数据
     * @param packetLen 每个aac packet实际大小
     * @param aacSampleRateIndex
     */
    private void addADtsHeader(byte[] packet, int packetLen, int aacSampleRateIndex) {
        int profile = 2; // AAC LC
        int freqIdx = aacSampleRateIndex; // samplerate
        int chanCfg = 2; // CPE

        packet[0] = (byte) 0xFF; // 0xFFF(12bit) 这里只取了8位，所以还差4位放到下一个里面
        packet[1] = (byte) 0xF9; // 第一个t位放F
        packet[2] = (byte) (((profile - 1) << 6) + (freqIdx << 2) + (chanCfg >> 2));
        packet[3] = (byte) (((chanCfg & 3) << 6) + (packetLen >> 11));
        packet[4] = (byte) ((packetLen & 0x7FF) >> 3);
        packet[5] = (byte) (((packetLen & 7) << 5) + 0x1F);
        packet[6] = (byte) 0xFC;
    }

    /**
     * 获取采样速率在MediaCodec中对应的序号(sampling_frequency_index)
     */
    private int getADTSsamplerateIndex(int sampleRate) {
        int rate = 4;
        switch (sampleRate)
        {
            case 96000:
                rate = 0;
                break;
            case 88200:
                rate = 1;
                break;
            case 64000:
                rate = 2;
                break;
            case 48000:
                rate = 3;
                break;
            case 44100:
                rate = 4;
                break;
            case 32000:
                rate = 5;
                break;
            case 24000:
                rate = 6;
                break;
            case 22050:
                rate = 7;
                break;
            case 16000:
                rate = 8;
                break;
            case 12000:
                rate = 9;
                break;
            case 11025:
                rate = 10;
                break;
            case 8000:
                rate = 11;
                break;
            case 7350:
                rate = 12;
                break;
        }
        return rate;
    }
    //endregion

    //region c 调用 java 方法........................................................................
    public void onCallPrepared(){
        if(onPreparedListener != null){
            onPreparedListener.OnPrepared();
        }
    }

    public void onCallFinished(){
        if(onFinishedListener!=null){
            onFinishedListener.onFinished();
        }
    }

    public void onCallSchedule(int currentTime,int duration){
        if(onScheduleChangedListener!=null){
            onScheduleChangedListener.OnScheduleChanged(currentTime,duration);
        }
    }

    public void onCallPcmToAAc(int size, byte[] buffer){
        encodecPcmToAAc(size,buffer);
    }
    //endregion

    //region java 调用 c 本地方法....................................................................
    private native String n_version();
    private native void n_logAllCodec();
    private native void n_setSource(String url);
    private native void n_prepare();
    private native void n_stop();
    private native void n_play();
    private native void n_pause();
    private native void n_continue();
    private native void n_seek(int progress);
    private native int n_getSampleRate();
    private native void n_setIsRecoding(boolean isRecoding);



    //endregion
}
