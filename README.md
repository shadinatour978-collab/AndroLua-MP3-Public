# AndroLua MP3 Encoder

Android MP3 encoder components intended for AndroLua+ integration.

## Build outputs

The GitHub Actions workflow builds both Android ABIs:

- `arm64-v8a`
- `armeabi-v7a`

Each ABI artifact contains:

- `lib/libmp3lame.a` — LAME static encoder archive.
- `lib/libmp3encoder.so` — JNI shared library that links LAME.
- `mp3encoder.jar` — Java DEX wrapper (the JAR contains `classes.dex`).
- `include/lame.h` and `licenses/` — development header and license files.
- `BUILD-INFO.txt` — build target information.

## Java API

Class name: `com.androlua.mp3.Mp3Encoder`

Before encoding, load the native library by its absolute path:

```java
Mp3Encoder.loadNativeLibrary("/absolute/path/to/libmp3encoder.so");
int result = Mp3Encoder.encodeWav(inputWavPath, outputMp3Path, 192);
```

The wrapper must be loaded through a DEX-capable class loader, and the matching ABI's
`libmp3encoder.so` must be used. The bridge exposes a synchronous API; callers should
run encoding away from the UI thread.

## WAV input requirements

The current JNI bridge accepts RIFF/WAVE PCM files with:

- 16-bit signed little-endian samples.
- One (mono) or two (stereo) channels.
- Sample rates from 8 kHz through 48 kHz.

It writes a separate MP3 file and does not modify or delete the source WAV after success.

Return codes from `encodeWav`:

| Code | Meaning |
|---:|---|
| 0 | Success |
| 1 | Invalid arguments |
| 2 | Input file error |
| 3 | Invalid WAV container |
| 4 | Unsupported WAV encoding or layout |
| 5 | LAME initialization failed |
| 6 | Output file error |
| 7 | Encoding or write failure |

## Status and limitations

The static LAME archives have been built successfully for both listed ABIs. The JNI/DEX
workflow validates compilation, symbol export, and packaging. Runtime loading and an
end-to-end encode on an actual AndroLua+ installation still need to be verified before
integrating the bridge into an application.
