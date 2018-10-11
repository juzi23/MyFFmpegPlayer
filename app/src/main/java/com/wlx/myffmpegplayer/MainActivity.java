package com.wlx.myffmpegplayer;

import android.app.ActivityManager;
import android.os.Environment;
import android.support.annotation.NonNull;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.view.View;
import android.widget.Button;
import android.widget.SeekBar;
import android.widget.TextView;

import com.wlx.ffmpegplayer.FFmpegPlayer;
import com.wlx.ffmpegplayer.listener.OnFinishedListener;
import com.wlx.ffmpegplayer.listener.OnPreparedListener;
import com.wlx.ffmpegplayer.listener.OnScheduleChangedListener;
import com.wlx.ffmpegplayer.util.MyLog;
import com.wlx.ffmpegplayer.util.TimeUtil;

import java.io.File;

public class MainActivity extends AppCompatActivity implements View.OnClickListener, SeekBar.OnSeekBarChangeListener {
    private ActivityManager activityManager;

    private Button btn_start;
    private Button btn_stop;
    private Button btn_pause;
    private Button btn_continue;
    private Button btn_startRecord;
    private Button btn_stopRecord;
    private Button btn_pauseRecord;
    private Button btn_resumeRecord;

    private SeekBar sb_schedule;
    private TextView progressTextView;

    private FFmpegPlayer fFmpegPlayer;
    private String url;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        initManager();

        initView();

        initFFmpegPlayer();

        initViewListener();
    }

    private void initManager() {
        this.activityManager = (ActivityManager) getSystemService(ACTIVITY_SERVICE);
    }


    private void initView() {
        setContentView(R.layout.activity_main);
        btn_start = findViewById(R.id.btn_start);
        btn_stop = findViewById(R.id.btn_stop);
        btn_pause = findViewById(R.id.btn_pause);
        btn_continue = findViewById(R.id.btn_continue);
        sb_schedule = findViewById(R.id.sb_schedule);
        progressTextView = findViewById(R.id.tv_progress);
        btn_startRecord = findViewById(R.id.btn_startRecord);
        btn_stopRecord = findViewById(R.id.btn_stopRecord);
        btn_pauseRecord = findViewById(R.id.btn_pauseRecord);
        btn_resumeRecord = findViewById(R.id.btn_resumeRecord);
    }

    @NonNull
    private void initFFmpegPlayer() {
        fFmpegPlayer = new FFmpegPlayer();
        url = "http://mpge.5nd.com/2015/2015-11-26/69708/1.mp3";
        fFmpegPlayer.setOnPreparedListener(new OnPreparedListener() {
            @Override
            public void OnPrepared() {
                logFreeMemory();
                //MyLog.d("准备完成，可以播放！");
                fFmpegPlayer.play();
            }
        });
        fFmpegPlayer.setOnFinishedListener(new OnFinishedListener() {
            @Override
            public void onFinished() {
                logFreeMemory();
            }
        });
        // 设置进度条回调监听
        fFmpegPlayer.setOnScheduleChangedListener(new OnScheduleChangedListener() {
            @Override
            public void OnScheduleChanged(int currentTime, int duration) {
                sb_schedule.setMax(duration);
                sb_schedule.setProgress(currentTime);
            }
        });
    }


    private void initViewListener() {
        btn_start.setOnClickListener(this);
        btn_stop.setOnClickListener(this);
        btn_continue.setOnClickListener(this);
        btn_pause.setOnClickListener(this);
        sb_schedule.setOnSeekBarChangeListener(this);
        btn_startRecord.setOnClickListener(this);
        btn_stopRecord.setOnClickListener(this);
        btn_pauseRecord.setOnClickListener(this);
        btn_resumeRecord.setOnClickListener(this);
    }

    /**
     * 所有按钮点击事件监听
     * @param v
     */
    @Override
    public void onClick(View v) {
        switch (v.getId()){
            case R.id.btn_start:
                fFmpegPlayer.setSource(url);
                fFmpegPlayer.prepare();
                break;
            case R.id.btn_stop:
                fFmpegPlayer.asyncStop();
                break;
            case R.id.btn_pause:
                fFmpegPlayer.pause();
                break;
            case R.id.btn_continue:
                fFmpegPlayer.continue1();
                break;
            case R.id.btn_startRecord:
                fFmpegPlayer.startRecord(
                        new File(Environment.
                                getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS).
                                getAbsolutePath()+ File.separator+"temp.aac")
                        );
                break;
            case R.id.btn_stopRecord:
                fFmpegPlayer.stopRecord();
                break;
            case R.id.btn_pauseRecord:
                fFmpegPlayer.pauseRecord();
                break;
            case R.id.btn_resumeRecord:
                fFmpegPlayer.resumeRecord();
                break;
        }
    }

    public void logFreeMemory(){
        float freeMemory = (float) (Runtime.getRuntime().freeMemory()*1.0/(1024*1024));
        MyLog.d("当前剩余内存为"+freeMemory);
    }

    //region 进度条回调监听
    @Override
    public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
        // 将进度以文字信息输出到progressTextView
        progressTextView.setText(TimeUtil.secdsToDateFormat(progress,seekBar.getMax())+
                "/"+
                TimeUtil.secdsToDateFormat(seekBar.getMax(),seekBar.getMax()));
        if(progress>0 && progress == seekBar.getMax()){
            fFmpegPlayer.stop();
        }
    }

    @Override
    public void onStartTrackingTouch(SeekBar seekBar) {

    }

    @Override
    public void onStopTrackingTouch(SeekBar seekBar) {
        fFmpegPlayer.seek(seekBar.getProgress());
    }
    //endregion
}
