package com.kaviro.player;

import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/** Small, dependency-free SubRip parser suitable for local unit tests. */
public final class SrtSubtitleParser {
    public static final int MAX_BYTES = 5 * 1024 * 1024;
    public static final int MAX_CUES = 10000;
    public static final int MAX_CUE_CHARS = 4000;
    private static final Pattern TIMING = Pattern.compile("(\\d{1,2}):(\\d{2}):(\\d{2})[,.](\\d{1,3})\\s*-->\\s*(\\d{1,2}):(\\d{2}):(\\d{2})[,.](\\d{1,3}).*");

    private SrtSubtitleParser() {}

    public static final class Cue {
        public final long startMs;
        public final long endMs;
        public final String text;

        Cue(long startMs, long endMs, String text) {
            this.startMs = startMs;
            this.endMs = endMs;
            this.text = text;
        }
    }

    public static List<Cue> parse(byte[] contents) {
        if (contents == null) throw new IllegalArgumentException("Subtitle file is empty");
        if (contents.length > MAX_BYTES) throw new IllegalArgumentException("Subtitle file is too large (maximum 5 MB)");
        String source = new String(contents, StandardCharsets.UTF_8);
        if (source.startsWith("\uFEFF")) source = source.substring(1);
        String[] lines = source.split("\\r?\\n", -1);
        List<Cue> cues = new ArrayList<>();
        long start = -1, end = -1;
        StringBuilder cueText = new StringBuilder();
        for (String line : lines) {
            String trimmed = line.trim();
            Matcher timing = TIMING.matcher(trimmed);
            if (timing.matches()) {
                appendCue(cues, start, end, cueText);
                cueText.setLength(0);
                if (!hasValidClockFields(timing, 1) || !hasValidClockFields(timing, 5)) {
                    // Ignore impossible timestamps instead of interpreting their dialogue as a cue.
                    start = -1;
                    end = -1;
                    continue;
                }
                start = parseTime(timing, 1);
                end = parseTime(timing, 5);
            } else if (trimmed.isEmpty()) {
                appendCue(cues, start, end, cueText);
                start = -1;
                end = -1;
                cueText.setLength(0);
            } else if (start >= 0) {
                int extraChars = trimmed.length() + (cueText.length() > 0 ? 1 : 0);
                if (cueText.length() + extraChars > MAX_CUE_CHARS) {
                    throw new IllegalArgumentException("A subtitle cue is too long (maximum 4,000 characters)");
                }
                if (cueText.length() > 0) cueText.append('\n');
                cueText.append(trimmed);
            }
        }
        appendCue(cues, start, end, cueText);
        if (cues.isEmpty()) throw new IllegalArgumentException("No valid SRT subtitles found. Choose a SubRip (.srt) file.");
        Collections.sort(cues, Comparator.comparingLong(cue -> cue.startMs));
        return Collections.unmodifiableList(cues);
    }

    private static boolean hasValidClockFields(Matcher matcher, int group) {
        int minutes = Integer.parseInt(matcher.group(group + 1));
        int seconds = Integer.parseInt(matcher.group(group + 2));
        return minutes < 60 && seconds < 60;
    }

    private static void appendCue(List<Cue> cues, long start, long end, StringBuilder cueText) {
        if (start < 0 || end <= start || cueText.length() == 0) return;
        String text = cueText.toString().trim();
        if (text.isEmpty()) return;
        if (cues.size() >= MAX_CUES) {
            throw new IllegalArgumentException("Subtitle file contains too many cues (maximum 10,000)");
        }
        cues.add(new Cue(start, end, text));
    }

    private static long parseTime(Matcher matcher, int group) {
        long hours = Long.parseLong(matcher.group(group));
        long minutes = Long.parseLong(matcher.group(group + 1));
        long seconds = Long.parseLong(matcher.group(group + 2));
        String fraction = matcher.group(group + 3);
        long millis = Long.parseLong(fraction) * (fraction.length() == 1 ? 100 : fraction.length() == 2 ? 10 : 1);
        return (((hours * 60 + minutes) * 60) + seconds) * 1000 + millis;
    }
}
