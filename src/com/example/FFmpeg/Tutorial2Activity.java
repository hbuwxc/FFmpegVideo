package com.example.FFmpeg;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.KeyguardManager;
import android.graphics.Point;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.*;
import android.widget.Button;
import android.widget.RelativeLayout;

import java.io.File;
import java.io.IOException;

/**
 * Created by wxc on 15/7/24.
 */
public class Tutorial2Activity extends Activity implements SurfaceHolder.Callback {
    private static final String TAG = "android-ffmpeg";
    private static final String FRAME_DUMP_FOLDER_PATH = Environment.getExternalStorageDirectory()
            + File.separator + "android-ffmpeg-tutorial02";

    // video used to fill the width of the screen
    private static final String videoFileName = "test2.mov";  	//640x360
    private static final String videoFileName2 = "1_cut.yuv";  	//640x360
    private static final String videoFileName3 = "1.m4v";  	//640x360
    // video used to fill the height of the screen
//	private static final String videoFileName = "12.mp4";   //200x640

    private SurfaceView mSurfaceView;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);
        setContentView(R.layout.activity_tutorial2);
        int flags = WindowManager.LayoutParams.FLAG_SHOW_WHEN_LOCKED;
        getWindow().addFlags(flags);



        KeyguardManager keyguardManager = (KeyguardManager)getSystemService(KEYGUARD_SERVICE);
        KeyguardManager.KeyguardLock keyguardLock = keyguardManager.newKeyguardLock("");
        keyguardLock.disableKeyguard();
        //create directory for the tutorial
        File dumpFolder = new File(FRAME_DUMP_FOLDER_PATH);
        if (!dumpFolder.exists()) {
            dumpFolder.mkdirs();
        }

        Utils.copyAssets(this,"test.png",FRAME_DUMP_FOLDER_PATH);
        //copy input video file from assets folder to directory
        Utils.copyAssets(this, videoFileName, FRAME_DUMP_FOLDER_PATH);

        File f = new File(FRAME_DUMP_FOLDER_PATH + File.separator + videoFileName2);
        if(f.exists()){
            f.delete();
        }
        try {
            f.createNewFile();
        } catch (IOException e) {
            e.printStackTrace();
        }

        File f2 = new File(FRAME_DUMP_FOLDER_PATH + File.separator + videoFileName3);
        if(f2.exists()){
            f2.delete();
        }
        try {
            f2.createNewFile();
        } catch (IOException e) {
            e.printStackTrace();
        }

//        new Thread(new Runnable() {
//            @Override
//            public void run() {
                naCutVideo(FRAME_DUMP_FOLDER_PATH + File.separator + videoFileName, FRAME_DUMP_FOLDER_PATH + File.separator + videoFileName2,FRAME_DUMP_FOLDER_PATH+ File.separator+videoFileName3, 0, 5);
//            }
//        }).start();

        mSurfaceView = (SurfaceView)findViewById(R.id.surfaceview);
        mSurfaceView.getHolder().addCallback(this);

        Button btnStart = (Button) this.findViewById(R.id.buttonStart);
        btnStart.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
//                new Thread(new Runnable() {
//                    @Override
//                    public void run() {
//                        naCutVideo(FRAME_DUMP_FOLDER_PATH + File.separator + videoFileName, FRAME_DUMP_FOLDER_PATH + File.separator + videoFileName2, 0, 5);
//                    }
//                }).start();
                naInit(FRAME_DUMP_FOLDER_PATH + File.separator + videoFileName);
                int[] res = naGetVideoRes();
                Log.d(TAG, "res width " + res[0] + ": height " + res[1]);
                int[] screenRes = getScreenRes();
                int width, height;
                float widthScaledRatio = screenRes[0]*1.0f/res[0];
                float heightScaledRatio = screenRes[1]*1.0f/res[1];
                if (widthScaledRatio > heightScaledRatio) {
                    //use heightScaledRatio
                    width = (int) (res[0]*heightScaledRatio);
                    height = screenRes[1];
                } else {
                    //use widthScaledRatio
                    width = screenRes[0];
                    height = (int) (res[1]*widthScaledRatio);
                }
                Log.d(TAG, "width " + width + ",height:" + height);
                updateSurfaceView(width, height);
                naSetup(width, height);
                naPlay();
            }
        });
    }

    private void updateSurfaceView(int pWidth, int pHeight) {
        //update surfaceview dimension, this will cause the native window to change
        RelativeLayout.LayoutParams params = (RelativeLayout.LayoutParams) mSurfaceView.getLayoutParams();
        params.width = pWidth;
        params.height = pHeight;
        mSurfaceView.setLayoutParams(params);
    }

    @SuppressLint("NewApi")
    private int[] getScreenRes() {
        int[] res = new int[2];
        Display display = getWindowManager().getDefaultDisplay();
        if (Build.VERSION.SDK_INT >= 13) {
            Point size = new Point();
            display.getSize(size);
            res[0] = size.x;
            res[1] = size.y;
        } else {
            res[0] = display.getWidth();  // deprecated
            res[1] = display.getHeight();  // deprecated
        }
        return res;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
        naStop();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width,
                               int height) {
        Log.d(TAG, "surfacechanged: " + width + ":" + height);
        naSetSurface(holder.getSurface());
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        Log.d(TAG, "surfaceDestroyed");
        naSetSurface(null);
    }

    private static native int naCutVideo(String pFileName,String oFileName,String mp4FileName,int startTime,int length);
    private static native int naInit(String pFileName);
    private static native int[] naGetVideoRes();
    private static native void naSetSurface(Surface pSurface);
    private static native int naSetup(int pWidth, int pHeight);
    private static native void naPlay();
    private static native void naStop();

    static {
        System.loadLibrary("avutil-54");
        System.loadLibrary("swresample-1");
        System.loadLibrary("avcodec-56");
        System.loadLibrary("avformat-56");
        System.loadLibrary("swscale-3");
        System.loadLibrary("tutorial02");
    }
}
