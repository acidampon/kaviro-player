package com.kaviro.player;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.database.Cursor;
import android.media.AudioAttributes;
import android.media.AudioFocusRequest;
import android.media.AudioManager;
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
import java.util.ArrayList;
import org.json.JSONArray;
import org.json.JSONObject;
import java.io.FileOutputStream;
import java.io.InputStream;

public final class MainActivity extends Activity implements SurfaceHolder.Callback {
    static {
        System.loadLibrary("kaviro_android");
    }

    private long nativePlayer;
    private TextView statusView;
    private TextView queueSummaryView;
    private Button playPauseButton;
    private SurfaceView surfaceView;
    private HandlerThread playbackThread;
    private Handler playbackHandler;
    private boolean playing;
    private SeekBar seekBar;
    private boolean userSeeking;
    private AudioManager audioManager;
    private AudioFocusRequest audioFocusRequest;
    private boolean hasAudioFocus;
    private boolean resumeAfterFocusLoss;
    private boolean resumeAfterLifecycle;
    private SharedPreferences libraryPrefs;
    private String currentUri;
    private String currentName;
    private long lastPositionPersistMs;
    private static final int MAX_RECENT_ITEMS = 20;
    private static final int MAX_QUEUE_ITEMS = 50;
    private static final String PREFS_NAME = "kaviro_library_v1";
    private static final String RECENTS_KEY = "recent_items";
    private static final String QUEUE_KEY = "queue_items";
    private static final int REQUEST_OPEN_MEDIA = 1001;
    private static final int REQUEST_QUEUE_MEDIA = 1002;
    private final Runnable timelineTask = new Runnable() {
        @Override public void run() {
            if (nativePlayer == 0) return;
            if (!userSeeking) updateTimeline();
            updatePlaybackControls();
            if (currentUri != null && System.currentTimeMillis() - lastPositionPersistMs >= 2000) {
                persistCurrentPosition();
            }
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
    private static native String[] nativeAudioTracks(long handle);
    private static native String[] nativeVideoTracks(long handle);
    private static native boolean nativeSelectAudioTrack(long handle, int streamIndex);
    private static native boolean nativeSelectVideoTrack(long handle, int streamIndex);

    private final Runnable pumpTask = new Runnable() {
        @Override public void run() {
            if (!playing || nativePlayer == 0) return;
            final boolean ok = nativePump(nativePlayer, 4);
            if (!ok) {
                final String state = nativeState(nativePlayer);
                playing = false;
                if ("error".equals(state)) {
                    abandonAudioFocus();
                    runOnUiThread(() -> statusView.setText("Playback error: " + nativeLastError(nativePlayer)));
                    updatePlaybackControls();
                } else if ("ended".equals(state)) {
                    playbackHandler.post(() -> advanceQueueAfterEnd());
                } else {
                    runOnUiThread(() -> statusView.setText("Playback stopped\nSession: " + state));
                    updatePlaybackControls();
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
        audioManager = (AudioManager) getSystemService(Context.AUDIO_SERVICE);
        libraryPrefs = getSharedPreferences(PREFS_NAME, MODE_PRIVATE);
        audioFocusRequest = new AudioFocusRequest.Builder(AudioManager.AUDIOFOCUS_GAIN)
                .setAudioAttributes(new AudioAttributes.Builder()
                        .setUsage(AudioAttributes.USAGE_MEDIA)
                        .setContentType(AudioAttributes.CONTENT_TYPE_MOVIE)
                        .build())
                .setOnAudioFocusChangeListener(this::onAudioFocusChange)
                .build();
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

        playPauseButton = new Button(this);
        playPauseButton.setText("Play");
        playPauseButton.setOnClickListener(v -> togglePlayback());

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

        final Button audio = new Button(this);
        audio.setText("Audio");
        audio.setOnClickListener(v -> chooseTrack(true));

        final Button video = new Button(this);
        video.setText("Video");
        video.setOnClickListener(v -> chooseTrack(false));

        controls.addView(open, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        controls.addView(playPauseButton, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        controls.addView(stop, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        controls.addView(audio, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        controls.addView(video, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        root.addView(controls);

        final LinearLayout libraryControls = new LinearLayout(this);
        libraryControls.setOrientation(LinearLayout.HORIZONTAL);

        final Button library = new Button(this);
        library.setText("Library");
        library.setOnClickListener(v -> showLibrary());

        final Button queue = new Button(this);
        queue.setText("Queue");
        queue.setOnClickListener(v -> showQueue());

        final Button addQueue = new Button(this);
        addQueue.setText("Add Queue");
        addQueue.setOnClickListener(v -> {
            if (currentUri == null) {
                statusView.setText("Open media before adding it to the queue");
            } else if (addQueueItem(currentUri, currentName, nativeDurationMs(nativePlayer))) {
                statusView.setText("Added to queue: " + currentName);
            } else {
                statusView.setText("Already in queue: " + currentName);
            }
        });

        libraryControls.addView(library, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        libraryControls.addView(queue, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        libraryControls.addView(addQueue, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        root.addView(libraryControls);

        statusView = new TextView(this);
        statusView.setText(nativeEngineStatus() + "\nSession: " + nativeState(nativePlayer));
        statusView.setPadding(16, 8, 16, 16);
        root.addView(statusView);

        queueSummaryView = new TextView(this);
        queueSummaryView.setPadding(16, 4, 16, 12);
        root.addView(queueSummaryView);
        updateQueueSummary();
        updatePlaybackControls();

        setContentView(root);
        playbackHandler.post(timelineTask);
    }


    private void refreshPlaybackUi() {
        updatePlaybackControls();
        updateTimeline();
    }

    private void updatePlaybackControls() {
        if (playPauseButton == null || nativePlayer == 0) return;
        final String state = nativeState(nativePlayer);
        final String label;
        if ("playing".equals(state) && playing) {
            label = "Pause";
        } else if ("ended".equals(state)) {
            label = "Replay";
        } else {
            label = "Play";
        }
        runOnUiThread(() -> {
            if (playPauseButton != null) playPauseButton.setText(label);
        });
    }

    private void updateQueueSummary() {
        if (queueSummaryView == null) return;
        final JSONArray items = loadItems(QUEUE_KEY);
        final StringBuilder text = new StringBuilder("Queue: ");
        if (items.length() == 0) {
            text.append("empty");
        } else {
            text.append(items.length()).append(" upcoming");
            final JSONObject next = items.optJSONObject(0);
            if (next != null) {
                text.append("\nNext: ").append(next.optString("name", "Unknown media"));
            }
            if (items.length() > 1) {
                text.append("\nThen: ").append(items.optJSONObject(1) == null
                        ? "Unknown media"
                        : items.optJSONObject(1).optString("name", "Unknown media"));
            }
        }
        runOnUiThread(() -> queueSummaryView.setText(text.toString()));
    }

    private void advanceQueueAfterEnd() {
        if (nativePlayer == 0) return;
        final JSONArray items = loadItems(QUEUE_KEY);
        if (items.length() == 0) {
            abandonAudioFocus();
            runOnUiThread(() -> statusView.setText(
                    "Playback ended\nQueue is empty\nSession: " + nativeState(nativePlayer)));
            refreshPlaybackUi();
            return;
        }

        final JSONObject next = items.optJSONObject(0);
        if (next == null) {
            dropFirstQueueItem();
            advanceQueueAfterEnd();
            return;
        }

        final String nextUri = next.optString("uri", null);
        if (nextUri == null || nextUri.isEmpty()) {
            dropFirstQueueItem();
            advanceQueueAfterEnd();
            return;
        }

        final String nextName = next.optString("name", "Next media");
        final long durationMs = next.optLong("durationMs", 0);
        final boolean opened = openUri(Uri.parse(nextUri));
        if (!opened) {
            runOnUiThread(() -> statusView.setText(
                    "Next item failed to open: " + nativeLastError(nativePlayer)));
            refreshPlaybackUi();
            return;
        }
        if (!requestAudioFocus()) {
            restoreQueueItemAt(0, nextUri, nextName, durationMs);
            runOnUiThread(() -> statusView.setText(
                    "Next item opened but playback is waiting for audio focus\n" +
                    "Session: " + nativeState(nativePlayer)));
            refreshPlaybackUi();
            return;
        }
        if (nativePlay(nativePlayer)) {
            removeQueueItem(nextUri);
            playing = true;
            playbackHandler.post(pumpTask);
            runOnUiThread(() -> statusView.setText(
                    "Playing next: " + nextName + "\nSession: " + nativeState(nativePlayer)));
            refreshPlaybackUi();
        } else {
            abandonAudioFocus();
            restoreQueueItemAt(0, nextUri, nextName, durationMs);
            runOnUiThread(() -> statusView.setText(
                    "Next item failed: " + nativeLastError(nativePlayer)));
            refreshPlaybackUi();
        }
    }

    private void chooseTrack(boolean audio) {
        if (nativePlayer == 0) return;
        final String[] tracks = audio ? nativeAudioTracks(nativePlayer) : nativeVideoTracks(nativePlayer);
        if (tracks == null || tracks.length == 0) {
            statusView.setText((audio ? "No audio tracks" : "No video tracks") + "\nSession: " + nativeState(nativePlayer));
            return;
        }
        final String[] labels = new String[tracks.length];
        final int[] indexes = new int[tracks.length];
        for (int i = 0; i < tracks.length; ++i) {
            final String value = tracks[i] == null ? "" : tracks[i];
            final int tab = value.indexOf('\t');
            try {
                indexes[i] = Integer.parseInt(tab > 0 ? value.substring(0, tab) : value);
            } catch (NumberFormatException e) {
                indexes[i] = -1;
            }
            labels[i] = tab > 0 ? value.substring(tab + 1) : value;
        }
        new AlertDialog.Builder(this)
                .setTitle(audio ? "Audio tracks" : "Video tracks")
                .setItems(labels, (dialog, which) -> {
                    final int streamIndex = indexes[which];
                    final boolean ok = audio
                            ? nativeSelectAudioTrack(nativePlayer, streamIndex)
                            : nativeSelectVideoTrack(nativePlayer, streamIndex);
                    statusView.setText(ok
                            ? (audio ? "Audio track selected" : "Video track selected") + "\nSession: " + nativeState(nativePlayer)
                            : "Track selection failed: " + nativeLastError(nativePlayer));
                })
                .show();
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
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        intent.setType("*/*");
        startActivityForResult(intent, REQUEST_OPEN_MEDIA);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        if ((requestCode != REQUEST_OPEN_MEDIA && requestCode != REQUEST_QUEUE_MEDIA) ||
                resultCode != RESULT_OK || data == null || data.getData() == null) {
            return;
        }

        final Uri uri = data.getData();
        try {
            final int takeFlags = data.getFlags() &
                    (Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            getContentResolver().takePersistableUriPermission(uri, takeFlags & Intent.FLAG_GRANT_READ_URI_PERMISSION);
        } catch (Exception ignored) {
            // Providers may not offer persistable permissions; the cache-copy path remains available.
        }
        final String name = queryDisplayName(uri);
        if (requestCode == REQUEST_QUEUE_MEDIA) {
            playbackHandler.post(() -> {
                final boolean added = addQueueItem(uri.toString(), name, 0);
                runOnUiThread(() -> statusView.setText(
                        added ? "Added to queue: " + name : "Already in queue: " + name));
            });
        } else {
            playbackHandler.post(() -> openUri(uri));
        }
    }

    private String queryDisplayName(Uri uri) {
        if (uri == null) return "Unknown media";
        Cursor cursor = null;
        try {
            cursor = getContentResolver().query(uri, new String[]{"_display_name"}, null, null, null);
            if (cursor != null && cursor.moveToFirst()) {
                final int index = cursor.getColumnIndex("_display_name");
                if (index >= 0) {
                    final String value = cursor.getString(index);
                    if (value != null && !value.trim().isEmpty()) return value;
                }
            }
        } catch (Exception ignored) {
        } finally {
            if (cursor != null) cursor.close();
        }
        String value = uri.getLastPathSegment();
        return value == null || value.isEmpty() ? "Selected media" : value;
    }

    private JSONArray loadItems(String key) {
        try {
            return new JSONArray(libraryPrefs.getString(key, "[]"));
        } catch (Exception e) {
            return new JSONArray();
        }
    }

    private void saveItems(String key, JSONArray items) {
        libraryPrefs.edit().putString(key, items.toString()).apply();
    }

    private boolean sameUri(JSONObject item, String uri) {
        return uri != null && uri.equals(item.optString("uri", null));
    }

    private void addRecentItem(String uri, String name, long positionMs, long durationMs) {
        if (uri == null) return;
        JSONArray old = loadItems(RECENTS_KEY);
        JSONArray next = new JSONArray();
        try {
            JSONObject item = new JSONObject();
            item.put("uri", uri);
            item.put("name", name == null ? "Selected media" : name);
            item.put("positionMs", Math.max(0, positionMs));
            item.put("durationMs", Math.max(0, durationMs));
            item.put("lastOpenedMs", System.currentTimeMillis());
            next.put(item);
            for (int i = 0; i < old.length() && next.length() < MAX_RECENT_ITEMS; ++i) {
                JSONObject existing = old.optJSONObject(i);
                if (existing != null && !sameUri(existing, uri)) next.put(existing);
            }
            saveItems(RECENTS_KEY, next);
        } catch (Exception ignored) {
        }
    }

    private JSONObject findRecent(String uri) {
        if (uri == null) return null;
        JSONArray items = loadItems(RECENTS_KEY);
        for (int i = 0; i < items.length(); ++i) {
            JSONObject item = items.optJSONObject(i);
            if (item != null && sameUri(item, uri)) return item;
        }
        return null;
    }

    private boolean addQueueItem(String uri, String name, long durationMs) {
        if (uri == null) return false;
        JSONArray items = loadItems(QUEUE_KEY);
        for (int i = 0; i < items.length(); ++i) {
            JSONObject item = items.optJSONObject(i);
            if (item != null && sameUri(item, uri)) return false;
        }
        try {
            JSONObject item = new JSONObject();
            item.put("uri", uri);
            item.put("name", name == null ? "Selected media" : name);
            item.put("durationMs", Math.max(0, durationMs));
            items.put(item);
            if (items.length() > MAX_QUEUE_ITEMS) {
                JSONArray trimmed = new JSONArray();
                for (int i = Math.max(0, items.length() - MAX_QUEUE_ITEMS); i < items.length(); ++i) {
                    trimmed.put(items.opt(i));
                }
                items = trimmed;
            }
            saveItems(QUEUE_KEY, items);
            updateQueueSummary();
            return true;
        } catch (Exception ignored) {
            return false;
        }
    }

    private void restoreQueueItemAt(int index, String uri, String name, long durationMs) {
        if (uri == null || uri.isEmpty()) return;
        JSONArray items = loadItems(QUEUE_KEY);
        for (int i = 0; i < items.length(); ++i) {
            JSONObject item = items.optJSONObject(i);
            if (item != null && sameUri(item, uri)) {
                return;
            }
        }
        try {
            JSONObject item = new JSONObject();
            item.put("uri", uri);
            item.put("name", name == null ? "Next media" : name);
            item.put("durationMs", Math.max(0, durationMs));
            JSONArray restored = new JSONArray();
            final int safeIndex = Math.max(0, Math.min(index, items.length()));
            for (int i = 0; i < items.length() + 1; ++i) {
                if (i == safeIndex) restored.put(item);
                if (i < items.length()) restored.put(items.opt(i));
            }
            if (restored.length() > MAX_QUEUE_ITEMS) {
                JSONArray trimmed = new JSONArray();
                for (int i = 0; i < MAX_QUEUE_ITEMS; ++i) {
                    trimmed.put(restored.opt(i));
                }
                restored = trimmed;
            }
            saveItems(QUEUE_KEY, restored);
            updateQueueSummary();
        } catch (org.json.JSONException ignored) {
        }
    }

    private void dropFirstQueueItem() {
        final JSONArray items = loadItems(QUEUE_KEY);
        final JSONArray next = new JSONArray();
        for (int i = 1; i < items.length(); ++i) {
            next.put(items.opt(i));
        }
        saveItems(QUEUE_KEY, next);
        updateQueueSummary();
    }

    private void removeQueueItem(String uri) {
        JSONArray items = loadItems(QUEUE_KEY);
        JSONArray next = new JSONArray();
        for (int i = 0; i < items.length(); ++i) {
            JSONObject item = items.optJSONObject(i);
            if (item != null && !sameUri(item, uri)) next.put(item);
        }
        saveItems(QUEUE_KEY, next);
        updateQueueSummary();
    }

    private void persistCurrentPosition() {
        if (nativePlayer == 0 || currentUri == null) return;
        final long position = Math.max(0, nativePositionMs(nativePlayer));
        final long duration = Math.max(0, nativeDurationMs(nativePlayer));
        if (duration <= 0 || position <= 0) return;
        lastPositionPersistMs = System.currentTimeMillis();
        addRecentItem(currentUri, currentName, position, duration);
    }

    private void showLibrary() {
        JSONArray items = loadItems(RECENTS_KEY);
        if (items.length() == 0) {
            statusView.setText("Library is empty\nOpen media to build your Recent library.");
            return;
        }
        final String[] labels = new String[items.length()];
        final String[] uris = new String[items.length()];
        for (int i = 0; i < items.length(); ++i) {
            JSONObject item = items.optJSONObject(i);
            labels[i] = item == null ? "Unknown media" : item.optString("name", "Unknown media");
            uris[i] = item == null ? null : item.optString("uri", null);
        }
        new AlertDialog.Builder(this)
                .setTitle("Recent media")
                .setItems(labels, (dialog, which) -> {
                    if (uris[which] != null) {
                        final Uri uri = Uri.parse(uris[which]);
                        playbackHandler.post(() -> openUri(uri));
                    }
                })
                .setNegativeButton("Close", null)
                .show();
    }

    private void showQueue() {
        final JSONArray items = loadItems(QUEUE_KEY);
        if (items.length() == 0) {
            new AlertDialog.Builder(this)
                    .setTitle("Queue")
                    .setMessage("Queue is empty.")
                    .setPositiveButton("Add media", (dialog, which) -> chooseQueueMedia())
                    .setNegativeButton("Close", null)
                    .show();
            return;
        }

        final String[] labels = new String[items.length()];
        for (int i = 0; i < items.length(); ++i) {
            final JSONObject item = items.optJSONObject(i);
            labels[i] = item == null
                    ? (i + 1) + ". Unknown media"
                    : (i + 1) + ". " + item.optString("name", "Unknown media");
        }

        new AlertDialog.Builder(this)
                .setTitle("Queue — tap an item to play")
                .setItems(labels, (dialog, which) -> {
                    final JSONObject selected = items.optJSONObject(which);
                    if (selected == null) return;
                    final String uri = selected.optString("uri", null);
                    if (uri == null || uri.isEmpty()) return;
                    // The queue contains upcoming media only. Consume the
                    // selected item only after it opens successfully; otherwise it
                    // remains available for retry.
                    removeQueueItem(uri);
                    final String selectedName = selected.optString("name", "Selected media");
                    playbackHandler.post(() -> {
                        final boolean opened = openUri(Uri.parse(uri));
                        if (!opened) {
                            addQueueItem(uri, selectedName, selected.optLong("durationMs", 0));
                            runOnUiThread(() -> statusView.setText(
                                    "Queue item failed to open: " + nativeLastError(nativePlayer)));
                            refreshPlaybackUi();
                            return;
                        }
                        if (!requestAudioFocus()) {
                            addQueueItem(uri, selectedName, selected.optLong("durationMs", 0));
                            runOnUiThread(() -> statusView.setText(
                                    "Queue item opened but playback is waiting for audio focus"));
                            refreshPlaybackUi();
                            return;
                        }
                        if (nativePlay(nativePlayer)) {
                            playing = true;
                            playbackHandler.post(pumpTask);
                            runOnUiThread(() -> statusView.setText(
                                    "Playing: " + selectedName + "\nSession: " + nativeState(nativePlayer)));
                        } else {
                            abandonAudioFocus();
                            addQueueItem(uri, selectedName, selected.optLong("durationMs", 0));
                            runOnUiThread(() -> statusView.setText(
                                    "Queue item failed to play: " + nativeLastError(nativePlayer)));
                        }
                        refreshPlaybackUi();
                    });
                })
                .setNeutralButton("Manage", (dialog, which) -> showQueueManager())
                .setPositiveButton("Add media", (dialog, which) -> chooseQueueMedia())
                .setNegativeButton("Close", null)
                .show();
    }

    private void showQueueManager() {
        final JSONArray items = loadItems(QUEUE_KEY);
        if (items.length() == 0) {
            new AlertDialog.Builder(this)
                    .setTitle("Manage queue")
                    .setMessage("Queue is empty.")
                    .setPositiveButton("Add media", (dialog, which) -> chooseQueueMedia())
                    .setNegativeButton("Close", null)
                    .show();
            return;
        }

        final String[] labels = new String[items.length()];
        for (int i = 0; i < items.length(); ++i) {
            final JSONObject item = items.optJSONObject(i);
            labels[i] = item == null
                    ? (i + 1) + ". Unknown media"
                    : (i + 1) + ". " + item.optString("name", "Unknown media");
        }

        final String[] actions = {"Play", "Move up", "Move down", "Remove"};
        new AlertDialog.Builder(this)
                .setTitle("Manage queue")
                .setItems(labels, (dialog, which) -> showQueueItemActions(which, actions))
                .setNeutralButton("Clear queue", (dialog, which) -> clearQueue())
                .setPositiveButton("Add media", (dialog, which) -> chooseQueueMedia())
                .setNegativeButton("Close", null)
                .show();
    }

    private void showQueueItemActions(final int index, final String[] actions) {
        final JSONArray items = loadItems(QUEUE_KEY);
        if (index < 0 || index >= items.length()) return;
        final JSONObject selected = items.optJSONObject(index);
        if (selected == null) return;

        final String name = selected.optString("name", "Unknown media");
        new AlertDialog.Builder(this)
                .setTitle(name)
                .setItems(actions, (dialog, which) -> {
                    if (which == 0) {
                        final String uri = selected.optString("uri", null);
                        if (uri != null && !uri.isEmpty()) {
                            final long durationMs = selected.optLong("durationMs", 0);
                            playbackHandler.post(() -> {
                                final boolean opened = openUri(Uri.parse(uri));
                                if (!opened) {
                                    runOnUiThread(() -> statusView.setText(
                                            "Queue item failed to open: " + nativeLastError(nativePlayer)));
                                    refreshPlaybackUi();
                                    return;
                                }
                                removeQueueItem(uri);
                                if (!requestAudioFocus()) {
                                    addQueueItem(uri, name, durationMs);
                                    runOnUiThread(() -> statusView.setText(
                                            "Queue item opened but playback is waiting for audio focus"));
                                    refreshPlaybackUi();
                                    return;
                                }
                                if (nativePlay(nativePlayer)) {
                                    playing = true;
                                    playbackHandler.post(pumpTask);
                                    runOnUiThread(() -> statusView.setText(
                                            "Playing: " + name + "\nSession: " + nativeState(nativePlayer)));
                                } else {
                                    abandonAudioFocus();
                                    addQueueItem(uri, name, durationMs);
                                    runOnUiThread(() -> statusView.setText(
                                            "Queue item failed to play: " + nativeLastError(nativePlayer)));
                                }
                                refreshPlaybackUi();
                            });
                        }
                    } else if (which == 1) {
                        moveQueueItem(index, -1);
                    } else if (which == 2) {
                        moveQueueItem(index, 1);
                    } else {
                        removeQueueItemAt(index);
                    }
                })
                .show();
    }

    private void moveQueueItem(int index, int delta) {
        JSONArray items = loadItems(QUEUE_KEY);
        final int target = index + delta;
        if (index < 0 || index >= items.length() || target < 0 || target >= items.length()) {
            showQueueManager();
            return;
        }
        final Object current = items.opt(index);
        final Object swapped = items.opt(target);
        try {
            items.put(index, swapped);
            items.put(target, current);
        } catch (org.json.JSONException e) {
            statusView.setText("Queue reorder failed: " + e.getMessage());
            return;
        }
        saveItems(QUEUE_KEY, items);
        updateQueueSummary();
        showQueueManager();
    }

    private void removeQueueItemAt(int index) {
        JSONArray items = loadItems(QUEUE_KEY);
        if (index < 0 || index >= items.length()) return;
        JSONArray next = new JSONArray();
        for (int i = 0; i < items.length(); ++i) {
            if (i != index) next.put(items.opt(i));
        }
        saveItems(QUEUE_KEY, next);
        updateQueueSummary();
        showQueueManager();
    }

    private void clearQueue() {
        saveItems(QUEUE_KEY, new JSONArray());
        updateQueueSummary();
        statusView.setText("Queue cleared");
    }

    private void chooseQueueMedia() {
        final Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION);
        intent.setType("*/*");
        startActivityForResult(intent, REQUEST_QUEUE_MEDIA);
    }

    private boolean openUri(Uri uri) {