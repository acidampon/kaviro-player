package com.kaviro.player;

import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;

public final class MainActivity extends Activity {
    static {
        System.loadLibrary("kaviro_android");
    }

    private static native String nativeEngineStatus();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        TextView view = new TextView(this);
        view.setText(nativeEngineStatus());
        view.setTextSize(18f);
        setContentView(view);
    }
}
