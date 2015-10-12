package com.example.FFmpeg;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Log;
import android.webkit.WebView;
import android.widget.Toast;

public class MyActivity extends Activity {
    /**
     * Called when the activity is first created.
     */
    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.main);

//        startActivity(new Intent(this,MainActivity.class));  ///tutorial1
        gotoActivity(Tutorial2Activity.class);

        finish();

    }

    private void gotoActivity(Class activity){
        Intent intent = new Intent(this,activity);
        startActivity(intent);
    }
}
