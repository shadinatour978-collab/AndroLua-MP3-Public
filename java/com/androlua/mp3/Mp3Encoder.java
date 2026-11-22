package com.androlua.mp3;

/**
 * Minimal JNI bridge to the LAME MP3 encoder.
 *
 * Load this class from a DexClassLoader, create an instance, then call
 * loadNativeLibrary with the absolute path to libmp3encoder.so.
 */
public final class Mp3Encoder {
    private boolean loaded;

    public Mp3Encoder() {}

    public synchronized void loadNativeLibrary(String absolutePath) {
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
    public int encodeWav(String inputPath, String outputPath, int bitrateKbps) {
        if (!loaded) throw new IllegalStateException("Call loadNativeLibrary first");
        if (inputPath == null || outputPath == null) return 1;
        return encodeWavNative(inputPath, outputPath, bitrateKbps);
    }

    private native int encodeWavNative(String inputPath, String outputPath, int bitrateKbps);
}
