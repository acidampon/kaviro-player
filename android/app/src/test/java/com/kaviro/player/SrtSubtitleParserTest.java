package com.kaviro.player;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.junit.Assert.fail;

import java.nio.charset.StandardCharsets;
import java.util.List;
import org.junit.Test;

public class SrtSubtitleParserTest {
    private static byte[] utf8(String value) {
        return value.getBytes(StandardCharsets.UTF_8);
    }

    @Test public void parsesCommaAndDotFractionsAndNormalizesMilliseconds() {
        List<SrtSubtitleParser.Cue> cues = SrtSubtitleParser.parse(utf8(
                "1\n00:00:01,2 --> 00:00:02,34\nFirst\n\n" +
                "2\n00:00:03.456 --> 00:00:04.789\nSecond\n"));
        assertEquals(2, cues.size());
        assertEquals(1200L, cues.get(0).startMs);
        assertEquals(2340L, cues.get(0).endMs);
        assertEquals(3456L, cues.get(1).startMs);
        assertEquals(4789L, cues.get(1).endMs);
    }

    @Test public void acceptsUtf8BomAndWindowsLineEndings() {
        List<SrtSubtitleParser.Cue> cues = SrtSubtitleParser.parse(utf8(
                "\uFEFF1\r\n00:00:00,000 --> 00:00:01,000\r\nCaption\r\n"));
        assertEquals(1, cues.size());
        assertEquals("Caption", cues.get(0).text);
    }

    @Test public void preservesMultilineAndNumericOnlyDialogue() {
        List<SrtSubtitleParser.Cue> cues = SrtSubtitleParser.parse(utf8(
                "00:00:00,000 --> 00:00:01,000\nHello\nworld\n\n" +
                "00:00:01,000 --> 00:00:02,000\n12345\n"));
        assertEquals("Hello\nworld", cues.get(0).text);
        assertEquals("12345", cues.get(1).text);
    }

    @Test public void sortsCuesByStartTime() {
        List<SrtSubtitleParser.Cue> cues = SrtSubtitleParser.parse(utf8(
                "00:00:05,000 --> 00:00:06,000\nLater\n\n" +
                "00:00:01,000 --> 00:00:02,000\nEarlier\n"));
        assertEquals("Earlier", cues.get(0).text);
        assertEquals("Later", cues.get(1).text);
    }

    @Test public void ignoresImpossibleMinuteAndSecondValues() {
        List<SrtSubtitleParser.Cue> cues = SrtSubtitleParser.parse(utf8(
                "00:60:00,000 --> 00:60:01,000\nInvalid minute\n\n" +
                "00:00:60,000 --> 00:01:01,000\nInvalid second\n\n" +
                "00:00:00,000 --> 00:00:01,000\nValid cue\n"));
        assertEquals(1, cues.size());
        assertEquals("Valid cue", cues.get(0).text);
    }

    @Test public void ignoresMalformedAndNonIncreasingCuesButRequiresOneValidCue() {
        List<SrtSubtitleParser.Cue> cues = SrtSubtitleParser.parse(utf8(
                "not a timestamp\nignored\n\n" +
                "00:00:02,000 --> 00:00:01,000\nInvalid range\n\n" +
                "00:00:00,000 --> 00:00:01,000\nValid\n"));
        assertEquals(1, cues.size());
        assertEquals("Valid", cues.get(0).text);
        expectFailure("No valid SRT subtitles", () -> SrtSubtitleParser.parse(utf8("garbage")));
    }

    @Test public void rejectsCueTextOverLimit() {
        String longLine = new String(new char[SrtSubtitleParser.MAX_CUE_CHARS + 1]).replace('\\0', 'x');
        expectFailure("cue is too long", () -> SrtSubtitleParser.parse(utf8(
                "00:00:00,000 --> 00:00:01,000\n" + longLine)));
    }

    @Test public void rejectsMoreThanMaximumCueCount() {
        StringBuilder srt = new StringBuilder();
        for (int i = 0; i <= SrtSubtitleParser.MAX_CUES; i++) {
            srt.append("00:00:00,000 --> 00:00:01,000\nCue ").append(i).append("\n\n");
        }
        expectFailure("too many cues", () -> SrtSubtitleParser.parse(utf8(srt.toString())));
    }

    @Test public void rejectsOversizedInputAndNull() {
        expectFailure("too large", () -> SrtSubtitleParser.parse(new byte[SrtSubtitleParser.MAX_BYTES + 1]));
        expectFailure("empty", () -> SrtSubtitleParser.parse(null));
    }

    @Test public void returnedCueListCannotBeMutated() {
        List<SrtSubtitleParser.Cue> cues = SrtSubtitleParser.parse(utf8(
                "00:00:00,000 --> 00:00:01,000\nOne\n"));
        try {
            cues.clear();
            fail("Expected an unmodifiable list");
        } catch (UnsupportedOperationException expected) {
            assertTrue(true);
        }
    }

    private static void expectFailure(String messagePart, Runnable action) {
        try {
            action.run();
            fail("Expected parser to reject input containing: " + messagePart);
        } catch (IllegalArgumentException expected) {
            assertTrue("Unexpected error: " + expected.getMessage(),
                    expected.getMessage().toLowerCase().contains(messagePart.toLowerCase()));
        }
    }
}
