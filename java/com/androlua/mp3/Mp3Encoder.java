package com.androlua.mp3;

/**
 * Minimal JNI bridge to the LAME MP3 encoder.
 *
 * Call loadNativeLibrary with the absolute path to libmp3encoder.so before
 * calling encodeWav. Input must be little-endian, 16-bit PCM WAV (mono/stereo).
 */
public final class Mp3Encoder {
    private static boolean loaded;

    private Mp3Encoder() {}

    public static synchronized void loadNativeLibrary(String absolutePath) {
        if (loaded) return;
        if (absolutePath == null || absolutePath.length() == 0) {
            throw new IllegalArgumentException("Native library path is empty");
        }
        System.load(absolutePath);
        loaded = true;
    }

    /**
     * Encodes an existing PCM WAV file to MP3 without modifying the WAV.
     * @return 0 on success; 1 invalid arguments; 2 input I/O; 3 invalid WAV;
     *         4 unsupported WAV format; 5 LAME initialization; 6 output I/O;
     *         7 encoding/write failure.
     */
    public static int encodeWav(String inputPath, String outputPath, int bitrateKbps) {
        if (!loaded) throw new IllegalStateException("Call loadNativeLibrary first");
        if (inputPath == null || outputPath == null) return 1;
        return encodeWavNative(inputPath, outputPath, bitrateKbps);
    }

    private static native int encodeWavNative(String inputPath, String outputPath, int bitrateKbps);
}
