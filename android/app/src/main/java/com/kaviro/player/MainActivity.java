package com.kaviro.player;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.os.ParcelFileDescriptor;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.SeekBar;
import android.widget.TextView;
import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

public final class MainActivity extends Activity implements SurfaceHolder.Callback {
    static {
        System.loadLibrary("kaviro_android");
    }

    private long nativePlayer;
    private TextView statusView;
    private SurfaceView surfaceView;
    private HandlerThread playbackThread;
    private Handler playbackHandler;
    private boolean playing;
    private SeekBar seekBar;
    private boolean userSeeking;
    private final Runnable timelineTask = new Runnable() {
        @Override public void run() {
            if (nativePlayer == 0) return;
            if (!userSeeking) updateTimeline();
            playbackHandler.postDelayed(this, 250);
        }
    };

    private static native long nativeCreate();
    private static native void nativeRelease(long handle);
    private static native boolean nativeOpen(long handle, String path);
    private static native boolean nativeOpenFd(long handle, int fd);
    private static native boolean nativeSetSurface(long handle, Surface surface);
    private static native boolean nativePlay(long handle);
    private static native boolean nativePause(long handle);
    private static native boolean nativeStop(long handle);
    private static native boolean nativePump(long handle, int maxFrames);
    private static native boolean nativeSeekMs(long handle, long positionMs);
    private static native long nativePositionMs(long handle);
    private static native long nativeDurationMs(long handle);
    private static native String nativeState(long handle);
    private static native String nativeLastError(long handle);
    private static native String nativeEngineStatus();

    private final Runnable pumpTask = new Runnable() {
        @Override public void run() {
            if (!playing || nativePlayer == 0) return;
            final boolean ok = nativePump(nativePlayer, 4);
            if (!ok) {
                final String state = nativeState(nativePlayer);
                playing = false;
                if ("error".equals(state)) {
                    runOnUiThread(() -> statusView.setText("Playback error: " + nativeLastError(nativePlayer)));
                } else {
                    runOnUiThread(() -> statusView.setText("Playback finished\nSession: " + state));
                }
                return;
            }
            playbackHandler.postDelayed(this, 8);
        }
    };

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        nativePlayer = nativeCreate();
        playbackThread = new HandlerThread("KAVIRO-playback");
        playbackThread.start();
        playbackHandler = new Handler(playbackThread.getLooper());

        final LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);

        surfaceView = new SurfaceView(this);
        surfaceView.getHolder().addCallback(this);
        root.addView(surfaceView, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));

        final LinearLayout controls = new LinearLayout(this);
        controls.setOrientation(LinearLayout.HORIZONTAL);

        final Button open = new Button(this);
        open.setText("Open");
        open.setOnClickListener(v -> chooseMedia());

        final Button playPause = new Button(this);
        playPause.setText("Play / Pause");
        playPause.setOnClickListener(v -> togglePlayback());

        final Button stop = new Button(this);
        stop.setText("Stop");
        stop.setOnClickListener(v -> stopPlayback());

        seekBar = new SeekBar(this);
        seekBar.setMax(1);
        seekBar.setEnabled(false);
        seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override public void onProgressChanged(SeekBar bar, int progress, boolean fromUser) {}

            @Override public void onStartTrackingTouch(SeekBar bar) {
                userSeeking = true;
            }

            @Override public void onStopTrackingTouch(SeekBar bar) {
                userSeeking = false;
                final long duration = nativeDurationMs(nativePlayer);
                if (duration <= 0) {
                    updateTimeline();
                    return;
                }
                final int maxProgress = Math.max(1, bar.getMax());
                final long target = duration <= Integer.MAX_VALUE
                        ? bar.getProgress()
                        : (long) ((double) duration * bar.getProgress() / maxProgress);
                final long clamped = Math.max(0, Math.min(duration, target));
                final boolean ok = nativeSeekMs(nativePlayer, clamped);
                if (!ok) {
                    statusView.setText("Seek failed: " + nativeLastError(nativePlayer));
                } else {
                    updateTimeline();
                    statusView.setText("Position: " + formatMs(clamped) +
                            " / " + formatMs(duration) + "\nSession: " + nativeState(nativePlayer));
                }
            }
        });
        root.addView(seekBar, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        controls.addView(open, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        controls.addView(playPause, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        controls.addView(stop, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        root.addView(controls);

        statusView = new TextView(this);
        statusView.setText(nativeEngineStatus() + "\nSession: " + nativeState(nativePlayer));
        statusView.setPadding(16, 8, 16, 16);
        root.addView(statusView);

        setContentView(root);
        playbackHandler.post(timelineTask);
    }

    private void updateTimeline() {
        if (nativePlayer == 0 || seekBar == null) return;
        final long duration = nativeDurationMs(nativePlayer);
        final long position = nativePositionMs(nativePlayer);
        runOnUiThread(() -> {
            if (seekBar == null || userSeeking) return;
            if (duration <= 0) {
                seekBar.setEnabled(false);
                seekBar.setMax(1);
                seekBar.setProgress(0);
                return;
            }
            seekBar.setEnabled(true);
            final int max = (int) Math.min(Integer.MAX_VALUE, duration);
            seekBar.setMax(Math.max(1, max));
            final int progress = (int) Math.min(seekBar.getMax(), Math.max(0, position));
            seekBar.setProgress(progress);
        });
    }

    private static String formatMs(long ms) {
        long totalSeconds = Math.max(0, ms) / 1000;
        long hours = totalSeconds / 3600;
        long minutes = (totalSeconds % 3600) / 60;
        long seconds = totalSeconds % 60;
        return hours > 0
                ? String.format(java.util.Locale.US, "%d:%02d:%02d", hours, minutes, seconds)
                : String.format(java.util.Locale.US, "%d:%02d", minutes, seconds);
    }

    private void chooseMedia() {
        final Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType("*/*");
        startActivityForResult(intent, 1001);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if (requestCode != 1001 || resultCode != RESULT_OK || data == null || data.getData() == null) {
            return;
        }

        final Uri uri = data.getData();
        playbackHandler.post(() -> openUri(uri));
    }

    private void openUri(Uri uri) {
        try (ParcelFileDescriptor pfd = getContentResolver().openFileDescriptor(uri, "r")) {
            if (pfd != null && pfd.getFd() >= 0) {
                final boolean opened = nativeOpenFd(nativePlayer, pfd.getFd());
                if (opened) {
                    runOnUiThread(() -> statusView.setText(
                            "Opened selected media\nSession: " + nativeState(nativePlayer)));
                    runOnUiThread(this::updateTimeline);
                    return;
                }
            }

            // Some document providers expose a non-reopenable/virtual descriptor.
            // Fall back to a private cache copy only when direct descriptor access fails.
            final File localFile = copyToCache(uri);
            final boolean opened = nativeOpen(nativePlayer, localFile.getAbsolutePath());
            runOnUiThread(() -> statusView.setText(
                    opened ? "Opened: " + localFile.getName() + "\nSession: " + nativeState(nativePlayer)
                           : "Open failed: " + nativeLastError(nativePlayer)));
            runOnUiThread(this::updateTimeline);
        } catch (Exception e) {
            runOnUiThread(() -> statusView.setText("Open failed: " + e.getMessage()));
        }
    }

    private File copyToCache(Uri uri) throws Exception {
        String name = "kaviro-" + System.currentTimeMillis() + ".media";
        File target = new File(getCacheDir(), name);
        try (InputStream input = getContentResolver().openInputStream(uri);
             FileOutputStream output = new FileOutputStream(target)) {
            if (input == null) throw new IllegalStateException("Unable to read selected media");
            byte[] buffer = new byte[1024 * 1024];
            int read;
            while ((read = input.read(buffer)) != -1) {
                output.write(buffer, 0, read);
            }
        }
        return target;
    }

    private void stopPlayback() {
        if (nativePlayer == 0) return;
        playing = false;
        playbackHandler.removeCallbacks(pumpTask);
        final boolean ok = nativeStop(nativePlayer);
        statusView.setText(ok
                ? "Stopped\nPosition: 0:00\nSession: " + nativeState(nativePlayer)
                : "Stop failed: " + nativeLastError(nativePlayer));
        updateTimeline();
    }

    private void togglePlayback() {
        if (nativePlayer == 0) return;
        if (playing) {
            playing = false;
            playbackHandler.removeCallbacks(pumpTask);
            nativePause(nativePlayer);
        } else {
            if (nativePlay(nativePlayer)) {
                playing = true;
                playbackHandler.post(pumpTask);
            }
        }
        statusView.setText("Session: " + nativeState(nativePlayer));
    }

    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        if (nativePlayer != 0) nativeSetSurface(nativePlayer, holder.getSurface());
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        if (nativePlayer != 0) nativeSetSurface(nativePlayer, holder.getSurface());
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        if (nativePlayer != 0) nativeSetSurface(nativePlayer, null);
    }

    @Override
    protected void onDestroy() {
        playing = false;
        if (playbackHandler != null) playbackHandler.removeCallbacksAndMessages(null);
        if (playbackThread != null) {
            playbackThread.quitSafely();
            try { playbackThread.join(2000); } catch (InterruptedException ignored) { Thread.currentThread().interrupt(); }
        }
        if (nativePlayer != 0) {
            nativeRelease(nativePlayer);
            nativePlayer = 0;
        }
        super.onDestroy();
    }
}
