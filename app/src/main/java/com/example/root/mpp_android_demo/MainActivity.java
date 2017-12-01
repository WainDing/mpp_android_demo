package com.example.root.mpp_android_demo;

import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.widget.TextView;

import java.util.Objects;

public class MainActivity extends AppCompatActivity implements SurfaceHolder.Callback {
    SurfaceHolder surfaceholder;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        SurfaceView view = (SurfaceView) findViewById(R.id.sample_view);
        surfaceholder = view.getHolder();
        surfaceholder.addCallback(this);
    }

    @Override
    public void surfaceCreated(final SurfaceHolder holder) {
        new Thread(new Runnable() {
            @Override
            public void run() {
                int ret = MppDecoder.MppDecoderRegister(surfaceholder.getSurface());
                Log.d("sliver", "ret val:"+ret);
            }
        }).start();
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {

    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {

    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();
}
