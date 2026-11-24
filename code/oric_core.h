#ifndef ORIC_CORE_H
#define ORIC_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <wchar.h>

#include "oric_i18n.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ORIC_MAX_MAP_SIDES   2
#define ORIC_MAX_MAP_TRACKS  85
#define ORIC_MAX_MAP_SECTORS 32

typedef enum {
    ORIC_MEDIA_NONE = 0,
    ORIC_MEDIA_TAP,
    ORIC_MEDIA_DSK,
    ORIC_MEDIA_ROM,
    ORIC_MEDIA_UNKNOWN
} OricMediaType;

typedef enum {
    ORIC_DSK_NONE = 0,
    ORIC_DSK_ORICDISK,
    ORIC_DSK_MFM_DISK
} OricDskContainer;

typedef enum {
    ORIC_DOS_UNKNOWN = 0,
    ORIC_DOS_SEDORIC,
    ORIC_DOS_STRATSED,
    ORIC_DOS_FTDOS,
    ORIC_DOS_ORICDOS
} OricDskDosType;

typedef enum {
    ORIC_SECTOR_FREE = 0,
    ORIC_SECTOR_USED = 1,
    ORIC_SECTOR_SYSTEM = 2,
    ORIC_SECTOR_SELECTED = 3
} OricSectorState;

typedef enum {
    ORIC_SRC_NONE = 0,
    ORIC_SRC_TAP,
    ORIC_SRC_SEDORIC,
    ORIC_SRC_FTDOS,
    ORIC_SRC_ORICDOS,
    ORIC_SRC_ROM
} OricEntrySource;

typedef struct {
    wchar_t name[64];
    wchar_t typeLabel[32];
    wchar_t flagsLabel[48];
    wchar_t extraDetail[128];
    uint32_t sizeBytes;
    uint32_t sectorCount;
    uint32_t crc32;
    uint16_t startAddr;
    uint16_t endAddr;
    uint16_t execAddr;
    bool hasStartEnd;
    bool hasExec;
    bool hasCrc32;
    bool isProtected;
    bool isAutorun;
    bool isBasic;
    bool isScreen;

    /* Extraction metadata */
    OricEntrySource source;
    uint32_t tapPayloadOffset;
    uint32_t tapPayloadAvail;
    uint8_t tapFileType;
    uint8_t tapAutorun;
    uint8_t descSide;
    uint8_t descTrack;
    uint8_t descSector;
    uint8_t dosFileType;
} OricFileEntry;

typedef struct {
    uint32_t crc32;
    uint32_t sizeBytes;
    bool isKnownOfficial;
    bool isOric1;
    bool isAtmos;
    bool isTelestrat;
    bool isPravetz;
    bool isPeripheral;
    wchar_t modelName[64];
    wchar_t versionLabel[64];
    wchar_t authenticityLabel[96];
    wchar_t charsetHint[64];
    uint16_t baseAddr;
    uint16_t vecNmi;
    uint16_t vecReset;
    uint16_t vecIrq;
    bool hasVectors;
    wchar_t detectedStrings[512];
} OricRomInfo;

typedef struct {
    OricDskContainer containerType;
    OricDskDosType dosType;
    uint32_t sides;
    uint32_t tracks;
    uint32_t sectorsPerTrack;
    uint32_t geometryType;
    bool isSedoric;
    wchar_t diskName[64];
    wchar_t dosLabel[64];
    uint32_t freeSectors;
    uint32_t totalDataSectors;
    uint8_t sectorMap[ORIC_MAX_MAP_SIDES][ORIC_MAX_MAP_TRACKS][ORIC_MAX_MAP_SECTORS];
} OricDskInfo;

typedef struct {
    OricLang lang;
    OricMediaType mediaType;
    wchar_t filePath[520];
    wchar_t fileName[260];
    uint32_t rawFileSize;
    bool parseSuccess;
    wchar_t statusMessage[256];

    OricFileEntry *entries;
    size_t entryCount;
    size_t entryCapacity;

    OricDskInfo dsk;
    OricRomInfo rom;
} OricMediaReport;

typedef enum {
    ORIC_SCR_NONE = 0,
    ORIC_SCR_HIRES,
    ORIC_SCR_TEXT
} OricScreenMode;

/* Gestion du rapport */
void oric_report_init(OricMediaReport *report);
void oric_report_free(OricMediaReport *report);
bool oric_report_add_entry(OricMediaReport *report, const OricFileEntry *entry);

/* Analyse un fichier déjà chargé en mémoire */
bool oric_analyze_buffer(const uint8_t *data,
                         size_t size,
                         const wchar_t *filePath,
                         OricLang lang,
                         OricMediaReport *report);

/* Analyseurs spécialisés */
bool oric_parse_tap(const uint8_t *data, size_t size, OricMediaReport *report);
bool oric_parse_dsk(const uint8_t *data, size_t size, OricMediaReport *report);
bool oric_analyze_rom(const uint8_t *data, size_t size, OricMediaReport *report);

/* Extraction & carte disquette */
bool oric_extract_payload(const uint8_t *mediaData,
                          size_t mediaSize,
                          const OricMediaReport *report,
                          size_t entryIndex,
                          uint8_t **outBuf,
                          size_t *outSize);

void oric_mark_selected_disk_sectors(const uint8_t *mediaData,
                                     size_t mediaSize,
                                     const OricMediaReport *report,
                                     int entryIndex,
                                     uint8_t outMap[ORIC_MAX_MAP_SIDES][ORIC_MAX_MAP_TRACKS][ORIC_MAX_MAP_SECTORS]);

/* Décodeur BASIC, Désassembleur 6502 & Dump Hexa */
bool oric_detokenize_basic(const uint8_t *payload,
                           size_t size,
                           uint16_t baseAddr,
                           wchar_t *outBuf,
                           size_t outChars);

void oric_disassemble_6502(const uint8_t *data,
                           size_t size,
                           uint16_t baseAddr,
                           size_t startOffset,
                           size_t maxLines,
                           wchar_t *outBuf,
                           size_t outChars);

void oric_format_hex_dump(const uint8_t *data,
                          size_t size,
                          uint16_t baseAddr,
                          size_t maxBytes,
                          wchar_t *outBuf,
                          size_t outChars);

/* Rendu écran HIRES / TEXT (32-bit BGRA pour StretchDIBits) */
OricScreenMode oric_detect_screen_mode(const OricFileEntry *entry, size_t payloadSize);
bool oric_render_screen(const uint8_t *payload,
                        size_t payloadSize,
                        OricScreenMode mode,
                        uint32_t **outBgraPixels,
                        int *outWidth,
                        int *outHeight);

/* Export .TAP et .WAV */
bool oric_build_tap_buffer(const OricFileEntry *entry,
                           const uint8_t *payload,
                           size_t payloadSize,
                           uint8_t **outTap,
                           size_t *outTapSize);

bool oric_build_wav_from_tap(const uint8_t *tapData,
                             size_t tapSize,
                             uint8_t **outWav,
                             size_t *outWavSize);

/* Export PNG natif (C11 sans dépendance, DEFLATE stored) */
bool oric_build_png_from_bgra(const uint32_t *bgraPixels,
                              int width,
                              int height,
                              uint8_t **outPng,
                              size_t *outPngSize);

/* Utilitaires */
uint32_t oric_crc32(const uint8_t *data, size_t size);
void oric_ascii_to_wide(const char *src, size_t maxLen, wchar_t *dst, size_t dstChars);
void oric_format_summary(const OricMediaReport *report,
                         OricLang lang,
                         int selectedIndex,
                         wchar_t *outBuf,
                         size_t outChars);

#ifdef __cplusplus
}
#endif

#endif /* ORIC_CORE_H */
