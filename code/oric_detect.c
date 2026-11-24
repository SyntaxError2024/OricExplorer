#include "oric_core.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void oric_report_init(OricMediaReport *report) {
    if (!report) return;
    memset(report, 0, sizeof(*report));
    report->mediaType = ORIC_MEDIA_NONE;
}

void oric_report_free(OricMediaReport *report) {
    if (!report) return;
    free(report->entries);
    report->entries = NULL;
    report->entryCount = 0;
    report->entryCapacity = 0;
}

bool oric_report_add_entry(OricMediaReport *report, const OricFileEntry *entry) {
    if (!report || !entry) return false;
    if (report->entryCount >= report->entryCapacity) {
        size_t newCap = (report->entryCapacity == 0) ? 16 : report->entryCapacity * 2;
        OricFileEntry *newBuf = (OricFileEntry *)realloc(
            report->entries, newCap * sizeof(OricFileEntry));
        if (!newBuf) return false;
        report->entries = newBuf;
        report->entryCapacity = newCap;
    }
    report->entries[report->entryCount++] = *entry;
    return true;
}

void oric_ascii_to_wide(const char *src, size_t maxLen, wchar_t *dst, size_t dstChars) {
    if (!dst || dstChars == 0) return;
    if (!src) {
        dst[0] = L'\0';
        return;
    }
    size_t i = 0;
    while (i < maxLen && src[i] != '\0' && i + 1 < dstChars) {
        unsigned char c = (unsigned char)src[i];
        dst[i] = (c >= 32 && c < 127) ? (wchar_t)c : L'?';
        i++;
    }
    dst[i] = L'\0';
}

static bool has_extension_icase(const wchar_t *path, const wchar_t *extWithDot) {
    if (!path || !extWithDot) return false;
    const wchar_t *dot = wcsrchr(path, L'.');
    if (!dot) return false;
    while (*dot && *extWithDot) {
        wchar_t a = (*dot >= L'A' && *dot <= L'Z') ? (wchar_t)(*dot + 32) : *dot;
        wchar_t b = (*extWithDot >= L'A' && *extWithDot <= L'Z') ? (wchar_t)(*extWithDot + 32) : *extWithDot;
        if (a != b) return false;
        dot++;
        extWithDot++;
    }
    return (*dot == L'\0' && *extWithDot == L'\0');
}

static const wchar_t *extract_basename(const wchar_t *path) {
    if (!path) return L"";
    const wchar_t *slash1 = wcsrchr(path, L'\\');
    const wchar_t *slash2 = wcsrchr(path, L'/');
    const wchar_t *best = (slash1 > slash2) ? slash1 : slash2;
    return best ? (best + 1) : path;
}

bool oric_analyze_buffer(const uint8_t *data,
                         size_t size,
                         const wchar_t *filePath,
                         OricLang lang,
                         OricMediaReport *report) {
    if (!report) return false;
    oric_report_free(report);
    oric_report_init(report);
    report->lang = lang;

    if (filePath) {
        wcsncpy(report->filePath, filePath, 519);
        wcsncpy(report->fileName, extract_basename(filePath), 259);
    }
    report->rawFileSize = (uint32_t)size;

    if (!data || size == 0) {
        report->mediaType = ORIC_MEDIA_UNKNOWN;
        swprintf(report->statusMessage, 256, L"%ls", oric_tr(lang, STR_ERR_EMPTY_FILE));
        return false;
    }

    if (size >= 256 && (memcmp(data, "ORICDISK", 8) == 0 || memcmp(data, "MFM_DISK", 8) == 0)) {
        return oric_parse_dsk(data, size, report);
    }

    if (has_extension_icase(filePath, L".rom") || has_extension_icase(filePath, L".bin")) {
        return oric_analyze_rom(data, size, report);
    }

    if (has_extension_icase(filePath, L".tap")) {
        return oric_parse_tap(data, size, report);
    }

    if (has_extension_icase(filePath, L".dsk")) {
        return oric_parse_dsk(data, size, report);
    }

    if (size >= 16 && data[0] == 0x16) {
        size_t p = 0;
        while (p < size && p < 512 && data[p] == 0x16) p++;
        if (p < size && data[p] == 0x24) {
            return oric_parse_tap(data, size, report);
        }
    }

    if (size == 16384u || size == 8192u || size == 2048u || size == 32768u) {
        return oric_analyze_rom(data, size, report);
    }

    report->mediaType = ORIC_MEDIA_UNKNOWN;
    swprintf(report->statusMessage, 256, L"%ls", oric_tr(lang, STR_ERR_UNKNOWN_FORMAT));
    return false;
}

static void append_fmt(wchar_t *buf, size_t bufChars, size_t *pos, const wchar_t *fmt, ...) {
    if (*pos + 1 >= bufChars) return;
    va_list args;
    va_start(args, fmt);
    int n = vswprintf(buf + *pos, bufChars - *pos, fmt, args);
    va_end(args);
    if (n > 0) {
        *pos += (size_t)n;
        if (*pos >= bufChars) *pos = bufChars - 1;
    }
}

void oric_format_summary(const OricMediaReport *report,
                         OricLang lang,
                         int selectedIndex,
                         wchar_t *outBuf,
                         size_t outChars) {
    if (!outBuf || outChars == 0) return;
    outBuf[0] = L'\0';
    size_t pos = 0;

    if (!report || report->mediaType == ORIC_MEDIA_NONE) {
        append_fmt(outBuf, outChars, &pos, L"%ls", oric_tr(lang, STR_SUM_WELCOME));
        return;
    }

    append_fmt(outBuf, outChars, &pos,
               oric_tr(lang, STR_SUM_HEADER_FMT),
               report->fileName,
               report->rawFileSize,
               (double)report->rawFileSize / 1024.0);

    if (report->mediaType == ORIC_MEDIA_TAP) {
        uint64_t totalPayload = 0;
        for (size_t i = 0; i < report->entryCount; i++) {
            totalPayload += report->entries[i].sizeBytes;
        }
        append_fmt(outBuf, outChars, &pos,
                   oric_tr(lang, STR_SUM_TAP_FMT),
                   report->entryCount,
                   (unsigned long long)totalPayload);
    } else if (report->mediaType == ORIC_MEDIA_DSK) {
        const wchar_t *cont = (report->dsk.containerType == ORIC_DSK_ORICDISK)
                                  ? L"ORICDISK"
                                  : L"MFM_DISK";
        append_fmt(outBuf, outChars, &pos,
                   oric_tr(lang, STR_SUM_DSK_FMT),
                   cont,
                   report->dsk.dosLabel,
                   report->dsk.diskName,
                   report->dsk.sides,
                   report->dsk.tracks,
                   report->dsk.sectorsPerTrack,
                   report->entryCount);
        if (report->dsk.totalDataSectors > 0) {
            append_fmt(outBuf, outChars, &pos,
                       oric_tr(lang, STR_SUM_DSK_FREE_FMT),
                       report->dsk.freeSectors,
                       report->dsk.totalDataSectors);
        }
    } else if (report->mediaType == ORIC_MEDIA_ROM) {
        append_fmt(outBuf, outChars, &pos,
                   oric_tr(lang, STR_SUM_ROM_PART1_FMT),
                   report->rom.authenticityLabel,
                   report->rom.modelName,
                   report->rom.versionLabel,
                   report->rom.crc32,
                   report->rom.sizeBytes,
                   report->rom.sizeBytes / 1024u,
                   report->rom.baseAddr);

        if (report->rom.charsetHint[0] != L'\0') {
            append_fmt(outBuf, outChars, &pos,
                       oric_tr(lang, STR_SUM_ROM_CHARSET_FMT),
                       report->rom.charsetHint);
        }

        if (report->rom.hasVectors) {
            append_fmt(outBuf, outChars, &pos,
                       oric_tr(lang, STR_SUM_ROM_VECTORS_FMT),
                       report->rom.vecNmi,
                       report->rom.vecReset,
                       report->rom.vecIrq);
        }
        append_fmt(outBuf, outChars, &pos,
                   oric_tr(lang, STR_SUM_ROM_STRINGS_FMT),
                   report->rom.detectedStrings);
    } else {
        append_fmt(outBuf, outChars, &pos,
                   oric_tr(lang, STR_SUM_UNKNOWN_FMT),
                   report->statusMessage);
    }

    if (selectedIndex >= 0 && (size_t)selectedIndex < report->entryCount &&
        report->mediaType != ORIC_MEDIA_ROM) {
        const OricFileEntry *e = &report->entries[selectedIndex];
        append_fmt(outBuf, outChars, &pos,
                   oric_tr(lang, STR_SUM_SEL_HEADER_FMT),
                   e->name, e->typeLabel, e->sizeBytes);
        if (e->hasStartEnd) {
            append_fmt(outBuf, outChars, &pos,
                       oric_tr(lang, STR_SUM_SEL_MEM_FMT),
                       e->startAddr, e->endAddr);
        }
        if (e->hasExec) {
            append_fmt(outBuf, outChars, &pos,
                       oric_tr(lang, STR_SUM_SEL_EXEC_FMT),
                       e->execAddr);
        }
        if (e->hasCrc32) {
            append_fmt(outBuf, outChars, &pos,
                       oric_tr(lang, STR_SUM_SEL_CRC_FMT),
                       e->crc32);
        }
        append_fmt(outBuf, outChars, &pos,
                   oric_tr(lang, STR_SUM_SEL_TAIL_FMT),
                   e->flagsLabel, e->extraDetail);
    }
}
