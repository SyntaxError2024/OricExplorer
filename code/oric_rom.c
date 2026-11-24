#include "oric_core.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    uint32_t crc32;
    uint32_t sizeBytes;
    bool isOric1;
    bool isAtmos;
    bool isTelestrat;
    bool isPravetz;
    bool isPeripheral;
    const wchar_t *fixedModelName;
    OricStrId modelStrId;
    const wchar_t *fixedVersionLabel;
    OricStrId versionStrId;
} KnownOricRom;

static const KnownOricRom KNOWN_ROMS[] = {
    { 0xF18710B4u, 16384u, true,  false, false, false, false, L"Oric-1",     STR_ID_COUNT,            L"BASIC v1.0 (1983)",  STR_ID_COUNT },
    { 0xC3A92BEFu, 16384u, false, true,  false, false, false, L"Oric Atmos", STR_ID_COUNT,            L"BASIC v1.1b (1984)", STR_ID_COUNT },
    { 0xD4D4A7E2u, 16384u, false, true,  false, false, false, NULL,          STR_ROM_MODEL_ATMOS_VAR, L"BASIC v1.1",         STR_ID_COUNT },
    { 0xAA727C5Du, 16384u, false, false, true,  false, false, NULL,          STR_ROM_MODEL_TELESTRAT, NULL,                  STR_ROM_VER_TELMON },
    { 0x1D96AB50u, 16384u, false, false, true,  false, false, NULL,          STR_ROM_MODEL_TELESTRAT, NULL,                  STR_ROM_VER_HYPERBAS },
    { 0x68B0FDE6u, 16384u, false, false, true,  false, false, NULL,          STR_ROM_MODEL_TELESTRAT, NULL,                  STR_ROM_VER_TELEASS },
    { 0x58079502u, 16384u, false, false, false, true,  false, NULL,          STR_ROM_MODEL_PRAVETZ,   NULL,                  STR_ROM_VER_PRAVETZ },
    { 0xA9664A9Cu,  8192u, false, false, false, false, true,  NULL,          STR_ROM_MODEL_MICRODISC, NULL,                  STR_ROM_VER_MICRODISC },
    { 0x61952E34u,  8192u, false, false, false, false, true,  NULL,          STR_ROM_MODEL_BD500,     NULL,                  STR_ROM_VER_BD500 },
    { 0x37220E89u,  2048u, false, false, false, false, true,  NULL,          STR_ROM_MODEL_JASMIN,    NULL,                  STR_ROM_VER_JASMIN }
};

uint32_t oric_crc32(const uint8_t *data, size_t size) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int b = 0; b < 8; b++) {
            uint32_t mask = (uint32_t)(-(int32_t)(crc & 1u));
            crc = (crc >> 1) ^ (0xEDB88320u & mask);
        }
    }
    return ~crc;
}

static bool contains_icase(const char *haystack, const char *needle) {
    size_t hlen = strlen(haystack);
    size_t nlen = strlen(needle);
    if (nlen == 0 || hlen < nlen) {
        return false;
    }
    for (size_t i = 0; i <= hlen - nlen; i++) {
        size_t j = 0;
        while (j < nlen &&
               tolower((unsigned char)haystack[i + j]) == tolower((unsigned char)needle[j])) {
            j++;
        }
        if (j == nlen) {
            return true;
        }
    }
    return false;
}

static void extract_rom_strings(const uint8_t *data, size_t size,
                                OricLang lang,
                                wchar_t *outBuf, size_t outChars,
                                bool *sawOric1Hint, bool *sawAtmosHint,
                                bool *sawBasicHint, bool *sawFrenchHint,
                                bool *sawPravetzHint) {
    char run[128];
    size_t runLen = 0;
    size_t written = 0;
    outBuf[0] = L'\0';

    for (size_t i = 0; i <= size; i++) {
        uint8_t c = (i < size) ? data[i] : 0;
        bool printable = (c >= 32 && c <= 126);
        bool hiLast = (c >= 0xA0 && c <= 0xFE && runLen >= 4);

        if (printable && runLen < sizeof(run) - 1) {
            run[runLen++] = (char)c;
        } else {
            if (hiLast && runLen < sizeof(run) - 1) {
                run[runLen++] = (char)(c & 0x7F);
            }
            if (runLen >= 5) {
                run[runLen] = '\0';
                bool interesting =
                    contains_icase(run, "ORIC") ||
                    contains_icase(run, "ATMOS") ||
                    contains_icase(run, "TANGERINE") ||
                    contains_icase(run, "MICROSOFT") ||
                    contains_icase(run, "BASIC") ||
                    contains_icase(run, "EXTENDED") ||
                    contains_icase(run, "COPYRIGHT") ||
                    contains_icase(run, "(C)") ||
                    contains_icase(run, "MICRODISC") ||
                    contains_icase(run, "JASMIN") ||
                    contains_icase(run, "SEDORIC") ||
                    contains_icase(run, "TELESTRAT") ||
                    contains_icase(run, "TELEMON") ||
                    contains_icase(run, "HYPER") ||
                    contains_icase(run, "PRAVETZ") ||
                    contains_icase(run, "AZERTY") ||
                    contains_icase(run, "FRAN") ||
                    contains_icase(run, "1983") ||
                    contains_icase(run, "1984") ||
                    contains_icase(run, "1986");

                if (contains_icase(run, "ORIC-1") || contains_icase(run, "1983")) {
                    *sawOric1Hint = true;
                }
                if (contains_icase(run, "ATMOS") || contains_icase(run, "1984") ||
                    contains_icase(run, "EXTENDED")) {
                    *sawAtmosHint = true;
                }
                if (contains_icase(run, "BASIC") || contains_icase(run, "TANGERINE") ||
                    contains_icase(run, "MICROSOFT")) {
                    *sawBasicHint = true;
                }
                if (contains_icase(run, "AZERTY") || contains_icase(run, "FRAN") ||
                    contains_icase(run, "ACCENT")) {
                    *sawFrenchHint = true;
                }
                if (contains_icase(run, "PRAVETZ")) {
                    *sawPravetzHint = true;
                }

                if (interesting && written + 4 < outChars) {
                    wchar_t wline[128];
                    oric_ascii_to_wide(run, sizeof(run), wline, 128);
                    int n = swprintf(outBuf + written, outChars - written,
                                     L"%s\"%ls\"",
                                     (written > 0) ? L"\r\n  " : L"  ",
                                     wline);
                    if (n > 0) {
                        written += (size_t)n;
                    }
                }
            }
            runLen = 0;
        }
    }

    if (written == 0) {
        swprintf(outBuf, outChars, L"%ls", oric_tr(lang, STR_ROM_NO_STRINGS));
    }
}

static void add_rom_row(OricMediaReport *report,
                        const wchar_t *name,
                        const wchar_t *type,
                        uint32_t sizeBytes,
                        const wchar_t *addrText,
                        const wchar_t *flagsText,
                        uint16_t startAddr) {
    OricFileEntry e;
    memset(&e, 0, sizeof(e));
    e.source = ORIC_SRC_ROM;
    wcsncpy(e.name, name, 63);
    wcsncpy(e.typeLabel, type, 31);
    e.sizeBytes = sizeBytes;
    e.startAddr = startAddr;
    e.hasStartEnd = (startAddr != 0);
    if (startAddr >= report->rom.baseAddr) {
        e.endAddr = 0xFFFFu;
    }
    wcsncpy(e.extraDetail, addrText, 127);
    wcsncpy(e.flagsLabel, flagsText, 47);
    oric_report_add_entry(report, &e);
}

bool oric_analyze_rom(const uint8_t *data, size_t size, OricMediaReport *report) {
    if (!data || !report || size == 0) {
        return false;
    }

    OricLang lang = report->lang;
    report->mediaType = ORIC_MEDIA_ROM;
    memset(&report->rom, 0, sizeof(report->rom));

    report->rom.sizeBytes = (uint32_t)size;
    report->rom.crc32 = oric_crc32(data, size);

    if (size <= 65536u) {
        report->rom.baseAddr = (uint16_t)(0x10000u - size);
    } else {
        report->rom.baseAddr = 0xC000u;
    }

    if (size >= 6) {
        const uint8_t *v = data + size - 6;
        report->rom.vecNmi = (uint16_t)(v[0] | ((uint16_t)v[1] << 8));
        report->rom.vecReset = (uint16_t)(v[2] | ((uint16_t)v[3] << 8));
        report->rom.vecIrq = (uint16_t)(v[4] | ((uint16_t)v[5] << 8));
        report->rom.hasVectors = true;
    }

    bool sawOric1Hint = false;
    bool sawAtmosHint = false;
    bool sawBasicHint = false;
    bool sawFrenchHint = false;
    bool sawPravetzHint = false;
    extract_rom_strings(data, size, lang,
                        report->rom.detectedStrings, 512,
                        &sawOric1Hint, &sawAtmosHint, &sawBasicHint,
                        &sawFrenchHint, &sawPravetzHint);

    const KnownOricRom *match = NULL;
    for (size_t i = 0; i < sizeof(KNOWN_ROMS) / sizeof(KNOWN_ROMS[0]); i++) {
        if (KNOWN_ROMS[i].crc32 == report->rom.crc32 &&
            KNOWN_ROMS[i].sizeBytes == report->rom.sizeBytes) {
            match = &KNOWN_ROMS[i];
            break;
        }
    }

    if (match) {
        report->rom.isKnownOfficial = true;
        report->rom.isOric1 = match->isOric1;
        report->rom.isAtmos = match->isAtmos;
        report->rom.isTelestrat = match->isTelestrat;
        report->rom.isPravetz = match->isPravetz;
        report->rom.isPeripheral = match->isPeripheral;
        const wchar_t *mName = match->fixedModelName
                                   ? match->fixedModelName
                                   : oric_tr(lang, match->modelStrId);
        const wchar_t *vLabel = match->fixedVersionLabel
                                    ? match->fixedVersionLabel
                                    : oric_tr(lang, match->versionStrId);
        wcsncpy(report->rom.modelName, mName, 63);
        wcsncpy(report->rom.versionLabel, vLabel, 63);
        swprintf(report->rom.authenticityLabel, 96,
                 oric_tr(lang, STR_ROM_AUTH_YES_FMT), report->rom.modelName);
    } else {
        report->rom.isKnownOfficial = false;
        if (size == 16384u && sawBasicHint) {
            if (sawPravetzHint) {
                report->rom.isPravetz = true;
                swprintf(report->rom.modelName, 64, L"%ls", oric_tr(lang, STR_ROM_MODEL_PRAVETZ));
                swprintf(report->rom.versionLabel, 64, L"%ls", oric_tr(lang, STR_ROM_VER_PRAVETZ));
            } else if (sawAtmosHint) {
                swprintf(report->rom.modelName, 64, L"%ls", oric_tr(lang, STR_ROM_MODEL_ATMOS_MOD));
                swprintf(report->rom.versionLabel, 64, L"%ls", oric_tr(lang, STR_ROM_VER_ATMOS_MOD));
            } else if (sawOric1Hint) {
                swprintf(report->rom.modelName, 64, L"%ls", oric_tr(lang, STR_ROM_MODEL_ORIC1_MOD));
                swprintf(report->rom.versionLabel, 64, L"%ls", oric_tr(lang, STR_ROM_VER_ORIC1_MOD));
            } else {
                swprintf(report->rom.modelName, 64, L"%ls", oric_tr(lang, STR_ROM_MODEL_16K_CUSTOM));
                swprintf(report->rom.versionLabel, 64, L"%ls", oric_tr(lang, STR_ROM_VER_CUSTOM));
            }
            swprintf(report->rom.authenticityLabel, 96, L"%ls", oric_tr(lang, STR_ROM_AUTH_NO_MOD));
        } else {
            swprintf(report->rom.modelName, 64, L"%ls", oric_tr(lang, STR_ROM_MODEL_UNLISTED));
            swprintf(report->rom.versionLabel, 64, L"%ls", oric_tr(lang, STR_ROM_VER_UNKNOWN));
            swprintf(report->rom.authenticityLabel, 96, L"%ls", oric_tr(lang, STR_ROM_AUTH_NO_DB));
        }
    }

    if (report->rom.isPravetz || sawPravetzHint) {
        swprintf(report->rom.charsetHint, 64, L"%ls", oric_tr(lang, STR_ROM_CHARSET_CYR));
    } else if (sawFrenchHint) {
        swprintf(report->rom.charsetHint, 64, L"%ls", oric_tr(lang, STR_ROM_CHARSET_FR));
    } else {
        swprintf(report->rom.charsetHint, 64, L"%ls", oric_tr(lang, STR_ROM_CHARSET_STD));
    }

    wchar_t bufAddr[64];
    swprintf(bufAddr, 64, L"$%04X - $FFFF", report->rom.baseAddr);
    add_rom_row(report,
                oric_tr(lang, STR_ROM_ROW_ID),
                oric_tr(lang, STR_ROM_ROW_AUTH_TYPE),
                report->rom.sizeBytes,
                bufAddr,
                report->rom.isKnownOfficial
                    ? oric_tr(lang, STR_ROM_FLAG_ORIGINAL)
                    : oric_tr(lang, STR_ROM_FLAG_UNOFFICIAL),
                report->rom.baseAddr);

    wchar_t crcStr[48];
    swprintf(crcStr, 48, L"CRC32: 0x%08X", report->rom.crc32);
    add_rom_row(report,
                report->rom.modelName,
                oric_tr(lang, STR_ROM_ROW_MODEL_TYPE),
                report->rom.sizeBytes,
                report->rom.versionLabel,
                crcStr,
                report->rom.hasVectors ? report->rom.vecReset : report->rom.baseAddr);

    if (report->entryCount >= 1) {
        report->entries[0].crc32 = report->rom.crc32;
        report->entries[0].hasCrc32 = true;
    }
    if (report->entryCount >= 2) {
        report->entries[1].crc32 = report->rom.crc32;
        report->entries[1].hasCrc32 = true;
    }

    if (report->rom.hasVectors) {
        wchar_t vecText[48];
        bool vectorsValid =
            (report->rom.vecReset >= report->rom.baseAddr) ||
            (report->rom.vecReset >= 0xC000u);

        swprintf(vecText, 48, L"$%04X", report->rom.vecNmi);
        add_rom_row(report, oric_tr(lang, STR_ROM_ROW_NMI),
                    oric_tr(lang, STR_ROM_ROW_VEC_TYPE), 2, vecText,
                    oric_tr(lang, STR_ROM_DESC_NMI), report->rom.vecNmi);

        swprintf(vecText, 48, L"$%04X", report->rom.vecReset);
        add_rom_row(report, oric_tr(lang, STR_ROM_ROW_RESET),
                    oric_tr(lang, STR_ROM_ROW_VEC_TYPE), 2, vecText,
                    vectorsValid
                        ? oric_tr(lang, STR_ROM_DESC_VEC_VALID)
                        : oric_tr(lang, STR_ROM_DESC_VEC_OUT),
                    report->rom.vecReset);

        swprintf(vecText, 48, L"$%04X", report->rom.vecIrq);
        add_rom_row(report, oric_tr(lang, STR_ROM_ROW_IRQ),
                    oric_tr(lang, STR_ROM_ROW_VEC_TYPE), 2, vecText,
                    oric_tr(lang, STR_ROM_DESC_IRQ), report->rom.vecIrq);
    }

    report->parseSuccess = true;
    swprintf(report->statusMessage, 256,
             oric_tr(lang, STR_ROM_OK_FMT),
             report->rom.modelName, report->rom.crc32);
    return true;
}
