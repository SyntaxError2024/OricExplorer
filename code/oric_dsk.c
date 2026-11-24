#include "oric_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    const uint8_t *data;
    size_t size;
    OricDskContainer container;
    uint32_t sides;
    uint32_t tracks;
    uint32_t sectorsPerTrack;
    uint32_t geometryType;
    size_t mfmTrackStride;
} DskContext;

static uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static bool mfm_track_has_idam(const uint8_t *trkBuf, size_t trkLen,
                               uint8_t expectTrack, uint8_t expectSide) {
    if (!trkBuf || trkLen < 8) return false;
    for (size_t pass = 0; pass < 2; pass++) {
        size_t minI = (pass == 0) ? 2u : 1u;
        for (size_t i = minI; i + 5 < trkLen; i++) {
            bool idam = (pass == 0)
                            ? (trkBuf[i] == 0xFE && trkBuf[i - 1] == 0xA1 && trkBuf[i - 2] == 0xA1)
                            : (trkBuf[i] == 0xFE && trkBuf[i - 1] == 0xA1);
            if (idam && trkBuf[i + 1] == expectTrack && trkBuf[i + 2] == expectSide &&
                trkBuf[i + 3] >= 1 && trkBuf[i + 3] <= ORIC_MAX_MAP_SECTORS &&
                trkBuf[i + 4] <= 3) {
                return true;
            }
        }
    }
    return false;
}

static bool dsk_init_context(const uint8_t *data, size_t size, DskContext *ctx) {
    if (!data || !ctx || size < 256) return false;
    memset(ctx, 0, sizeof(*ctx));
    ctx->data = data;
    ctx->size = size;

    if (memcmp(data, "ORICDISK", 8) == 0) {
        ctx->container = ORIC_DSK_ORICDISK;
        ctx->sides = read_le32(data + 8);
        ctx->tracks = read_le32(data + 12);
        ctx->sectorsPerTrack = read_le32(data + 16);
        if (ctx->sides == 0 || ctx->sides > 2) ctx->sides = 1;
        if (ctx->tracks == 0 || ctx->tracks > ORIC_MAX_MAP_TRACKS) ctx->tracks = 42;
        if (ctx->sectorsPerTrack == 0 || ctx->sectorsPerTrack > ORIC_MAX_MAP_SECTORS) ctx->sectorsPerTrack = 17;
        return true;
    }

    if (memcmp(data, "MFM_DISK", 8) == 0) {
        ctx->container = ORIC_DSK_MFM_DISK;
        ctx->sides = read_le32(data + 8);
        ctx->tracks = read_le32(data + 12);
        ctx->geometryType = read_le32(data + 16);
        ctx->sectorsPerTrack = 17;
        if (ctx->sides == 0 || ctx->sides > 2) ctx->sides = 1;
        if (ctx->tracks == 0 || ctx->tracks > ORIC_MAX_MAP_TRACKS) ctx->tracks = 42;

        ctx->mfmTrackStride = 6400u;
        bool strideMatched = false;
        size_t totalTracks = (size_t)ctx->sides * (size_t)ctx->tracks;
        if (totalTracks > 0 && size > 256u) {
            size_t rem = size - 256u;
            if (rem % totalTracks == 0) {
                size_t cand = rem / totalTracks;
                if (cand >= 5000u && cand <= 8192u) {
                    ctx->mfmTrackStride = cand;
                    strideMatched = true;
                }
            }
        }

        if (!strideMatched && ctx->tracks > 1 && size > 256u + 5000u) {
            static const size_t kCandidateStrides[] = {
                6400u, 6250u, 6144u, 6300u, 6500u, 6600u, 5120u
            };
            static const uint8_t kProbeTrks[] = { 1, 2, 3, 5, 10, 15, 20, 30 };
            size_t trkMul = (ctx->geometryType == 2u) ? (size_t)ctx->sides : 1u;
            int bestScore = 0;
            size_t bestStride = 6400u;

            for (size_t k = 0; k < sizeof(kCandidateStrides) / sizeof(kCandidateStrides[0]); k++) {
                size_t cand = kCandidateStrides[k];
                int score = 0;
                for (size_t p = 0; p < sizeof(kProbeTrks) / sizeof(kProbeTrks[0]); p++) {
                    uint8_t pt = kProbeTrks[p];
                    if (pt >= ctx->tracks) continue;
                    size_t off = 256u + (size_t)pt * trkMul * cand;
                    if (off + 128u <= size) {
                        size_t avail = size - off;
                        if (avail > cand) avail = cand;
                        if (mfm_track_has_idam(data + off, avail, pt, 0)) {
                            score++;
                        }
                    }
                }
                if (score > bestScore) {
                    bestScore = score;
                    bestStride = cand;
                }
            }

            if (bestScore == 0 && ctx->sides == 2 && ctx->geometryType != 2u) {
                for (size_t k = 0; k < sizeof(kCandidateStrides) / sizeof(kCandidateStrides[0]); k++) {
                    size_t cand = kCandidateStrides[k];
                    int score = 0;
                    for (size_t p = 0; p < sizeof(kProbeTrks) / sizeof(kProbeTrks[0]); p++) {
                        uint8_t pt = kProbeTrks[p];
                        if (pt >= ctx->tracks) continue;
                        size_t off = 256u + (size_t)pt * 2u * cand;
                        if (off + 128u <= size) {
                            size_t avail = size - off;
                            if (avail > cand) avail = cand;
                            if (mfm_track_has_idam(data + off, avail, pt, 0)) {
                                score++;
                            }
                        }
                    }
                    if (score > bestScore) {
                        bestScore = score;
                        bestStride = cand;
                    }
                }
            }

            if (bestScore > 0) {
                ctx->mfmTrackStride = bestStride;
            }
        }

        /* Détecter si l'image MFM contient plus de 17 secteurs/piste (ex: 18 ou 19) */
        size_t probeTracks = (totalTracks > 0 && totalTracks < 84u) ? totalTracks : 84u;
        for (size_t pt = 0; pt < probeTracks; pt++) {
            size_t trkOff = 256u + pt * ctx->mfmTrackStride;
            if (trkOff + 8 > size) break;
            size_t avail = size - trkOff;
            if (avail > ctx->mfmTrackStride) avail = ctx->mfmTrackStride;
            const uint8_t *trkPtr = data + trkOff;
            for (size_t i = 2; i + 5 < avail; i++) {
                if (trkPtr[i] == 0xFE && trkPtr[i - 1] == 0xA1 && trkPtr[i - 2] == 0xA1) {
                    uint8_t idTrk = trkPtr[i + 1];
                    uint8_t idSide = trkPtr[i + 2];
                    uint8_t secNo = trkPtr[i + 3];
                    uint8_t szCode = trkPtr[i + 4];
                    if (idTrk < ctx->tracks && idSide <= 1 && szCode <= 3 &&
                        secNo > ctx->sectorsPerTrack && secNo <= ORIC_MAX_MAP_SECTORS) {
                        ctx->sectorsPerTrack = secNo;
                    }
                }
            }
        }
        return true;
    }

    return false;
}

static bool mfm_extract_sector_from_track(const uint8_t *trkBuf, size_t trkLen,
                                          uint8_t targetTrack, uint8_t targetSide,
                                          uint8_t targetSector, bool strictSide,
                                          uint8_t *out256) {
    if (!trkBuf || trkLen < 16) return false;

    for (int syncPass = 0; syncPass < 2; syncPass++) {
        size_t minI = (syncPass == 0) ? 2u : 1u;
        for (size_t i = minI; i + 6 < trkLen; i++) {
            bool idamMatch = (syncPass == 0)
                                 ? (trkBuf[i] == 0xFE && trkBuf[i - 1] == 0xA1 && trkBuf[i - 2] == 0xA1)
                                 : (trkBuf[i] == 0xFE && trkBuf[i - 1] == 0xA1);
            if (!idamMatch) continue;

            uint8_t idTrack = trkBuf[i + 1];
            uint8_t idSide = trkBuf[i + 2];
            uint8_t idSector = trkBuf[i + 3];
            uint8_t idSizeCode = trkBuf[i + 4];

            if (idSizeCode > 3) continue;
            if (idTrack != targetTrack || idSector != targetSector) continue;
            if (strictSide) {
                if (idSide != targetSide) continue;
            } else if (idSide > 1) {
                continue;
            }

            size_t secSize = (size_t)128u << idSizeCode;
            size_t searchEnd = i + 96u;
            if (searchEnd > trkLen) searchEnd = trkLen;

            /* Passe 1 : marque DAM précédée de 0xA1 */
            for (size_t j = i + 7; j + 1 < searchEnd; j++) {
                if (j >= i + 9 && trkBuf[j] == 0xFE && trkBuf[j - 1] == 0xA1 && trkBuf[j - 2] == 0xA1) {
                    break;
                }
                if ((trkBuf[j] == 0xFB || trkBuf[j] == 0xF8) && trkBuf[j - 1] == 0xA1) {
                    size_t dataStart = j + 1;
                    if (secSize >= 256u && dataStart + 256u <= trkLen) {
                        memcpy(out256, trkBuf + dataStart, 256u);
                        return true;
                    }
                    if (secSize == 128u && dataStart + 128u <= trkLen) {
                        memset(out256, 0, 256u);
                        memcpy(out256, trkBuf + dataStart, 128u);
                        return true;
                    }
                }
            }

            /* Passe 2 : marque DAM sans 0xA1 (images converties non standard, comme Oricutron) */
            for (size_t j = i + 7; j + 1 < searchEnd; j++) {
                if (j >= i + 9 && trkBuf[j] == 0xFE && trkBuf[j - 1] == 0xA1 && trkBuf[j - 2] == 0xA1) {
                    break;
                }
                if ((trkBuf[j] == 0xFB || trkBuf[j] == 0xF8) &&
                    (trkBuf[j - 1] == 0x00 || trkBuf[j - 1] == 0x4E)) {
                    size_t dataStart = j + 1;
                    if (secSize >= 256u && dataStart + 256u <= trkLen) {
                        memcpy(out256, trkBuf + dataStart, 256u);
                        return true;
                    }
                    if (secSize == 128u && dataStart + 128u <= trkLen) {
                        memset(out256, 0, 256u);
                        memcpy(out256, trkBuf + dataStart, 128u);
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

static bool dsk_read_sector(const DskContext *ctx, uint32_t side, uint32_t track,
                            uint32_t sector, uint8_t *out256) {
    if (!ctx || !ctx->data || sector == 0 || side >= ctx->sides || track >= ctx->tracks) {
        return false;
    }

    if (ctx->container == ORIC_DSK_ORICDISK) {
        if (ctx->sectorsPerTrack == 0 || sector > ctx->sectorsPerTrack) {
            return false;
        }
        size_t lba = ((size_t)side * ctx->tracks + track) * ctx->sectorsPerTrack + (sector - 1u);
        size_t offset = 256u + lba * 256u;
        if (offset + 256u > ctx->size) {
            return false;
        }
        memcpy(out256, ctx->data + offset, 256u);
        return true;
    }

    if (ctx->container == ORIC_DSK_MFM_DISK) {
        const size_t trackStride = (ctx->mfmTrackStride > 0) ? ctx->mfmTrackStride : 6400u;
        size_t trackIndex = (ctx->geometryType == 2u)
                                ? ((size_t)track * ctx->sides + side)
                                : ((size_t)side * ctx->tracks + track);

        size_t offset = 256u + trackIndex * trackStride;
        if (offset + 256u <= ctx->size) {
            size_t avail = ctx->size - offset;
            if (avail > trackStride) avail = trackStride;
            if (mfm_extract_sector_from_track(ctx->data + offset, avail,
                                              (uint8_t)track, (uint8_t)side,
                                              (uint8_t)sector, true, out256)) {
                return true;
            }
            if (mfm_extract_sector_from_track(ctx->data + offset, avail,
                                              (uint8_t)track, (uint8_t)side,
                                              (uint8_t)sector, false, out256)) {
                return true;
            }
        }

        if (ctx->sides > 1) {
            size_t altIndex = (ctx->geometryType == 2u)
                                  ? ((size_t)side * ctx->tracks + track)
                                  : ((size_t)track * ctx->sides + side);
            size_t altOffset = 256u + altIndex * trackStride;
            if (altOffset != offset && altOffset + 256u <= ctx->size) {
                size_t avail = ctx->size - altOffset;
                if (avail > trackStride) avail = trackStride;
                if (mfm_extract_sector_from_track(ctx->data + altOffset, avail,
                                                  (uint8_t)track, (uint8_t)side,
                                                  (uint8_t)sector, true, out256)) {
                    return true;
                }
            }
        }
    }

    return false;
}

static void trim_ascii_copy(const uint8_t *src, size_t len, char *dst, size_t dstSize) {
    size_t out = 0;
    for (size_t i = 0; i < len && out + 1 < dstSize; i++) {
        uint8_t c = src[i];
        dst[out++] = (c >= 32 && c < 127) ? (char)c : ' ';
    }
    dst[out] = '\0';
    while (out > 0 && dst[out - 1] == ' ') {
        dst[--out] = '\0';
    }
}

static bool is_printable_padded_field(const uint8_t *src, size_t len) {
    bool seenNul = false;
    bool hasChar = false;
    for (size_t i = 0; i < len; i++) {
        uint8_t c = src[i];
        if (c == 0x00) {
            seenNul = true;
            continue;
        }
        if (c == ' ') {
            continue;
        }
        if (seenNul || c < 32 || c >= 127) {
            return false;
        }
        hasChar = true;
    }
    return hasChar;
}

static bool eq_icase_ext(const char *ext, const char *target) {
    for (int i = 0; i < 4; i++) {
        if (toupper((unsigned char)ext[i]) != toupper((unsigned char)target[i])) {
            return false;
        }
        if (ext[i] == '\0') {
            return true;
        }
    }
    return true;
}

static bool buffer_contains_ascii(const uint8_t *buf, size_t len, const char *needle) {
    size_t nlen = strlen(needle);
    if (nlen == 0 || len < nlen) return false;
    for (size_t i = 0; i <= len - nlen; i++) {
        size_t j = 0;
        while (j < nlen && toupper(buf[i + j]) == toupper((unsigned char)needle[j])) {
            j++;
        }
        if (j == nlen) return true;
    }
    return false;
}

static void map_mark_sector(uint8_t map[ORIC_MAX_MAP_SIDES][ORIC_MAX_MAP_TRACKS][ORIC_MAX_MAP_SECTORS],
                            uint32_t side, uint32_t track, uint32_t sector, uint8_t val) {
    if (side < ORIC_MAX_MAP_SIDES && track < ORIC_MAX_MAP_TRACKS &&
        sector >= 1 && sector <= ORIC_MAX_MAP_SECTORS) {
        map[side][track][sector - 1u] = val;
    }
}

static void classify_dos_entry(const char *ext, uint8_t descType, uint8_t descFlag,
                               OricLang lang, OricFileEntry *entry) {
    const wchar_t *label = oric_tr(lang, STR_TYPE_SEDORIC_FILE);
    entry->isBasic = false;
    entry->isScreen = false;

    if (eq_icase_ext(ext, "BAS")) {
        label = L"BASIC";
        entry->isBasic = true;
    } else if (eq_icase_ext(ext, "COM") || eq_icase_ext(ext, "BIN")) {
        label = oric_tr(lang, STR_TYPE_BINARY_ML);
    } else if (eq_icase_ext(ext, "HIR") || eq_icase_ext(ext, "HRS")) {
        label = oric_tr(lang, STR_TYPE_HIRES);
        entry->isScreen = true;
    } else if (eq_icase_ext(ext, "TXT") || eq_icase_ext(ext, "DOC") || eq_icase_ext(ext, "SCR")) {
        label = oric_tr(lang, STR_TYPE_TEXT);
        if (entry->hasStartEnd && entry->startAddr == 0xBB80u) {
            entry->isScreen = true;
        }
    } else if (eq_icase_ext(ext, "DAT") || eq_icase_ext(ext, "SEQ")) {
        label = oric_tr(lang, STR_TYPE_DATA);
    } else if (eq_icase_ext(ext, "SYS")) {
        label = oric_tr(lang, STR_TYPE_SYSTEM);
    } else if (descType == 0x40) {
        label = oric_tr(lang, STR_TYPE_DATA);
    } else if ((descType == 0x00 || (entry->source == ORIC_SRC_SEDORIC && (descType & 0xC0u) == 0)) &&
               (descFlag & 0x80u) == 0) {
        label = L"BASIC";
        entry->isBasic = true;
    } else if ((descType & 0x80u) != 0 || (descFlag & 0x80u) != 0) {
        if (entry->hasStartEnd && (entry->startAddr == 0xA000u || entry->startAddr == 0xBB80u)) {
            label = oric_tr(lang, STR_TYPE_SCREEN_MEM);
            entry->isScreen = true;
        } else {
            label = oric_tr(lang, STR_TYPE_BINARY_ML);
        }
    }

    if (entry->hasStartEnd && (entry->startAddr == 0xA000u || entry->startAddr == 0xBB80u) &&
        entry->sizeBytes >= 1000u && !entry->isBasic) {
        entry->isScreen = true;
    }

    swprintf(entry->typeLabel, 32, L"%ls", label);
}

static void sedoric_walk_sectors(const DskContext *ctx, const OricFileEntry *entry,
                                 uint8_t map[ORIC_MAX_MAP_SIDES][ORIC_MAX_MAP_TRACKS][ORIC_MAX_MAP_SECTORS],
                                 uint8_t sectorVal,
                                 uint8_t *outBuf, size_t maxBytes,
                                 size_t *outCollected, size_t *outDataSectors) {
    uint8_t curSide = entry->descSide;
    uint8_t curTrack = entry->descTrack;
    uint8_t curSector = entry->descSector;
    uint8_t visitedD[64][3];
    size_t dCount = 0;
    size_t collected = 0;
    size_t dataSectors = 0;

    while (curSector != 0 && curTrack != 0xFF && dCount < 64) {
        bool seen = false;
        for (size_t v = 0; v < dCount; v++) {
            if (visitedD[v][0] == curSide && visitedD[v][1] == curTrack && visitedD[v][2] == curSector) {
                seen = true;
                break;
            }
        }
        if (seen) break;
        visitedD[dCount][0] = curSide;
        visitedD[dCount][1] = curTrack;
        visitedD[dCount][2] = curSector;
        dCount++;

        uint8_t desc[256];
        if (!dsk_read_sector(ctx, curSide, curTrack, curSector, desc)) {
            break;
        }
        if (map) {
            map_mark_sector(map, curSide, curTrack, curSector, sectorVal);
        }

        /* Premier descripteur: paires (piste,secteur) à partir de l'offset 12 ($C10C).
           Descripteurs suivants: à partir de l'offset 2 ($C102), sauf descripteur joint (#FF). */
        size_t startOff = 2u;
        if (dCount == 1u) {
            startOff = 12u;
        } else if (desc[2] == 0xFF && desc[12] != 0xFF && desc[13] >= 1u &&
                   desc[13] <= ctx->sectorsPerTrack) {
            startOff = 12u;
        }

        for (size_t off = startOff; off + 1 < 256; off += 2) {
            uint8_t t = desc[off];
            uint8_t s = desc[off + 1];
            if (t == 0xFF || s == 0x00) {
                break;
            }
            uint32_t dSide = (t & 0x80u) ? 1u : 0u;
            uint32_t dTrack = (uint32_t)(t & 0x7Fu);
            uint32_t dSec = (uint32_t)s;

            if (dSide >= ctx->sides || dTrack >= ctx->tracks || dSec > ctx->sectorsPerTrack) {
                continue;
            }

            if (map) {
                map_mark_sector(map, dSide, dTrack, dSec, sectorVal);
            }

            dataSectors++;

            if (outBuf && collected < maxBytes) {
                uint8_t secData[256];
                if (dsk_read_sector(ctx, dSide, dTrack, dSec, secData)) {
                    size_t take = 256u;
                    if (collected + take > maxBytes) {
                        take = maxBytes - collected;
                    }
                    memcpy(outBuf + collected, secData, take);
                    collected += take;
                }
            }
        }

        uint8_t nt = desc[0];
        uint8_t ns = desc[1];
        if (nt == 0xFF || ns == 0x00) {
            break;
        }
        curSide = (nt & 0x80u) ? 1u : 0u;
        curTrack = (uint8_t)(nt & 0x7Fu);
        curSector = ns;
    }

    if (outCollected) {
        *outCollected = collected;
    }
    if (outDataSectors) {
        *outDataSectors = dataSectors;
    }
}

static bool detect_stratsed_signature(const DskContext *ctx) {
    if (ctx->tracks >= 78) {
        return true;
    }
    uint8_t sec[256];
    for (uint32_t s = 1; s <= 4; s++) {
        if (dsk_read_sector(ctx, 0, 0, s, sec)) {
            if (buffer_contains_ascii(sec, 256, "STRATSED") ||
                buffer_contains_ascii(sec, 256, "TELESTRAT")) {
                return true;
            }
        }
    }
    if (ctx->tracks > 20 && dsk_read_sector(ctx, 0, 20, 2, sec)) {
        if (buffer_contains_ascii(sec, 256, "STRATSED")) {
            return true;
        }
    }
    return false;
}

static bool detect_ftdos_boot_hint(const DskContext *ctx) {
    uint8_t boot[256];
    for (uint32_t s = 1; s <= 3; s++) {
        if (dsk_read_sector(ctx, 0, 0, s, boot) &&
            (buffer_contains_ascii(boot, 256, "FT-DOS") ||
             buffer_contains_ascii(boot, 256, "FTDOS") ||
             buffer_contains_ascii(boot, 256, "JASMIN"))) {
            return true;
        }
    }
    return false;
}

static bool detect_oricdos_boot_hint(const DskContext *ctx) {
    uint8_t sec[256];
    for (uint32_t s = 1; s <= 4; s++) {
        if (dsk_read_sector(ctx, 0, 0, s, sec) &&
            (buffer_contains_ascii(sec, 256, "ORICDOS") ||
             buffer_contains_ascii(sec, 256, "BOOTUP") ||
             buffer_contains_ascii(sec, 256, "TANGERINE"))) {
            return true;
        }
    }
    return false;
}

static bool ftdos_entry_valid(const DskContext *ctx, const uint8_t *e) {
    if (e[0] == 0x00 || e[0] == 0xFF) return false;
    if (!is_printable_padded_field(e, 16)) return false;
    uint8_t startT = e[16];
    uint8_t startS = e[17];
    uint8_t secCnt = e[18];
    uint8_t side = (startT & 0x80u) ? 1u : 0u;
    if (side >= ctx->sides) return false;
    if ((startT & 0x7Fu) >= ctx->tracks || startS == 0 || startS > ctx->sectorsPerTrack || secCnt == 0) {
        return false;
    }
    return true;
}

static bool try_parse_ftdos(const DskContext *ctx, OricMediaReport *report, OricLang lang) {
    if (ctx->tracks <= 20) return false;
    uint8_t sec1[256], sec2[256];
    if (!dsk_read_sector(ctx, 0, 20, 1, sec1) || !dsk_read_sector(ctx, 0, 20, 2, sec2)) {
        return false;
    }

    bool ftdosHint = detect_ftdos_boot_hint(ctx);
    if (!ftdosHint) {
        int validLook = 0;
        for (uint32_t s = 2; s <= 4 && validLook == 0; s++) {
            uint8_t probe[256];
            if (!dsk_read_sector(ctx, 0, 20, s, probe)) continue;
            for (size_t i = 0; i + 20 <= 256; i += 20) {
                if (ftdos_entry_valid(ctx, probe + i)) {
                    validLook++;
                }
            }
        }
        if (validLook == 0) return false;
    }

    report->dsk.dosType = ORIC_DOS_FTDOS;
    wcscpy(report->dsk.dosLabel, L"FT-DOS (Jasmin)");
    map_mark_sector(report->dsk.sectorMap, 0, 20, 1, ORIC_SECTOR_SYSTEM);
    map_mark_sector(report->dsk.sectorMap, 0, 20, 2, ORIC_SECTOR_SYSTEM);
    map_mark_sector(report->dsk.sectorMap, 0, 20, 3, ORIC_SECTOR_SYSTEM);

    for (uint32_t s = 2; s <= ctx->sectorsPerTrack; s++) {
        uint8_t cat[256];
        if (!dsk_read_sector(ctx, 0, 20, s, cat)) break;

        /* Ne pas confondre la bitmap (secteur 2) avec une table de catalogue */
        int sectorEntries = 0;
        for (size_t off = 0; off + 20 <= 256; off += 20) {
            if (ftdos_entry_valid(ctx, cat + off)) {
                sectorEntries++;
            }
        }
        if (s <= 3 && sectorEntries == 0) continue;
        if (sectorEntries == 0) continue;

        map_mark_sector(report->dsk.sectorMap, 0, 20, s, ORIC_SECTOR_SYSTEM);

        for (size_t off = 0; off + 20 <= 256; off += 20) {
            const uint8_t *e = cat + off;
            if (!ftdos_entry_valid(ctx, e)) continue;

            char fname[20];
            trim_ascii_copy(e, 16, fname, sizeof(fname));
            if (fname[0] == '\0') continue;

            uint8_t startT = e[16];
            uint8_t startS = e[17];
            uint16_t secCnt = (uint16_t)e[18];
            uint8_t ftype = e[19];

            OricFileEntry entry;
            memset(&entry, 0, sizeof(entry));
            entry.source = ORIC_SRC_FTDOS;
            entry.descSide = (startT & 0x80u) ? 1u : 0u;
            entry.descTrack = (uint8_t)(startT & 0x7Fu);
            entry.descSector = startS;
            entry.sectorCount = secCnt;
            entry.sizeBytes = (uint32_t)secCnt * 256u;
            entry.dosFileType = ftype;
            oric_ascii_to_wide(fname, sizeof(fname), entry.name, 64);

            const char *dot = strrchr(fname, '.');
            const char *ext = dot ? (dot + 1) : "";
            classify_dos_entry(ext, ftype, 0, lang, &entry);
            swprintf(entry.flagsLabel, 48, oric_tr(lang, STR_DSK_FLAG_SECTORS_FMT), secCnt);
            swprintf(entry.extraDetail, 128, oric_tr(lang, STR_DSK_DETAIL_CHAIN_FMT),
                     entry.descSide, entry.descTrack, entry.descSector, ftype);
            oric_report_add_entry(report, &entry);

            map_mark_sector(report->dsk.sectorMap, entry.descSide, entry.descTrack, entry.descSector, ORIC_SECTOR_USED);
        }
    }

    return true;
}

static bool try_parse_oricdos(const DskContext *ctx, OricMediaReport *report, OricLang lang) {
    uint8_t sec2[256], sec3[256];
    if (!detect_oricdos_boot_hint(ctx)) return false;

    report->dsk.dosType = ORIC_DOS_ORICDOS;
    wcscpy(report->dsk.dosLabel, L"OricDOS (Microdisc)");
    map_mark_sector(report->dsk.sectorMap, 0, 0, 1, ORIC_SECTOR_SYSTEM);
    map_mark_sector(report->dsk.sectorMap, 0, 0, 2, ORIC_SECTOR_SYSTEM);
    map_mark_sector(report->dsk.sectorMap, 0, 0, 3, ORIC_SECTOR_SYSTEM);

    if (dsk_read_sector(ctx, 0, 0, 2, sec2) && dsk_read_sector(ctx, 0, 0, 3, sec3)) {
        const uint8_t *catSecs[2] = { sec2, sec3 };
        for (int c = 0; c < 2; c++) {
            for (size_t off = 0; off + 16 <= 256; off += 16) {
                const uint8_t *e = catSecs[c] + off;
                if (e[0] == 0x00 || e[0] == 0xFF) continue;
                if (!is_printable_padded_field(e, 11)) continue;

                char base[12], ext[6], full[24];
                trim_ascii_copy(e, 8, base, sizeof(base));
                trim_ascii_copy(e + 8, 3, ext, sizeof(ext));
                if (base[0] == '\0') continue;
                if (ext[0] != '\0') snprintf(full, sizeof(full), "%s.%s", base, ext);
                else snprintf(full, sizeof(full), "%s", base);

                uint8_t t = e[12];
                uint8_t s = e[13];
                uint8_t cnt = e[14];
                uint8_t side = (t & 0x80u) ? 1u : 0u;
                if (side >= ctx->sides || (t & 0x7Fu) >= ctx->tracks || s == 0 || s > ctx->sectorsPerTrack || cnt == 0) {
                    continue;
                }

                OricFileEntry entry;
                memset(&entry, 0, sizeof(entry));
                entry.source = ORIC_SRC_ORICDOS;
                entry.descSide = side;
                entry.descTrack = (uint8_t)(t & 0x7Fu);
                entry.descSector = s;
                entry.sectorCount = cnt;
                entry.sizeBytes = (uint32_t)cnt * 256u;
                oric_ascii_to_wide(full, sizeof(full), entry.name, 64);
                classify_dos_entry(ext, e[11], e[15], lang, &entry);
                swprintf(entry.flagsLabel, 48, oric_tr(lang, STR_DSK_FLAG_SECTORS_FMT), cnt);
                swprintf(entry.extraDetail, 128, oric_tr(lang, STR_DSK_DETAIL_CHAIN_FMT),
                         entry.descSide, entry.descTrack, entry.descSector, e[11]);
                oric_report_add_entry(report, &entry);
                map_mark_sector(report->dsk.sectorMap, entry.descSide, entry.descTrack, entry.descSector, ORIC_SECTOR_USED);
            }
        }
    }
    return true;
}

static void dsk_populate_entry_crc32(const uint8_t *data, size_t size, OricMediaReport *report) {
    if (!data || !report) return;

    for (size_t i = 0; i < report->entryCount; i++) {
        uint8_t *payload = NULL;
        size_t payloadSize = 0;
        if (oric_extract_payload(data, size, report, i, &payload, &payloadSize)) {
            if (payload && payloadSize > 0) {
                report->entries[i].crc32 = oric_crc32(payload, payloadSize);
                report->entries[i].hasCrc32 = true;
            }
            free(payload);
        }
    }
}

static bool sedoric_dir_header_plausible(const DskContext *ctx, const uint8_t *dirSec) {
    uint8_t nt = dirSec[0];
    uint8_t ns = dirSec[1];
    uint8_t freeOff = dirSec[2];
    bool linkValid = (nt == 0x00 && ns == 0x00) ||
                     (nt == 0xFF) ||
                     (((nt & 0x7Fu) < ctx->tracks) && ns >= 1u && ns <= ctx->sectorsPerTrack);
    bool reservedZero = (dirSec[3] == 0x00 && dirSec[4] == 0x00 && dirSec[5] == 0x00);
    bool freeOffValid = (freeOff == 0x00) ||
                        ((freeOff & 0x0Fu) == 0 && freeOff >= 0x10u && freeOff <= 0xF0u) ||
                        (freeOff <= 15u && reservedZero);
    return linkValid && freeOffValid && reservedZero;
}

bool oric_parse_dsk(const uint8_t *data, size_t size, OricMediaReport *report) {
    OricLang lang = report ? report->lang : ORIC_LANG_EN;

    DskContext ctx;
    if (!report || size < 256) {
        if (report) {
            swprintf(report->statusMessage, 256, L"%ls", oric_tr(lang, STR_DSK_TOO_SHORT));
        }
        return false;
    }

    if (!dsk_init_context(data, size, &ctx)) {
        swprintf(report->statusMessage, 256, L"%ls", oric_tr(lang, STR_DSK_BAD_SIG));
        return false;
    }

    report->mediaType = ORIC_MEDIA_DSK;
    memset(&report->dsk, 0, sizeof(report->dsk));
    report->dsk.containerType = ctx.container;
    report->dsk.dosType = ORIC_DOS_UNKNOWN;
    report->dsk.sides = ctx.sides;
    report->dsk.tracks = ctx.tracks;
    report->dsk.sectorsPerTrack = ctx.sectorsPerTrack;
    report->dsk.geometryType = ctx.geometryType;
    swprintf(report->dsk.diskName, 64, L"%ls", oric_tr(lang, STR_DSK_UNNAMED));
    swprintf(report->dsk.dosLabel, 64, L"%ls", oric_tr(lang, STR_DSK_NO_DOS));

    /* Marquer le secteur de boot piste 0 */
    map_mark_sector(report->dsk.sectorMap, 0, 0, 1, ORIC_SECTOR_SYSTEM);

    if (ctx.tracks <= 20) {
        /* Disquettes < 21 pistes : tenter OricDOS sur piste 0 */
        try_parse_oricdos(&ctx, report, lang);
        dsk_populate_entry_crc32(data, size, report);
        report->parseSuccess = true;
        const wchar_t *contName = (ctx.container == ORIC_DSK_ORICDISK) ? L"ORICDISK" : L"MFM_DISK";
        swprintf(report->statusMessage, 256,
                 (report->dsk.dosType == ORIC_DOS_UNKNOWN)
                     ? oric_tr(lang, STR_DSK_NO_TRACK20)
                     : oric_tr(lang, STR_DSK_OK_FMT),
                 contName, report->dsk.dosLabel, report->entryCount);
        return true;
    }

    /* Piste 20 Secteur 2 : métadonnées & bitmap SEDORIC / StratSED */
    uint8_t sec2[256];
    bool hasSec2 = dsk_read_sector(&ctx, 0, 20, 2, sec2);
    if (hasSec2) {
        uint16_t freeSec = read_le16(sec2 + 2);
        uint16_t totalSec = read_le16(sec2 + 4);
        if (totalSec > 0 && totalSec <= 4000 && freeSec <= totalSec) {
            report->dsk.freeSectors = freeSec;
            report->dsk.totalDataSectors = totalSec;
        }
        char volName[24];
        trim_ascii_copy(sec2 + 9, 21, volName, sizeof(volName));
        if (volName[0] != '\0' && is_printable_padded_field(sec2 + 9, 21)) {
            oric_ascii_to_wide(volName, sizeof(volName), report->dsk.diskName, 64);
        }
    }

    /* Vérifier si FT-DOS / OricDOS explicite sans répertoire Sedoric valide */
    bool bootIsFtOrOricDos = detect_ftdos_boot_hint(&ctx) || detect_oricdos_boot_hint(&ctx);

    /* Parcours de la chaîne du répertoire SEDORIC / StratSED à partir de Piste 20, Secteur 4 */
    uint8_t curTrack = 20;
    uint8_t curSector = 4;
    uint8_t visitedTrack[32];
    uint8_t visitedSector[32];
    size_t visitedCount = 0;
    bool looksLikeSedoric = false;
    size_t validPrimaryDescCount = 0;

    while (curTrack != 0 && curSector != 0 && visitedCount < 32) {
        bool alreadyVisited = false;
        for (size_t v = 0; v < visitedCount; v++) {
            if (visitedTrack[v] == curTrack && visitedSector[v] == curSector) {
                alreadyVisited = true;
                break;
            }
        }
        if (alreadyVisited) break;

        visitedTrack[visitedCount] = curTrack;
        visitedSector[visitedCount] = curSector;
        visitedCount++;

        uint8_t dirSec[256];
        uint32_t dirSide = (curTrack & 0x80) ? 1u : 0u;
        uint32_t physTrack = (uint32_t)(curTrack & 0x7Fu);

        if (!dsk_read_sector(&ctx, dirSide, physTrack, curSector, dirSec)) {
            break;
        }

        if (visitedCount == 1) {
            looksLikeSedoric = sedoric_dir_header_plausible(&ctx, dirSec);
            if (bootIsFtOrOricDos && !looksLikeSedoric) {
                break;
            }
        }

        map_mark_sector(report->dsk.sectorMap, 0, 20, 2, ORIC_SECTOR_SYSTEM);
        map_mark_sector(report->dsk.sectorMap, 0, 20, 3, ORIC_SECTOR_SYSTEM);
        map_mark_sector(report->dsk.sectorMap, dirSide, physTrack, curSector, ORIC_SECTOR_SYSTEM);

        for (size_t idx = 0; idx < 15; idx++) {
            const uint8_t *e = dirSec + 0x10 + idx * 16;

            if (e[0] == 0x00 || e[0] == 0xFF || e[12] == 0xFF ||
                (e[12] == 0x00 && e[13] == 0x00)) {
                continue;
            }

            if (!is_printable_padded_field(e, 9)) {
                continue;
            }

            char baseName[16];
            char extName[8];
            trim_ascii_copy(e, 9, baseName, sizeof(baseName));
            trim_ascii_copy(e + 9, 3, extName, sizeof(extName));
            if (baseName[0] == '\0') continue;

            uint32_t descSide = (e[12] & 0x80) ? 1u : 0u;
            uint32_t descTrack = (uint32_t)(e[12] & 0x7Fu);
            uint32_t descSector = (uint32_t)e[13];
            uint32_t sectorsUsed = (uint32_t)e[14];
            uint8_t attr = e[15];

            if (descSide >= ctx.sides || descTrack >= ctx.tracks ||
                descSector == 0 || descSector > ctx.sectorsPerTrack) {
                continue;
            }

            char fullName[24];
            if (extName[0] != '\0') {
                snprintf(fullName, sizeof(fullName), "%s.%s", baseName, extName);
            } else {
                snprintf(fullName, sizeof(fullName), "%s", baseName);
            }

            OricFileEntry entry;
            memset(&entry, 0, sizeof(entry));
            entry.source = ORIC_SRC_SEDORIC;
            entry.descSide = (uint8_t)descSide;
            entry.descTrack = (uint8_t)descTrack;
            entry.descSector = (uint8_t)descSector;
            oric_ascii_to_wide(fullName, sizeof(fullName), entry.name, 64);
            entry.sectorCount = sectorsUsed;
            entry.isProtected = ((attr & 0x40) != 0);

            /* Compter les secteurs de données réels dans la chaîne de descripteurs */
            size_t walkedDataSectors = 0;
            sedoric_walk_sectors(&ctx, &entry, report->dsk.sectorMap, ORIC_SECTOR_USED,
                                 NULL, 0, NULL, &walkedDataSectors);

            uint8_t descBuf[256];
            uint8_t descType = 0;
            uint8_t descFlag = attr;
            bool hasValidDesc = false;

            if (dsk_read_sector(&ctx, descSide, descTrack, descSector, descBuf)) {
                hasValidDesc = (descBuf[2] == 0xFF);
                if (hasValidDesc) {
                    validPrimaryDescCount++;
                }
                descType = descBuf[3];
                entry.dosFileType = descType;

                uint16_t startAddr = read_le16(descBuf + 4);
                uint16_t endAddr = read_le16(descBuf + 6);
                uint16_t execAddr = read_le16(descBuf + 8);
                uint16_t descSecCount = read_le16(descBuf + 10);

                if (descSecCount > 0 && descSecCount <= ctx.sides * ctx.tracks * ctx.sectorsPerTrack) {
                    if (entry.sectorCount == 0 || descSecCount > entry.sectorCount) {
                        entry.sectorCount = descSecCount;
                    }
                }

                uint32_t dataByteBound = (walkedDataSectors > 0)
                                             ? (uint32_t)(walkedDataSectors * 256u)
                                             : ((entry.sectorCount > 1) ? (entry.sectorCount - 1u) * 256u : 256u);

                /* Les fichiers de données ($C103 == 0x40) stockent nb fiches / taille fiche en C104..C107 */
                bool isRecordHeader = (descType == 0x40 && startAddr < 0x0400u);
                bool validRange = hasValidDesc && !isRecordHeader &&
                                  (endAddr >= startAddr) &&
                                  !(startAddr == 0 && endAddr == 0) &&
                                  (startAddr >= 0x0100u || walkedDataSectors <= 1u) &&
                                  ((uint32_t)(endAddr - startAddr + 1u) <= dataByteBound + 256u);
                if (validRange) {
                    entry.startAddr = startAddr;
                    entry.endAddr = endAddr;
                    entry.execAddr = execAddr;
                    entry.hasStartEnd = true;
                    entry.hasExec = (execAddr != 0);
                    entry.sizeBytes = (uint32_t)(endAddr - startAddr + 1u);
                } else {
                    entry.sizeBytes = dataByteBound;
                }
            } else {
                entry.sizeBytes = (walkedDataSectors > 0)
                                      ? (uint32_t)(walkedDataSectors * 256u)
                                      : ((sectorsUsed > 0) ? sectorsUsed * 256u : 256u);
            }

            classify_dos_entry(extName, descType, descFlag, lang, &entry);

            if (entry.isProtected) {
                swprintf(entry.flagsLabel, 48, oric_tr(lang, STR_DSK_FLAG_PROTECTED_FMT), entry.sectorCount);
            } else {
                swprintf(entry.flagsLabel, 48, oric_tr(lang, STR_DSK_FLAG_SECTORS_FMT), entry.sectorCount);
            }

            swprintf(entry.extraDetail, 128,
                     oric_tr(lang, STR_DSK_DETAIL_FMT),
                     descSide, descTrack, descSector, attr);

            oric_report_add_entry(report, &entry);
        }

        curTrack = dirSec[0];
        curSector = dirSec[1];
    }

    /* Si des entrées ont été collectées mais qu'aucun descripteur #FF valide n'a été vu
       et qu'un autre DOS est détecté (ex. FT-DOS piste 20 secteur 4), repli propre */
    if (report->entryCount > 0 && validPrimaryDescCount == 0) {
        OricMediaReport probeRep;
        oric_report_init(&probeRep);
        probeRep.lang = lang;
        probeRep.dsk = report->dsk;
        swprintf(probeRep.dsk.diskName, 64, L"%ls", oric_tr(lang, STR_DSK_UNNAMED));
        probeRep.dsk.freeSectors = 0;
        probeRep.dsk.totalDataSectors = 0;
        if (try_parse_ftdos(&ctx, &probeRep, lang) || try_parse_oricdos(&ctx, &probeRep, lang)) {
            oric_report_free(report);
            *report = probeRep;
            report->mediaType = ORIC_MEDIA_DSK;
            report->rawFileSize = (uint32_t)size;
            if (data) {
                dsk_populate_entry_crc32(data, size, report);
            }
            report->parseSuccess = true;
            const wchar_t *contName = (ctx.container == ORIC_DSK_ORICDISK) ? L"ORICDISK" : L"MFM_DISK";
            swprintf(report->statusMessage, 256,
                     oric_tr(lang, STR_DSK_OK_FMT),
                     contName, report->dsk.dosLabel, report->entryCount);
            return true;
        }
        oric_report_free(&probeRep);
    }

    if (report->entryCount > 0 || (looksLikeSedoric && report->dsk.totalDataSectors > 0 && !bootIsFtOrOricDos)) {
        report->dsk.isSedoric = true;
        if (detect_stratsed_signature(&ctx)) {
            report->dsk.dosType = ORIC_DOS_STRATSED;
            wcscpy(report->dsk.dosLabel, L"StratSED");
        } else {
            report->dsk.dosType = ORIC_DOS_SEDORIC;
            wcscpy(report->dsk.dosLabel, L"SEDORIC");
        }
    } else {
        /* Repli vers FT-DOS (Jasmin) puis OricDOS (Microdisc) */
        swprintf(report->dsk.diskName, 64, L"%ls", oric_tr(lang, STR_DSK_UNNAMED));
        report->dsk.freeSectors = 0;
        report->dsk.totalDataSectors = 0;
        if (!try_parse_ftdos(&ctx, report, lang)) {
            try_parse_oricdos(&ctx, report, lang);
        }
    }

    dsk_populate_entry_crc32(data, size, report);
    report->parseSuccess = true;
    const wchar_t *contName = (ctx.container == ORIC_DSK_ORICDISK) ? L"ORICDISK" : L"MFM_DISK";
    if (report->entryCount > 0) {
        swprintf(report->statusMessage, 256,
                 oric_tr(lang, STR_DSK_OK_FMT),
                 contName, report->dsk.dosLabel, report->entryCount);
    } else if (report->dsk.dosType != ORIC_DOS_UNKNOWN) {
        swprintf(report->statusMessage, 256,
                 oric_tr(lang, STR_DSK_EMPTY_FMT),
                 contName, report->dsk.dosLabel);
    } else {
        swprintf(report->statusMessage, 256,
                 oric_tr(lang, STR_DSK_NO_CATALOG_FMT),
                 contName);
    }

    return true;
}

bool oric_extract_payload(const uint8_t *mediaData,
                          size_t mediaSize,
                          const OricMediaReport *report,
                          size_t entryIndex,
                          uint8_t **outBuf,
                          size_t *outSize) {
    if (!mediaData || !report || !outBuf || !outSize || entryIndex >= report->entryCount) {
        return false;
    }
    *outBuf = NULL;
    *outSize = 0;

    const OricFileEntry *e = &report->entries[entryIndex];

    if (e->source == ORIC_SRC_TAP) {
        if (e->tapPayloadAvail == 0 ||
            (size_t)e->tapPayloadOffset + (size_t)e->tapPayloadAvail > mediaSize) {
            return false;
        }
        uint8_t *buf = (uint8_t *)malloc(e->tapPayloadAvail);
        if (!buf) return false;
        memcpy(buf, mediaData + e->tapPayloadOffset, e->tapPayloadAvail);
        *outBuf = buf;
        *outSize = e->tapPayloadAvail;
        return true;
    }

    if (e->source == ORIC_SRC_ROM) {
        uint8_t *buf = (uint8_t *)malloc(mediaSize);
        if (!buf) return false;
        memcpy(buf, mediaData, mediaSize);
        *outBuf = buf;
        *outSize = mediaSize;
        return true;
    }

    if (e->source == ORIC_SRC_SEDORIC) {
        DskContext ctx;
        if (!dsk_init_context(mediaData, mediaSize, &ctx)) return false;

        size_t allocSize = e->sizeBytes > 0 ? e->sizeBytes : (e->sectorCount * 256u);
        if (allocSize == 0) allocSize = 256u;
        if (allocSize > 1024u * 1024u) allocSize = 1024u * 1024u;

        uint8_t *buf = (uint8_t *)malloc(allocSize);
        if (!buf) return false;

        size_t got = 0;
        sedoric_walk_sectors(&ctx, e, NULL, 0, buf, allocSize, &got, NULL);
        if (got == 0) {
            free(buf);
            return false;
        }
        *outBuf = buf;
        *outSize = got;
        return true;
    }

    if (e->source == ORIC_SRC_FTDOS || e->source == ORIC_SRC_ORICDOS) {
        DskContext ctx;
        if (!dsk_init_context(mediaData, mediaSize, &ctx)) return false;
        size_t maxSectors = (e->sectorCount > 0 && e->sectorCount < 1024) ? e->sectorCount : 64u;
        uint8_t *buf = (uint8_t *)malloc(maxSectors * 256u);
        if (!buf) return false;

        uint8_t side = e->descSide;
        uint8_t track = e->descTrack;
        uint8_t sector = e->descSector;
        size_t collected = 0;
        for (size_t i = 0; i < maxSectors; i++) {
            uint8_t sec[256];
            if (!dsk_read_sector(&ctx, side, track, sector, sec)) break;
            uint8_t nextT = (uint8_t)(sec[0] & 0x7Fu);
            uint8_t nextSide = (sec[0] & 0x80u) ? 1u : 0u;
            if (e->source == ORIC_SRC_FTDOS && nextT < ctx.tracks && nextSide < ctx.sides &&
                sec[1] >= 1 && sec[1] <= ctx.sectorsPerTrack) {
                memcpy(buf + collected, sec + 2, 254u);
                collected += 254u;
                track = nextT;
                side = nextSide;
                sector = sec[1];
            } else {
                memcpy(buf + collected, sec, 256u);
                collected += 256u;
                sector++;
                if (sector > ctx.sectorsPerTrack) {
                    sector = 1;
                    track++;
                    if (track >= ctx.tracks) {
                        track = 0;
                        side++;
                        if (side >= ctx.sides) break;
                    }
                }
            }
        }
        if (collected == 0) {
            free(buf);
            return false;
        }
        *outBuf = buf;
        *outSize = collected;
        return true;
    }

    return false;
}

void oric_mark_selected_disk_sectors(const uint8_t *mediaData,
                                     size_t mediaSize,
                                     const OricMediaReport *report,
                                     int entryIndex,
                                     uint8_t outMap[ORIC_MAX_MAP_SIDES][ORIC_MAX_MAP_TRACKS][ORIC_MAX_MAP_SECTORS]) {
    if (!report || !outMap) return;
    memcpy(outMap, report->dsk.sectorMap,
           sizeof(uint8_t) * ORIC_MAX_MAP_SIDES * ORIC_MAX_MAP_TRACKS * ORIC_MAX_MAP_SECTORS);

    if (!mediaData || entryIndex < 0 || (size_t)entryIndex >= report->entryCount) {
        return;
    }

    const OricFileEntry *e = &report->entries[entryIndex];
    DskContext ctx;
    if (!dsk_init_context(mediaData, mediaSize, &ctx)) return;

    if (e->source == ORIC_SRC_SEDORIC) {
        sedoric_walk_sectors(&ctx, e, outMap, ORIC_SECTOR_SELECTED, NULL, 0, NULL, NULL);
    } else if (e->source == ORIC_SRC_FTDOS || e->source == ORIC_SRC_ORICDOS) {
        map_mark_sector(outMap, e->descSide, e->descTrack, e->descSector, ORIC_SECTOR_SELECTED);
    }
}
