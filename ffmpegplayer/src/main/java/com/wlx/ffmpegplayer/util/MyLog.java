package com.wlx.ffmpegplayer.util;

import android.util.Log;

public class MyLog {
    public static boolean isDebug = true;
    public static String TAG = "System.out";

    public static void d(String msg){
        if (isDebug) {
            Log.d(TAG,msg);
        }
    }

    public static void v(String msg){
        if (isDebug) {
            Log.v(TAG,msg);
        }
    }

    public static void e(String msg){
        if (isDebug) {
            Log.e(TAG,msg);
        }
    }
}
