/* Decodes the power-up jingle (AAC in a 3GP file) with the operating system's own decoder, so the desktop importer needs no extra software.
 *   Windows: Media Foundation (loaded at run time, so the program still starts on Windows editions that ship without it)
 *   macOS:   AudioToolbox
 *   others:  not available here (the importer decodes the jingle itself with aac_dec.c first; this file is only the fallback)
 * Returns a malloc'd 16-bit PCM WAV file (the sample rate and channel count are the decoder's own; the engine converts when it loads it), or NULL.
 * Plain C interface, no SDL, so the Windows headers stay out of the game's own files. */
#include <stdlib.h>
#include <string.h>

unsigned char *pnj_native_decode(const char *path, size_t *out_size);

static unsigned char *wav_wrap(const unsigned char *pcm, size_t n, unsigned rate, unsigned ch) {
    unsigned char *w = (unsigned char *)malloc(44 + n); if (!w) return 0;
    unsigned br = rate * ch * 2, ba = ch * 2; unsigned v;
    #define P32(o, x) (v = (unsigned)(x), w[o] = v & 255, w[(o) + 1] = (v >> 8) & 255, w[(o) + 2] = (v >> 16) & 255, w[(o) + 3] = (v >> 24) & 255)
    #define P16(o, x) (v = (unsigned)(x), w[o] = v & 255, w[(o) + 1] = (v >> 8) & 255)
    memcpy(w, "RIFF", 4); P32(4, 36 + n); memcpy(w + 8, "WAVEfmt ", 8); P32(16, 16); P16(20, 1); P16(22, ch); P32(24, rate); P32(28, br); P16(32, ba); P16(34, 16);
    memcpy(w + 36, "data", 4); P32(40, n); memcpy(w + 44, pcm, n);
    #undef P32
    #undef P16
    return w;
}

#if defined(_WIN32)
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>

typedef HRESULT (WINAPI *pMFStartup)(ULONG, DWORD);
typedef HRESULT (WINAPI *pMFShutdown)(void);
typedef HRESULT (WINAPI *pMFCreateMediaType)(IMFMediaType **);
typedef HRESULT (WINAPI *pMFCreateSourceReaderFromURL)(LPCWSTR, IMFAttributes *, IMFSourceReader **);

unsigned char *pnj_native_decode(const char *path, size_t *out_size) {
    unsigned char *result = 0;
    HMODULE plat = LoadLibraryW(L"mfplat.dll"), rdr = LoadLibraryW(L"mfreadwrite.dll");
    if (!plat || !rdr) return 0;
    pMFStartup startup = (pMFStartup)(void *)GetProcAddress(plat, "MFStartup");
    pMFShutdown shutdown = (pMFShutdown)(void *)GetProcAddress(plat, "MFShutdown");
    pMFCreateMediaType create_type = (pMFCreateMediaType)(void *)GetProcAddress(plat, "MFCreateMediaType");
    pMFCreateSourceReaderFromURL create_reader = (pMFCreateSourceReaderFromURL)(void *)GetProcAddress(rdr, "MFCreateSourceReaderFromURL");
    if (!startup || !shutdown || !create_type || !create_reader) return 0;

    HRESULT co = CoInitializeEx(NULL, COINIT_MULTITHREADED);
    if (FAILED(startup(MF_VERSION, MFSTARTUP_LITE))) { if (SUCCEEDED(co)) CoUninitialize(); return 0; }
    wchar_t wpath[1024]; if (!MultiByteToWideChar(CP_UTF8, 0, path, -1, wpath, 1024)) goto done;

    IMFSourceReader *reader = 0; IMFMediaType *want = 0, *got = 0;
    unsigned char *pcm = 0; size_t n = 0, cap = 0; unsigned rate = 0, ch = 0, bits = 0;
    if (FAILED(create_reader(wpath, NULL, &reader)) || !reader) goto done;
    if (FAILED(create_type(&want))) goto done;
    IMFMediaType_SetGUID(want, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio);
    IMFMediaType_SetGUID(want, &MF_MT_SUBTYPE, &MFAudioFormat_PCM);
    if (FAILED(IMFSourceReader_SetCurrentMediaType(reader, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, want))) goto cleanup;
    if (SUCCEEDED(IMFSourceReader_GetCurrentMediaType(reader, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, &got)) && got) {
        UINT32 v = 0;
        if (SUCCEEDED(IMFMediaType_GetUINT32(got, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &v))) rate = v;
        if (SUCCEEDED(IMFMediaType_GetUINT32(got, &MF_MT_AUDIO_NUM_CHANNELS, &v))) ch = v;
        if (SUCCEEDED(IMFMediaType_GetUINT32(got, &MF_MT_AUDIO_BITS_PER_SAMPLE, &v))) bits = v;
    }
    if (bits != 16 || ch < 1 || ch > 2 || rate < 8000) goto cleanup;
    for (;;) {
        DWORD flags = 0; IMFSample *s = 0;
        if (FAILED(IMFSourceReader_ReadSample(reader, (DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &flags, NULL, &s))) break;
        if (s) {
            IMFMediaBuffer *b = 0;
            if (SUCCEEDED(IMFSample_ConvertToContiguousBuffer(s, &b)) && b) {
                BYTE *d = 0; DWORD len = 0;
                if (SUCCEEDED(IMFMediaBuffer_Lock(b, &d, NULL, &len)) && d) {
                    if (n + len > cap) { cap = (n + len) * 2 + 65536; unsigned char *q = (unsigned char *)realloc(pcm, cap); if (!q) { IMFMediaBuffer_Unlock(b); IMFMediaBuffer_Release(b); IMFSample_Release(s); goto cleanup; } pcm = q; }
                    memcpy(pcm + n, d, len); n += len; IMFMediaBuffer_Unlock(b);
                }
                IMFMediaBuffer_Release(b);
            }
            IMFSample_Release(s);
        }
        if (flags & (MF_SOURCE_READERF_ENDOFSTREAM | MF_SOURCE_READERF_ERROR)) break;
        if (n > (64u << 20)) break;
    }
    if (n > 4096) { result = wav_wrap(pcm, n, rate, ch); if (result) *out_size = 44 + n; }
cleanup:
    free(pcm);
    if (got) IMFMediaType_Release(got);
    if (want) IMFMediaType_Release(want);
    if (reader) IMFSourceReader_Release(reader);
done:
    shutdown();
    if (SUCCEEDED(co)) CoUninitialize();
    return result;
}

#elif defined(__APPLE__)
#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>

unsigned char *pnj_native_decode(const char *path, size_t *out_size) {
    unsigned char *result = 0;
    CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path, (CFIndex)strlen(path), false);
    if (!url) return 0;
    ExtAudioFileRef f = 0;
    OSStatus st = ExtAudioFileOpenURL(url, &f); CFRelease(url);
    if (st != noErr || !f) return 0;
    AudioStreamBasicDescription fmt; memset(&fmt, 0, sizeof fmt);
    fmt.mSampleRate = 44100; fmt.mFormatID = kAudioFormatLinearPCM; fmt.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
    fmt.mBytesPerPacket = 4; fmt.mFramesPerPacket = 1; fmt.mBytesPerFrame = 4; fmt.mChannelsPerFrame = 2; fmt.mBitsPerChannel = 16;
    if (ExtAudioFileSetProperty(f, kExtAudioFileProperty_ClientDataFormat, sizeof fmt, &fmt) != noErr) { ExtAudioFileDispose(f); return 0; }
    unsigned char *pcm = 0; size_t n = 0, cap = 0;
    for (;;) {
        unsigned char chunk[16384]; AudioBufferList bl; bl.mNumberBuffers = 1;
        bl.mBuffers[0].mNumberChannels = 2; bl.mBuffers[0].mDataByteSize = sizeof chunk; bl.mBuffers[0].mData = chunk;
        UInt32 frames = sizeof chunk / 4;
        if (ExtAudioFileRead(f, &frames, &bl) != noErr || frames == 0) break;
        size_t add = (size_t)frames * 4;
        if (n + add > cap) { cap = (n + add) * 2 + 65536; unsigned char *q = (unsigned char *)realloc(pcm, cap); if (!q) { free(pcm); ExtAudioFileDispose(f); return 0; } pcm = q; }
        memcpy(pcm + n, chunk, add); n += add;
        if (n > (64u << 20)) break;
    }
    ExtAudioFileDispose(f);
    if (n > 4096) { result = wav_wrap(pcm, n, 44100, 2); if (result) *out_size = 44 + n; }
    free(pcm);
    return result;
}

#else
unsigned char *pnj_native_decode(const char *path, size_t *out_size) { (void)path; (void)out_size; return 0; }
#endif
