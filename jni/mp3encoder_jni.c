#include <jni.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lame.h"

#define ERR_OK 0
#define ERR_ARGUMENT 1
#define ERR_INPUT 2
#define ERR_WAV_FORMAT 3
#define ERR_UNSUPPORTED_PCM 4
#define ERR_ENCODER_INIT 5
#define ERR_OUTPUT 6
#define ERR_ENCODE 7

static uint16_t le16(const unsigned char *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
static uint32_t le32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static int encode_wav(const char *input_path, const char *output_path, int bitrate) {
    FILE *in = NULL, *out = NULL;
    lame_t lame = NULL;
    int result = ERR_WAV_FORMAT;
    unsigned char header[12], chunk[8];
    uint16_t format = 0, channels = 0, bits = 0, block_align = 0;
    uint32_t sample_rate = 0, data_size = 0;
    long data_offset = -1;
    unsigned char *pcm_bytes = NULL;
    short *pcm = NULL;
    unsigned char mp3[8192];

    if (!input_path || !output_path || bitrate < 32 || bitrate > 320) return ERR_ARGUMENT;
    in = fopen(input_path, "rb");
    if (!in) return ERR_INPUT;
    if (fread(header, 1, 12, in) != 12 ||
        memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) {
        result = ERR_WAV_FORMAT; goto cleanup;
    }

    while (fread(chunk, 1, 8, in) == 8) {
        uint32_t size = le32(chunk + 4);
        long payload = ftell(in);
        if (memcmp(chunk, "fmt ", 4) == 0) {
            unsigned char fmt[40];
            size_t take = size < sizeof(fmt) ? size : sizeof(fmt);
            if (take < 16 || fread(fmt, 1, take, in) != take) {
                result = ERR_WAV_FORMAT; goto cleanup;
            }
            format = le16(fmt);
            channels = le16(fmt + 2);
            sample_rate = le32(fmt + 4);
            block_align = le16(fmt + 12);
            bits = le16(fmt + 14);
            if (fseek(in, payload + (long)size + (size & 1U), SEEK_SET) != 0) {
                result = ERR_WAV_FORMAT; goto cleanup;
            }
        } else if (memcmp(chunk, "data", 4) == 0) {
            data_offset = payload;
            data_size = size;
            if (fseek(in, payload + (long)size + (size & 1U), SEEK_SET) != 0) {
                result = ERR_WAV_FORMAT; goto cleanup;
            }
        } else {
            if (fseek(in, payload + (long)size + (size & 1U), SEEK_SET) != 0) {
                result = ERR_WAV_FORMAT; goto cleanup;
            }
        }
        if (format && data_offset >= 0) break;
    }

    if (format != 1 || (channels != 1 && channels != 2) || bits != 16 ||
        sample_rate < 8000 || sample_rate > 48000 ||
        block_align != channels * 2 || data_offset < 0 || data_size < block_align) {
        result = ERR_UNSUPPORTED_PCM; goto cleanup;
    }

    lame = lame_init();
    if (!lame) { result = ERR_ENCODER_INIT; goto cleanup; }
    lame_set_in_samplerate(lame, (int)sample_rate);
    lame_set_out_samplerate(lame, (int)sample_rate);
    lame_set_num_channels(lame, (int)channels);
    lame_set_brate(lame, bitrate);
    lame_set_quality(lame, 2);
    lame_set_mode(lame, channels == 1 ? MONO : JOINT_STEREO);
    if (lame_init_params(lame) < 0) { result = ERR_ENCODER_INIT; goto cleanup; }

    out = fopen(output_path, "wb");
    if (!out) { result = ERR_OUTPUT; goto cleanup; }
    if (fseek(in, data_offset, SEEK_SET) != 0) { result = ERR_INPUT; goto cleanup; }

    {
        const size_t samples_per_block = 1152;
        const size_t bytes_per_block = samples_per_block * block_align;
        pcm_bytes = (unsigned char *)malloc(bytes_per_block);
        pcm = (short *)malloc(samples_per_block * channels * sizeof(short));
        if (!pcm_bytes || !pcm) { result = ERR_ENCODE; goto cleanup; }
        uint32_t remaining = data_size;
        while (remaining >= block_align) {
            size_t want = remaining < bytes_per_block ? remaining : bytes_per_block;
            want -= want % block_align;
            size_t got = fread(pcm_bytes, 1, want, in);
            if (got == 0 || got % block_align != 0) { result = ERR_INPUT; goto cleanup; }
            size_t sample_count = got / block_align;
            for (size_t i = 0; i < sample_count * channels; ++i) {
                pcm[i] = (short)le16(pcm_bytes + i * 2);
            }
            int encoded;
            if (channels == 2) {
                encoded = lame_encode_buffer_interleaved(lame, pcm, (int)sample_count, mp3, (int)sizeof(mp3));
            } else {
                encoded = lame_encode_buffer(lame, pcm, pcm, (int)sample_count, mp3, (int)sizeof(mp3));
            }
            if (encoded < 0 || (encoded > 0 && fwrite(mp3, 1, (size_t)encoded, out) != (size_t)encoded)) {
                result = ERR_ENCODE; goto cleanup;
            }
            remaining -= (uint32_t)got;
        }
    }

    {
        int encoded = lame_encode_flush(lame, mp3, (int)sizeof(mp3));
        if (encoded < 0 || (encoded > 0 && fwrite(mp3, 1, (size_t)encoded, out) != (size_t)encoded)) {
            result = ERR_ENCODE; goto cleanup;
        }
    }
    if (fflush(out) != 0) { result = ERR_OUTPUT; goto cleanup; }
    result = ERR_OK;

cleanup:
    if (out) fclose(out);
    if (result != ERR_OK && output_path) remove(output_path);
    if (in) fclose(in);
    if (lame) lame_close(lame);
    free(pcm_bytes);
    free(pcm);
    return result;
}

JNIEXPORT jint JNICALL
Java_com_androlua_mp3_Mp3Encoder_encodeWavNative(JNIEnv *env, jclass clazz,
                                                  jstring input, jstring output,
                                                  jint bitrate) {
    (void)clazz;
    if (!input || !output) return ERR_ARGUMENT;
    const char *in_path = (*env)->GetStringUTFChars(env, input, NULL);
    if (!in_path) return ERR_ARGUMENT;
    const char *out_path = (*env)->GetStringUTFChars(env, output, NULL);
    if (!out_path) {
        (*env)->ReleaseStringUTFChars(env, input, in_path);
        return ERR_ARGUMENT;
    }
    int result = encode_wav(in_path, out_path, (int)bitrate);
    (*env)->ReleaseStringUTFChars(env, input, in_path);
    (*env)->ReleaseStringUTFChars(env, output, out_path);
    return (jint)result;
}
