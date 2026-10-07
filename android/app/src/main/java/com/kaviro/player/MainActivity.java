package com.kaviro.player;

import android.app.Activity;
import android.os.Bundle;
import android.widget.TextView;

public final class MainActivity extends Activity {
    static {
        System.loadLibrary("kaviro_android");
    }

    private long nativePlayer;
    private TextView statusView;

    private static native long nativeCreate();
    private static native void nativeRelease(long handle);
    private static native boolean nativeOpen(long handle, String path);
    private static native boolean nativePlay(long handle);
    private static native boolean nativePause(long handle);
    private static native boolean nativeSeekMs(long handle, long positionMs);
    private static native boolean nativePump(long handle, int maxFrames);
    private static native String nativeState(long handle);
    private static native String nativeLastError(long handle);
    private static native String nativeEngineStatus();

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        statusView = new TextView(this);
        statusView.setTextSize(18f);
        nativePlayer = nativeCreate();
        statusView.setText(nativeEngineStatus() + "\nSession: " + nativeState(nativePlayer));
        setContentView(statusView);
    }

    @Override
    protected void onDestroy() {
        if (nativePlayer != 0) {
            nativeRelease(nativePlayer);
            nativePlayer = 0;
        }
        super.onDestroy();
    }

    // The UI will call these through the real media-picker/player surface in
    // the next Android playback batch. Keeping the native session persistent
    // here prevents each control action from constructing a fresh decoder.
    private boolean openNativePath(String path) {
        return nativeOpen(nativePlayer, path);
    }

    private boolean playNative() {
        return nativePlay(nativePlayer);
    }

    private boolean pauseNative() {
        return nativePause(nativePlayer);
    }

    private boolean seekNative(long positionMs) {
        return nativeSeekMs(nativePlayer, positionMs);
    }

    private boolean pumpNative(int maxFrames) {
        return nativePump(nativePlayer, maxFrames);
    }

    private String nativeError() {
        return nativeLastError(nativePlayer);
    }
}
