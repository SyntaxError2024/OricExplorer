#include "oric_core.h"
#include <stdlib.h>
#include <string.h>

static void write_le16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
}

static void write_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)((v >> 8) & 0xFFu);
    p[2] = (uint8_t)((v >> 16) & 0xFFu);
    p[3] = (uint8_t)((v >> 24) & 0xFFu);
}

bool oric_build_tap_buffer(const OricFileEntry *entry,
                           const uint8_t *payload,
                           size_t payloadSize,
                           uint8_t **outTap,
                           size_t *outTapSize) {
    if (!payload || payloadSize == 0 || !outTap || !outTapSize) return false;
    *outTap = NULL;
    *outTapSize = 0;

    char asciiName[24];
    size_t nlen = 0;
    if (entry) {
        for (size_t i = 0; entry->name[i] != L'\0' && nlen < sizeof(asciiName) - 1; i++) {
            wchar_t wc = entry->name[i];
            asciiName[nlen++] = (wc >= 32 && wc < 127) ? (char)wc : '_';
        }
    }
    if (nlen == 0) {
        strcpy(asciiName, "ORICFILE");
        nlen = strlen(asciiName);
    }
    asciiName[nlen] = '\0';

    uint8_t ftype = 0x80u;
    uint8_t autorun = 0x00u;
    uint16_t startAddr = 0x0501u;
    if (entry) {
        if (entry->source == ORIC_SRC_TAP) {
            ftype = entry->tapFileType;
            autorun = entry->tapAutorun;
        } else {
            ftype = entry->isBasic ? 0x00u : 0x80u;
            autorun = entry->isAutorun ? 0xC7u : 0x00u;
        }
        if (entry->hasStartEnd) {
            startAddr = entry->startAddr;
        }
    }
    uint16_t endAddr = (uint16_t)(startAddr + (uint16_t)(payloadSize - 1u));

    /* 4 octets 0x16 + 0x24 + 9 octets en-tête + nom + 0x00 + payload */
    size_t total = 4u + 1u + 9u + nlen + 1u + payloadSize;
    uint8_t *buf = (uint8_t *)malloc(total);
    if (!buf) return false;

    size_t pos = 0;
    buf[pos++] = 0x16;
    buf[pos++] = 0x16;
    buf[pos++] = 0x16;
    buf[pos++] = 0x16;
    buf[pos++] = 0x24;

    buf[pos++] = 0x00;
    buf[pos++] = ftype;
    buf[pos++] = autorun;
    buf[pos++] = (uint8_t)((endAddr >> 8) & 0xFFu);
    buf[pos++] = (uint8_t)(endAddr & 0xFFu);
    buf[pos++] = (uint8_t)((startAddr >> 8) & 0xFFu);
    buf[pos++] = (uint8_t)(startAddr & 0xFFu);
    buf[pos++] = 0x00;
    buf[pos++] = 0x00;

    memcpy(buf + pos, asciiName, nlen + 1u);
    pos += nlen + 1u;

    memcpy(buf + pos, payload, payloadSize);

    *outTap = buf;
    *outTapSize = total;
    return true;
}

typedef struct {
    uint8_t *buf;
    size_t len;
    size_t cap;
} WavVec;

static bool wav_push_samples(WavVec *v, uint8_t sample, size_t count) {
    if (v->len + count > v->cap) {
        size_t newCap = (v->cap == 0) ? 65536u : v->cap * 2u;
        while (newCap < v->len + count) newCap *= 2u;
        uint8_t *nb = (uint8_t *)realloc(v->buf, newCap);
        if (!nb) return false;
        v->buf = nb;
        v->cap = newCap;
    }
    memset(v->buf + v->len, sample, count);
    v->len += count;
    return true;
}

/* 48000 Hz => 208 us = 10 échantillons ; 416 us = 20 éch ; 624 us = 30 éch */
static bool wav_emit_bit(WavVec *v, int bit) {
    if (!wav_push_samples(v, 0xD8u, 10u)) return false;
    return wav_push_samples(v, 0x28u, bit ? 10u : 20u);
}

static bool wav_emit_byte(WavVec *v, uint8_t byte) {
    /* Start bit (0) */
    if (!wav_emit_bit(v, 0)) return false;

    /* 8 bits LSB first + calcul parité impaire Oric */
    int ones = 0;
    for (int b = 0; b < 8; b++) {
        int bit = (byte >> b) & 1;
        ones += bit;
        if (!wav_emit_bit(v, bit)) return false;
    }
    int parity = (ones & 1) ? 0 : 1;
    if (!wav_emit_bit(v, parity)) return false;

    /* 3 stop bits (1) */
    for (int s = 0; s < 3; s++) {
        if (!wav_emit_bit(v, 1)) return false;
    }
    return true;
}

bool oric_build_wav_from_tap(const uint8_t *tapData,
                             size_t tapSize,
                             uint8_t **outWav,
                             size_t *outWavSize) {
    if (!tapData || tapSize < 12 || !outWav || !outWavSize) return false;
    *outWav = NULL;
    *outWavSize = 0;

    WavVec pcm;
    memset(&pcm, 0, sizeof(pcm));

    /* En-tête RIFF WAVE 44 octets réservé */
    if (!wav_push_samples(&pcm, 0x80u, 44u)) {
        free(pcm.buf);
        return false;
    }

    size_t pos = 0;
    bool emittedAny = false;
    while (pos < tapSize) {
        if (tapData[pos] != 0x16) {
            pos++;
            continue;
        }
        while (pos < tapSize && tapData[pos] == 0x16) pos++;
        if (pos >= tapSize || tapData[pos] != 0x24) continue;
        pos++;

        if (pos + 9 > tapSize) break;
        const uint8_t *hdr = tapData + pos;
        pos += 9;

        uint16_t endAddr = (uint16_t)(((uint16_t)hdr[3] << 8) | hdr[4]);
        uint16_t startAddr = (uint16_t)(((uint16_t)hdr[5] << 8) | hdr[6]);

        size_t nameStart = pos;
        while (pos < tapSize && tapData[pos] != 0x00) pos++;
        size_t nameLen = (pos < tapSize) ? (pos - nameStart + 1u) : (pos - nameStart);
        if (pos < tapSize && tapData[pos] == 0x00) pos++;

        uint32_t payloadSize = (endAddr >= startAddr) ? (uint32_t)(endAddr - startAddr + 1u) : 0u;
        if (pos + payloadSize > tapSize) payloadSize = (uint32_t)(tapSize - pos);

        /* Leader de synchronisation K7 : 256 octets 0x16 + 0x24 */
        for (int i = 0; i < 256; i++) {
            if (!wav_emit_byte(&pcm, 0x16u)) { free(pcm.buf); return false; }
        }
        if (!wav_emit_byte(&pcm, 0x24u)) { free(pcm.buf); return false; }

        for (size_t i = 0; i < 9; i++) {
            if (!wav_emit_byte(&pcm, hdr[i])) { free(pcm.buf); return false; }
        }
        for (size_t i = 0; i < nameLen; i++) {
            uint8_t nc = (nameStart + i < tapSize) ? tapData[nameStart + i] : 0u;
            if (!wav_emit_byte(&pcm, nc)) { free(pcm.buf); return false; }
        }

        /* Courte pause (~25 ms) entre nom et bloc de données pour le lecteur Oric */
        for (int p = 0; p < 80; p++) {
            if (!wav_emit_bit(&pcm, 1)) { free(pcm.buf); return false; }
        }

        for (uint32_t i = 0; i < payloadSize; i++) {
            if (!wav_emit_byte(&pcm, tapData[pos + i])) { free(pcm.buf); return false; }
        }
        pos += payloadSize;
        emittedAny = true;

        /* Pause de fin (50 ms) */
        if (!wav_push_samples(&pcm, 0x80u, 2400u)) { free(pcm.buf); return false; }
    }

    if (!emittedAny || pcm.len <= 44u) {
        free(pcm.buf);
        return false;
    }

    uint32_t dataBytes = (uint32_t)(pcm.len - 44u);
    uint8_t *h = pcm.buf;
    memcpy(h + 0, "RIFF", 4);
    write_le32(h + 4, 36u + dataBytes);
    memcpy(h + 8, "WAVE", 4);
    memcpy(h + 12, "fmt ", 4);
    write_le32(h + 16, 16u);
    write_le16(h + 20, 1u);     /* PCM */
    write_le16(h + 22, 1u);     /* Mono */
    write_le32(h + 24, 48000u); /* 48 kHz */
    write_le32(h + 28, 48000u); /* byte rate */
    write_le16(h + 32, 1u);     /* block align */
    write_le16(h + 34, 8u);     /* 8-bit unsigned PCM */
    memcpy(h + 36, "data", 4);
    write_le32(h + 40, dataBytes);

    *outWav = pcm.buf;
    *outWavSize = pcm.len;
    return true;
}

static void write_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24) & 0xFFu);
    p[1] = (uint8_t)((v >> 16) & 0xFFu);
    p[2] = (uint8_t)((v >> 8) & 0xFFu);
    p[3] = (uint8_t)(v & 0xFFu);
}

static uint32_t oric_adler32(const uint8_t *data, size_t len) {
    uint32_t s1 = 1u;
    uint32_t s2 = 0u;
    const uint32_t MOD_ADLER = 65521u;
    for (size_t i = 0; i < len; i++) {
        s1 = (s1 + data[i]) % MOD_ADLER;
        s2 = (s2 + s1) % MOD_ADLER;
    }
    return (s2 << 16) | s1;
}

bool oric_build_png_from_bgra(const uint32_t *bgraPixels,
                              int width,
                              int height,
                              uint8_t **outPng,
                              size_t *outPngSize) {
    if (!bgraPixels || width <= 0 || height <= 0 || !outPng || !outPngSize) return false;
    *outPng = NULL;
    *outPngSize = 0;

    if ((size_t)width > 16384u || (size_t)height > 16384u) return false;

    size_t rowBytes = 1u + (size_t)width * 3u;
    size_t rawSize = (size_t)height * rowBytes;
    uint8_t *raw = (uint8_t *)malloc(rawSize);
    if (!raw) return false;

    for (int y = 0; y < height; y++) {
        uint8_t *dstRow = raw + (size_t)y * rowBytes;
        const uint32_t *srcRow = bgraPixels + (size_t)y * (size_t)width;
        dstRow[0] = 0x00u; /* PNG Filter Type 0: None */
        for (int x = 0; x < width; x++) {
            uint32_t px = srcRow[x];
            dstRow[1u + (size_t)x * 3u + 0u] = (uint8_t)((px >> 16) & 0xFFu); /* R */
            dstRow[1u + (size_t)x * 3u + 1u] = (uint8_t)((px >> 8)  & 0xFFu); /* G */
            dstRow[1u + (size_t)x * 3u + 2u] = (uint8_t)(px & 0xFFu);         /* B */
        }
    }

    size_t numBlocks = (rawSize + 65534u) / 65535u;
    if (numBlocks == 0) numBlocks = 1u;
    size_t zlibSize = 2u + numBlocks * 5u + rawSize + 4u;

    /* Signature(8) + IHDR(12+13) + IDAT(12+zlibSize) + IEND(12) */
    size_t totalSize = 8u + 25u + (12u + zlibSize) + 12u;
    uint8_t *png = (uint8_t *)malloc(totalSize);
    if (!png) {
        free(raw);
        return false;
    }

    size_t pos = 0;
    static const uint8_t PNG_SIG[8] = { 0x89u, 'P', 'N', 'G', '\r', '\n', 0x1Au, '\n' };
    memcpy(png + pos, PNG_SIG, 8u);
    pos += 8u;

    /* Chunk IHDR */
    write_be32(png + pos, 13u);
    pos += 4u;
    size_t ihdrTypeStart = pos;
    memcpy(png + pos, "IHDR", 4u);
    pos += 4u;
    write_be32(png + pos, (uint32_t)width);  pos += 4u;
    write_be32(png + pos, (uint32_t)height); pos += 4u;
    png[pos++] = 8u; /* Bit depth: 8 */
    png[pos++] = 2u; /* Color type: 2 (Truecolor RGB) */
    png[pos++] = 0u; /* Compression: DEFLATE */
    png[pos++] = 0u; /* Filter: Adaptive (0) */
    png[pos++] = 0u; /* Interlace: None */
    write_be32(png + pos, oric_crc32(png + ihdrTypeStart, 4u + 13u));
    pos += 4u;

    /* Chunk IDAT */
    write_be32(png + pos, (uint32_t)zlibSize);
    pos += 4u;
    size_t idatTypeStart = pos;
    memcpy(png + pos, "IDAT", 4u);
    pos += 4u;

    /* zlib header (CMF=0x78, FLG=0x01: deflate 32K window, fastest) */
    png[pos++] = 0x78u;
    png[pos++] = 0x01u;

    size_t rawOffset = 0;
    while (rawOffset < rawSize) {
        size_t chunk = rawSize - rawOffset;
        if (chunk > 65535u) chunk = 65535u;
        bool isFinal = (rawOffset + chunk >= rawSize);

        uint16_t len16 = (uint16_t)chunk;
        uint16_t nlen16 = (uint16_t)(~len16);

        png[pos++] = isFinal ? 0x01u : 0x00u;
        write_le16(png + pos, len16);  pos += 2u;
        write_le16(png + pos, nlen16); pos += 2u;
        memcpy(png + pos, raw + rawOffset, chunk);
        pos += chunk;
        rawOffset += chunk;
    }

    write_be32(png + pos, oric_adler32(raw, rawSize));
    pos += 4u;
    free(raw);

    write_be32(png + pos, oric_crc32(png + idatTypeStart, 4u + zlibSize));
    pos += 4u;

    /* Chunk IEND */
    write_be32(png + pos, 0u);
    pos += 4u;
    size_t iendTypeStart = pos;
    memcpy(png + pos, "IEND", 4u);
    pos += 4u;
    write_be32(png + pos, oric_crc32(png + iendTypeStart, 4u));
    pos += 4u;

    *outPng = png;
    *outPngSize = pos;
    return true;
}
