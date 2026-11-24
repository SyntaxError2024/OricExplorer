#include "oric_core.h"
#include <stdio.h>
#include <string.h>

static uint16_t tap_read_be16(const uint8_t *p) {
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static uint16_t tap_read_le16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static void tap_add_block_entry(const uint8_t *data, size_t size, OricMediaReport *report,
                                OricLang lang, int blockIndex, const char *rawName,
                                uint8_t fileType, uint8_t autorun,
                                uint16_t startAddr, uint16_t endAddr, bool hasValidRange,
                                uint32_t payloadOffset, uint32_t payloadSize,
                                uint32_t actualAvail, bool truncated) {
    OricFileEntry entry;
    memset(&entry, 0, sizeof(entry));
    entry.source = ORIC_SRC_TAP;
    entry.tapPayloadOffset = payloadOffset;
    entry.tapPayloadAvail = actualAvail;
    entry.tapFileType = fileType;
    entry.tapAutorun = autorun;

    if (rawName[0] != '\0') {
        oric_ascii_to_wide(rawName, 64, entry.name, 64);
    } else {
        swprintf(entry.name, 64, oric_tr(lang, STR_TAP_UNNAMED_FMT), blockIndex);
    }

    bool looksLikeScreen = hasValidRange &&
                            (startAddr == 0xA000u || startAddr == 0xBB80u ||
                             (fileType == 0x80 && (payloadSize == 8000u || payloadSize == 1120u ||
                              (startAddr >= 0x9800u && endAddr <= 0xBFDFu && payloadSize >= 1000u))));

    if (fileType == 0x00) {
        wcscpy(entry.typeLabel, L"BASIC");
        entry.isBasic = true;
    } else if (fileType == 0x80) {
        if (looksLikeScreen) {
            swprintf(entry.typeLabel, 32, L"%ls", oric_tr(lang, STR_TYPE_SCREEN_MEM));
            entry.isScreen = true;
        } else {
            swprintf(entry.typeLabel, 32, L"%ls", oric_tr(lang, STR_TYPE_BINARY_ML));
        }
    } else if (fileType == 0x40) {
        swprintf(entry.typeLabel, 32, L"%ls", oric_tr(lang, STR_TYPE_ARRAY));
    } else {
        swprintf(entry.typeLabel, 32, oric_tr(lang, STR_TYPE_HEX_FMT), fileType);
    }

    entry.sizeBytes = payloadSize;
    entry.startAddr = startAddr;
    entry.endAddr = endAddr;
    entry.hasStartEnd = hasValidRange;
    entry.isAutorun = (autorun != 0x00);
    if (actualAvail > 0 && (size_t)payloadOffset + (size_t)actualAvail <= size) {
        entry.crc32 = oric_crc32(data + payloadOffset, actualAvail);
        entry.hasCrc32 = true;
    }

    if (entry.isAutorun && truncated) {
        swprintf(entry.flagsLabel, 48, oric_tr(lang, STR_TAP_FLAG_AUTORUN_TRUNC_FMT), autorun);
    } else if (entry.isAutorun) {
        swprintf(entry.flagsLabel, 48, oric_tr(lang, STR_TAP_FLAG_AUTORUN_FMT), autorun);
    } else if (truncated) {
        swprintf(entry.flagsLabel, 48, L"%ls", oric_tr(lang, STR_TAP_FLAG_TRUNCATED));
    } else {
        swprintf(entry.flagsLabel, 48, L"%ls", oric_tr(lang, STR_TAP_FLAG_STANDARD));
    }

    swprintf(entry.extraDetail, 128,
             oric_tr(lang, STR_TAP_DETAIL_FMT),
             blockIndex, fileType, autorun, payloadSize);

    oric_report_add_entry(report, &entry);
}

static bool tap_scan_name(const uint8_t *data, size_t nameStart, size_t size,
                          char *outName, size_t outCap,
                          size_t *outAfterNul, size_t *outRawLen, bool *outPrintable) {
    size_t scanPos = nameStart;
    size_t rawLen = 0;
    size_t outLen = 0;
    bool foundNul = false;
    bool printable = true;

    while (scanPos < size) {
        uint8_t ch = data[scanPos++];
        if (ch == 0x00) {
            foundNul = true;
            break;
        }
        if (ch >= 127) {
            printable = false;
        }
        if (outName && outLen + 1 < outCap) {
            outName[outLen++] = (ch >= 32 && ch < 127) ? (char)ch : '?';
        }
        rawLen++;
        if (rawLen > 256u) {
            break;
        }
    }
    if (outName && outCap > 0) {
        outName[outLen] = '\0';
    }
    if (outAfterNul) *outAfterNul = scanPos;
    if (outRawLen) *outRawLen = rawLen;
    if (outPrintable) *outPrintable = printable;
    return foundNul;
}

static bool tap_has_sync_header_at(const uint8_t *data, size_t pos, size_t size, size_t minSync) {
    if (!data || pos >= size || data[pos] != 0x16) return false;
    size_t p = pos;
    size_t cnt = 0;
    while (p < size && data[p] == 0x16) {
        cnt++;
        p++;
    }
    if (cnt < minSync || p >= size || data[p] != 0x24) return false;
    p++;
    if (p + 9u > size) return false;
    return tap_scan_name(data, p + 9u, size, NULL, 0, NULL, NULL, NULL);
}

static size_t tap_find_next_sync(const uint8_t *data, size_t startPos, size_t size, size_t minSync) {
    size_t p = startPos;
    while (p < size) {
        if (data[p] == 0x16) {
            size_t syncPos = p;
            size_t cnt = 0;
            while (p < size && data[p] == 0x16) {
                cnt++;
                p++;
            }
            if (cnt >= minSync && p < size && data[p] == 0x24) {
                size_t after = p + 1u;
                if (after + 9u <= size &&
                    tap_scan_name(data, after + 9u, size, NULL, 0, NULL, NULL, NULL)) {
                    return syncPos;
                }
            }
        } else {
            p++;
        }
    }
    return size;
}

typedef struct {
    bool valid;
    int score;
    uint8_t fileType;
    uint8_t autorun;
    uint16_t startAddr;
    uint16_t endAddr;
    uint32_t payloadSize;
} TapHdrCand;

static bool tap_bytes_are_padding(const uint8_t *data, size_t from, size_t to) {
    for (size_t i = from; i < to; i++) {
        uint8_t b = data[i];
        if (b != 0x00 && b != 0x16 && b != 0xFF) {
            return false;
        }
    }
    return true;
}

static TapHdrCand tap_select_best_header(const uint8_t *data, size_t size,
                                         const uint8_t *hdr, size_t payloadOffset,
                                         size_t nextSync3) {
    TapHdrCand best;
    memset(&best, 0, sizeof(best));
    best.score = -100000;

    size_t avail = (payloadOffset < size) ? (size - payloadOffset) : 0u;

    for (int mode = 0; mode < 7; mode++) {
        uint8_t ftype = 0;
        uint8_t autorun = 0;
        uint16_t endAddr = 0;
        uint16_t startAddr = 0;
        int score = 0;

        if (mode == 0) {
            /* Standard ROM Oric : [0]=00/FF, [1]=type, [2]=auto, [3..4]=end BE, [5..6]=start BE */
            ftype = hdr[1];
            autorun = hdr[2];
            endAddr = tap_read_be16(hdr + 3);
            startAddr = tap_read_be16(hdr + 5);
            score = 40;
            if (hdr[0] == 0x00 || hdr[0] == 0xFF) score += 15;
            if (hdr[7] == 0x00 && hdr[8] == 0x00) score += 5;
        } else if (mode == 1) {
            /* En-tête décalé +1 : [0..1]=00 00 / FF FF (ou 24), [2]=type, [3]=auto, [4..5]=end BE, [6..7]=start BE */
            if (!((hdr[0] == 0x00 && hdr[1] == 0x00) ||
                  (hdr[0] == 0xFF && hdr[1] == 0xFF) ||
                  hdr[0] == 0x24)) {
                continue;
            }
            ftype = hdr[2];
            autorun = hdr[3];
            endAddr = tap_read_be16(hdr + 4);
            startAddr = tap_read_be16(hdr + 6);
            score = 25;
            if (hdr[8] == 0x00) score += 5;
        } else if (mode == 2) {
            /* En-tête compact -1 : [0]=type, [1]=auto, [2..3]=end BE, [4..5]=start BE, [7..8]=00 */
            if (!((hdr[0] == 0x00 || hdr[0] == 0x40 || hdr[0] == 0x80) &&
                  hdr[7] == 0x00 && hdr[8] == 0x00)) {
                continue;
            }
            ftype = hdr[0];
            autorun = hdr[1];
            endAddr = tap_read_be16(hdr + 2);
            startAddr = tap_read_be16(hdr + 4);
            score = 20;
        } else if (mode == 3) {
            /* Variante non standard Little-Endian sur [3..4] / [5..6] */
            ftype = hdr[1];
            autorun = hdr[2];
            endAddr = tap_read_le16(hdr + 3);
            startAddr = tap_read_le16(hdr + 5);
            score = 10;
            if (hdr[0] == 0x00 || hdr[0] == 0xFF) score += 10;
        } else if (mode == 4) {
            /* Variante non standard décalée +1 Little-Endian sur [4..5] / [6..7] */
            if (!((hdr[0] == 0x00 && hdr[1] == 0x00) ||
                  (hdr[0] == 0xFF && hdr[1] == 0xFF))) {
                continue;
            }
            ftype = hdr[2];
            autorun = hdr[3];
            endAddr = tap_read_le16(hdr + 4);
            startAddr = tap_read_le16(hdr + 6);
            score = 5;
        } else if (mode == 5) {
            /* Variante start/end inversés BE sur [3..4]=start, [5..6]=end */
            ftype = hdr[1];
            autorun = hdr[2];
            startAddr = tap_read_be16(hdr + 3);
            endAddr = tap_read_be16(hdr + 5);
            if (startAddr < 0x0200u) continue;
            score = 8;
        } else {
            /* Variante start/end inversés LE sur [3..4]=start, [5..6]=end */
            ftype = hdr[1];
            autorun = hdr[2];
            startAddr = tap_read_le16(hdr + 3);
            endAddr = tap_read_le16(hdr + 5);
            if (startAddr < 0x0200u) continue;
            score = 4;
        }

        if (endAddr < startAddr) {
            continue;
        }

        uint32_t len = (uint32_t)(endAddr - startAddr + 1u);
        size_t endPos = payloadOffset + (size_t)len;

        if (ftype == 0x00 || ftype == 0x40 || ftype == 0x80) {
            score += 15;
        } else if ((ftype & 0x3Fu) == 0) {
            score += 5;
        } else {
            score -= 10;
        }

        if (autorun == 0x00 || autorun == 0xC7 || autorun == 0x80) {
            score += 5;
        }

        if (startAddr >= 0x0400u) {
            score += 20;
        } else if (startAddr >= 0x0200u) {
            score += 5;
        } else {
            score -= 25; /* Page zéro ou pile 6502 ($01xx) très improbable */
        }

        if (endPos == size) {
            score += 120;
        } else if (endPos < size) {
            if ((size - endPos) <= 512u && tap_bytes_are_padding(data, endPos, size)) {
                score += 95;
            } else if (tap_has_sync_header_at(data, endPos, size, 2u)) {
                score += 120;
            } else {
                size_t padPos = endPos;
                while (padPos < size && (padPos - endPos) < 256u &&
                       (data[padPos] == 0x00 || data[padPos] == 0xFF)) {
                    padPos++;
                }
                if (padPos > endPos && padPos < size &&
                    tap_has_sync_header_at(data, padPos, size, 2u)) {
                    score += 95;
                } else if (padPos > endPos && padPos < size &&
                           tap_has_sync_header_at(data, padPos, size, 1u)) {
                    score += 75;
                } else if (tap_has_sync_header_at(data, endPos, size, 1u)) {
                    score += 75;
                } else if (nextSync3 < size && endPos < nextSync3) {
                    if (nextSync3 - endPos <= 16u) score += 40;
                    else score -= 35;
                } else if (nextSync3 < size && endPos > nextSync3) {
                    score -= 50;
                } else {
                    score -= 20;
                }
            }
        } else {
            if (len > 49152u && avail < len) {
                score -= 90;
            } else if (endPos - size <= 256u && nextSync3 >= size) {
                score -= 10;
            } else {
                score -= 70;
            }
        }

        if (!best.valid || score > best.score) {
            best.valid = true;
            best.score = score;
            best.fileType = ftype;
            best.autorun = autorun;
            best.startAddr = startAddr;
            best.endAddr = endAddr;
            best.payloadSize = len;
        }
    }

    return best;
}

bool oric_parse_tap(const uint8_t *data, size_t size, OricMediaReport *report) {
    size_t pos = 0;
    size_t expectedNextPos = 0;
    int blockIndex = 0;
    OricLang lang = report ? report->lang : ORIC_LANG_EN;

    if (!data || !report || size < 12) {
        if (report) {
            swprintf(report->statusMessage, 256,
                     oric_tr(lang, STR_TAP_TOO_SHORT), size);
        }
        return false;
    }

    report->mediaType = ORIC_MEDIA_TAP;

    while (pos < size) {
        bool directSyncAtBoundary = (pos == expectedNextPos && data[pos] == 0x24);
        if (data[pos] != 0x16 && !directSyncAtBoundary) {
            pos++;
            continue;
        }

        size_t syncStartPos = pos;
        size_t syncCount = 0;
        while (pos < size && data[pos] == 0x16) {
            syncCount++;
            pos++;
        }
        if (pos >= size || data[pos] != 0x24) {
            continue;
        }
        pos++; /* Consomme 0x24 */
        size_t afterSyncPos = pos;

        if (afterSyncPos + 9u > size) {
            break;
        }

        const uint8_t *hdr = data + afterSyncPos;
        char rawName[64];
        size_t scanPos = 0;
        size_t rawNameLen = 0;
        bool namePrintable = true;

        if (!tap_scan_name(data, afterSyncPos + 9u, size,
                           rawName, sizeof(rawName),
                           &scanPos, &rawNameLen, &namePrintable)) {
            pos = afterSyncPos;
            continue;
        }

        size_t payloadOffset = scanPos;
        size_t avail = (payloadOffset < size) ? (size - payloadOffset) : 0u;
        size_t nextSync3 = tap_find_next_sync(data, payloadOffset, size, 3u);

        TapHdrCand cand = tap_select_best_header(data, size, hdr, payloadOffset, nextSync3);

        if (directSyncAtBoundary && !(cand.valid && cand.score >= 100)) {
            pos = afterSyncPos;
            continue;
        }

        bool leadingPadStart = (blockIndex == 0 && tap_bytes_are_padding(data, 0, syncStartPos));
        bool atBoundary = (syncStartPos == 0) || (syncStartPos == expectedNextPos) || leadingPadStart;
        if (syncCount < 3u && !directSyncAtBoundary) {
            bool plausibleShort = false;
            if (syncCount >= 2u && (blockIndex == 0 || atBoundary)) {
                plausibleShort = true;
            } else if (syncCount == 1u && (syncStartPos == 0 || (blockIndex > 0 && syncStartPos == expectedNextPos))) {
                plausibleShort = true;
            } else if (syncCount == 1u && leadingPadStart && cand.valid && cand.score >= 40) {
                plausibleShort = true;
            } else if (cand.valid && cand.score >= 120 && namePrintable && rawNameLen <= 48u) {
                plausibleShort = true;
            }
            if (!plausibleShort) {
                pos = afterSyncPos;
                continue;
            }
        }

        uint8_t fileType = hdr[1];
        uint8_t autorun = hdr[2];
        uint16_t startAddr = 0;
        uint16_t endAddr = 0;
        bool hasValidRange = false;
        uint32_t payloadSize = 0;
        uint32_t actualAvail = 0;
        bool truncated = false;

        if (cand.valid && (cand.score > 0 || (cand.score > -30 && cand.payloadSize <= avail + 256u))) {
            fileType = cand.fileType;
            autorun = cand.autorun;
            startAddr = cand.startAddr;
            endAddr = cand.endAddr;
            hasValidRange = true;
            payloadSize = cand.payloadSize;

            if (payloadSize > avail) {
                truncated = true;
                actualAvail = (uint32_t)avail;
                pos = size;
            } else {
                actualAvail = payloadSize;
                pos = payloadOffset + payloadSize;
            }
        } else {
            /* Bloc personnalisé sans plage BE/LE cohérente : lire jusqu'au prochain marqueur 0x16x3+0x24 */
            size_t span = (nextSync3 > payloadOffset) ? (nextSync3 - payloadOffset) : 0u;
            payloadSize = (uint32_t)span;
            actualAvail = (uint32_t)span;
            pos = nextSync3;
        }

        expectedNextPos = pos;
        blockIndex++;
        tap_add_block_entry(data, size, report, lang, blockIndex, rawName,
                            fileType, autorun, startAddr, endAddr, hasValidRange,
                            (uint32_t)payloadOffset, payloadSize, actualAvail, truncated);
    }

    /* Repli si aucun leader 0x16 trouvé mais le flux démarre directement par 0x24 */
    if (report->entryCount == 0 && data[0] == 0x24 && size >= 12u) {
        const uint8_t *hdr = data + 1u;
        char rawName[64];
        size_t scanPos = 0;
        size_t rawNameLen = 0;
        bool namePrintable = true;
        if (tap_scan_name(data, 10u, size, rawName, sizeof(rawName),
                          &scanPos, &rawNameLen, &namePrintable)) {
            size_t nextSync3 = tap_find_next_sync(data, scanPos, size, 3u);
            TapHdrCand cand = tap_select_best_header(data, size, hdr, scanPos, nextSync3);
            if (cand.valid && cand.score >= 80) {
                size_t avail = (scanPos < size) ? (size - scanPos) : 0u;
                bool truncated = (cand.payloadSize > avail);
                uint32_t actualAvail = truncated ? (uint32_t)avail : cand.payloadSize;
                tap_add_block_entry(data, size, report, lang, 1, rawName,
                                    cand.fileType, cand.autorun,
                                    cand.startAddr, cand.endAddr, true,
                                    (uint32_t)scanPos, cand.payloadSize, actualAvail, truncated);
            }
        }
    }

    if (report->entryCount == 0) {
        report->parseSuccess = false;
        swprintf(report->statusMessage, 256, L"%ls", oric_tr(lang, STR_TAP_NO_VALID_BLOCK));
        return false;
    }

    report->parseSuccess = true;
    swprintf(report->statusMessage, 256,
             oric_tr(lang, STR_TAP_OK_FMT),
             report->entryCount);
    return true;
}
