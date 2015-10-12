package com.example.FFmpeg;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.graphics.Point;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.view.*;
import android.widget.Button;
import android.widget.RelativeLayout;

import java.io.File;

/**
 * Created by wxc on 15/7/29.
 */
public class Tutorial3Activity extends Activity{
    private static final String TAG = "android-ffmpeg";
    private static final String FRAME_DUMP_FOLDER_PATH = Environment.getExternalStorageDirectory()
            + File.separator + "android-ffmpeg-tutorial03";

    // video used to fill the width of the screen
    private static final String videoFileName = "1.mp4";  	//640x360
    // video used to fill the height of the screen
//	private static final String videoFileName = "12.mp4";   //200x640


    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);
        setContentView(R.layout.activity_tutorial3);
        //create directory for the tutorial
        File dumpFolder = new File(FRAME_DUMP_FOLDER_PATH);
        if (!dumpFolder.exists()) {
            dumpFolder.mkdirs();
        }
        //copy input video file from assets folder to directory
        Utils.copyAssets(this, videoFileName, FRAME_DUMP_FOLDER_PATH);

        Button btnStart = (Button) this.findViewById(R.id.buttonStart);
        btnStart.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                String path = FRAME_DUMP_FOLDER_PATH + File.separator + "1.mp4";
                nativeInit(path);
//                naInit(FRAME_DUMP_FOLDER_PATH + File.separator + videoFileName);
//                int[] res = naGetVideoRes();
//                Log.d(TAG, "res width " + res[0] + ": height " + res[1]);
//                int[] screenRes = getScreenRes();
//                int width, height;
//                float widthScaledRatio = screenRes[0]*1.0f/res[0];
//                float heightScaledRatio = screenRes[1]*1.0f/res[1];
//                if (widthScaledRatio > heightScaledRatio) {
//                    //use heightScaledRatio
//                    width = (int) (res[0]*heightScaledRatio);
//                    height = screenRes[1];
//                } else {
//                    //use widthScaledRatio
//                    width = screenRes[0];
//                    height = (int) (res[1]*widthScaledRatio);
//                }
//                Log.d(TAG, "width " + width + ",height:" + height);
//                updateSurfaceView(width, height);
//                naSetup(width, height);
//                naPlay();
            }
        });
    }

    private static native int nativeInit(String pFileName);

    // Load the .so
    static {
        System.loadLibrary("SDL");
        //System.loadLibrary("SDL_image");
        //System.loadLibrary("SDL_mixer");
        //System.loadLibrary("SDL_ttf");
        System.loadLibrary("main");
    }
}
