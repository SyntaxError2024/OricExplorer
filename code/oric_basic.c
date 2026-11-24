#include "oric_core.h"
#include <stdio.h>
#include <string.h>

static const char *const ORIC_BASIC_TOKENS[] = {
    /* 0x80 */ "END",     "EDIT",   "STORE",  "RECALL", "TRON",   "TROFF",  "POP",     "PLOT",
    /* 0x88 */ "PULL",    "LORES",  "DOKE",   "REPEAT", "UNTIL",  "FOR",    "LLIST",   "LPRINT",
    /* 0x90 */ "NEXT",    "DATA",   "INPUT",  "DIM",    "READ",   "LET",    "GOTO",    "RUN",
    /* 0x98 */ "IF",      "RESTORE","GOSUB",  "RETURN", "REM",    "HIMEM",  "GRAB",    "RELEASE",
    /* 0xA0 */ "TEXT",    "HIRES",  "SHOOT",  "EXPLODE","ZAP",    "PING",   "SOUND",   "MUSIC",
    /* 0xA8 */ "PLAY",    "CURSET", "CURMOV", "DRAW",   "CIRCLE", "PATTERN","FILL",    "CHAR",
    /* 0xB0 */ "PAPER",   "INK",    "STOP",   "ON",     "WAIT",   "CLOAD",  "CSAVE",   "DEF",
    /* 0xB8 */ "POKE",    "PRINT",  "CONT",   "LIST",   "CLEAR",  "GET",    "CALL",    "!",
    /* 0xC0 */ "NEW",     "TAB(",   "TO",     "FN",     "SPC(",   "@",      "AUTO",    "ELSE",
    /* 0xC8 */ "THEN",    "NOT",    "STEP",   "+",      "-",      "*",      "/",       "^",
    /* 0xD0 */ "AND",     "OR",     ">",      "=",      "<",      "SGN",    "INT",     "ABS",
    /* 0xD8 */ "USR",     "FRE",    "POS",    "HEX$",   "&",      "SQR",    "RND",     "LN",
    /* 0xE0 */ "EXP",     "COS",    "SIN",    "TAN",    "ATN",    "PEEK",   "DEEK",    "LOG",
    /* 0xE8 */ "LEN",     "STR$",   "VAL",    "ASC",    "CHR$",   "LEFT$",  "RIGHT$",  "MID$",
    /* 0xF0 */ "GO",      "KEY$",   "SCRN",   "POINT",  "TRUE",   "FALSE",  "PI"
};

#define TOKEN_BASE  0x80u
#define TOKEN_COUNT (sizeof(ORIC_BASIC_TOKENS) / sizeof(ORIC_BASIC_TOKENS[0]))

static void append_wchar(wchar_t *buf, size_t maxChars, size_t *pos, wchar_t ch) {
    if (*pos + 1 < maxChars) {
        buf[(*pos)++] = ch;
        buf[*pos] = L'\0';
    }
}

static void append_ascii(wchar_t *buf, size_t maxChars, size_t *pos, const char *str) {
    while (*str && *pos + 1 < maxChars) {
        buf[(*pos)++] = (wchar_t)(unsigned char)(*str++);
    }
    if (*pos < maxChars) buf[*pos] = L'\0';
}

static uint16_t read_le16_basic(const uint8_t *p) {
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static size_t find_eol_zero(const uint8_t *payload, size_t size, size_t bodyStart, size_t maxSpan) {
    size_t eol = bodyStart;
    while (eol < size && payload[eol] != 0x00u && (eol - bodyStart) < maxSpan) {
        eol++;
    }
    return eol;
}

static bool looks_like_ascii_basic(const uint8_t *payload, size_t size) {
    if (!payload || size == 0) return false;
    size_t trimmed = size;
    while (trimmed > 0 && (payload[trimmed - 1] == 0x00u || payload[trimmed - 1] == 0x1Au)) {
        trimmed--;
    }
    if (trimmed == 0) return false;

    bool hasLineBreak = false;
    for (size_t i = 0; i < trimmed; i++) {
        uint8_t b = payload[i];
        if (b == '\r' || b == '\n') {
            hasLineBreak = true;
            continue;
        }
        if (b == '\t') continue;
        if (b >= 32u && b < 127u) continue;
        return false;
    }
    /* Exiger au moins un saut de ligne ou un numéro de ligne ASCII en début de fichier */
    return hasLineBreak || (payload[0] >= '0' && payload[0] <= '9');
}

static bool format_ascii_basic(const uint8_t *payload, size_t size, wchar_t *outBuf, size_t outChars) {
    size_t trimmed = size;
    while (trimmed > 0 && (payload[trimmed - 1] == 0x00u || payload[trimmed - 1] == 0x1Au)) {
        trimmed--;
    }
    size_t pos = 0;
    for (size_t i = 0; i < trimmed && pos + 4 < outChars; i++) {
        uint8_t b = payload[i];
        if (b == '\r') {
            append_wchar(outBuf, outChars, &pos, L'\r');
            append_wchar(outBuf, outChars, &pos, L'\n');
            if (i + 1 < trimmed && payload[i + 1] == '\n') {
                i++;
            }
        } else if (b == '\n') {
            append_wchar(outBuf, outChars, &pos, L'\r');
            append_wchar(outBuf, outChars, &pos, L'\n');
        } else if (b == '\t') {
            append_wchar(outBuf, outChars, &pos, L' ');
        } else if (b >= 32u && b < 127u) {
            append_wchar(outBuf, outChars, &pos, (wchar_t)b);
        }
    }
    return pos > 0;
}

bool oric_detokenize_basic(const uint8_t *payload,
                           size_t size,
                           uint16_t baseAddr,
                           wchar_t *outBuf,
                           size_t outChars) {
    if (!outBuf || outChars == 0) return false;
    outBuf[0] = L'\0';
    if (!payload || size < 5) return false;

    if (looks_like_ascii_basic(payload, size)) {
        return format_ascii_basic(payload, size, outBuf, outChars);
    }

    size_t pos = 0;
    size_t offset = 0;
    uint16_t curMemAddr = (baseAddr != 0) ? baseAddr : 0x0501u;

    /* En RAM Oric, l'adresse $0500 contient un octet sentinelle 0x00 avant la 1re ligne à $0501 */
    if (size >= 7 && payload[0] == 0x00u && !(payload[1] == 0x00u && payload[2] == 0x00u)) {
        uint16_t ptr0 = read_le16_basic(payload);
        uint16_t ptr1 = read_le16_basic(payload + 1);
        size_t eol1 = find_eol_zero(payload, size, 5u, 512u);
        bool matches0501 = (ptr1 >= 0x0506u && ptr1 < 0xC000u &&
                            eol1 < size && (size_t)(ptr1 - 0x0501u) == eol1);
        if (baseAddr == 0x0500u || matches0501 ||
            (ptr0 < 0x0200u && ptr1 >= 0x0205u && ptr1 < 0xC000u)) {
            offset = 1u;
            curMemAddr = (baseAddr == 0x0500u || baseAddr == 0u) ? 0x0501u : (uint16_t)(baseAddr + 1u);
        }
    }

    int linesDecoded = 0;

    while (offset + 4 <= size && pos + 32 < outChars) {
        uint16_t nextPtr = read_le16_basic(payload + offset);
        if (nextPtr == 0x0000u) {
            break;
        }
        if (nextPtr < 0x0200u || nextPtr >= 0xF000u) {
            if (linesDecoded == 0) {
                outBuf[0] = L'\0';
                return false;
            }
            break;
        }

        uint16_t lineNum = read_le16_basic(payload + offset + 2);
        size_t bodyStart = offset + 4u;
        size_t eol = find_eol_zero(payload, size, bodyStart, 512u);
        size_t nextOffset = (eol < size && payload[eol] == 0x00u) ? (eol + 1u) : size;
        uint16_t nextMemAddr = (uint16_t)(curMemAddr + (uint16_t)(nextOffset - offset));
        bool matchedPtr = false;

        /* Si nextPtr pointe exactement après un 0x00 EOL (y compris après un 0x00 embarqué dans REM), suivre nextPtr */
        if (nextPtr > curMemAddr && (size_t)(nextPtr - curMemAddr) < 1024u) {
            size_t ptrNext = offset + (size_t)(nextPtr - curMemAddr);
            if (ptrNext >= bodyStart + 1u && ptrNext <= size && payload[ptrNext - 1u] == 0x00u) {
                eol = ptrNext - 1u;
                nextOffset = ptrNext;
                nextMemAddr = nextPtr;
                matchedPtr = true;
            }
        }

        /* Sinon, sur la première ligne, synchroniser l'adresse mémoire si le programme a été relocalisé */
        if (!matchedPtr && linesDecoded == 0 && eol < size && payload[eol] == 0x00u) {
            size_t seqLen = (eol + 1u) - offset;
            if (nextPtr > (uint16_t)seqLen) {
                uint16_t inferredBase = (uint16_t)(nextPtr - (uint16_t)seqLen);
                if (inferredBase >= 0x0200u && inferredBase < 0xC000u) {
                    curMemAddr = inferredBase;
                    nextMemAddr = nextPtr;
                }
            }
        }

        if (eol >= size && payload[size - 1u] != 0x00u) {
            if (linesDecoded == 0 && (eol - bodyStart) > 256u) {
                outBuf[0] = L'\0';
                return false;
            }
        }

        wchar_t numBuf[16];
        swprintf(numBuf, 16, L"%u ", (unsigned)lineNum);
        for (size_t k = 0; numBuf[k]; k++) {
            append_wchar(outBuf, outChars, &pos, numBuf[k]);
        }

        bool inQuotes = false;
        bool inRem = false;
        bool inData = false;
        for (size_t i = bodyStart; i < eol && pos + 16 < outChars; i++) {
            uint8_t b = payload[i];
            if (inRem) {
                if (b >= 32u && b < 127u) {
                    append_wchar(outBuf, outChars, &pos, (wchar_t)b);
                } else if (b == '\t') {
                    append_wchar(outBuf, outChars, &pos, L' ');
                } else {
                    char ctrl[8];
                    snprintf(ctrl, sizeof(ctrl), "\\x%02X", b);
                    append_ascii(outBuf, outChars, &pos, ctrl);
                }
            } else if (b == '"') {
                inQuotes = !inQuotes;
                append_wchar(outBuf, outChars, &pos, L'"');
            } else if (!inQuotes && !inData && b == '\'') {
                append_wchar(outBuf, outChars, &pos, L'\'');
                inRem = true;
            } else if (!inQuotes && inData && b == ':') {
                append_wchar(outBuf, outChars, &pos, L':');
                inData = false;
            } else if (!inQuotes && !inData && b >= TOKEN_BASE) {
                size_t tidx = (size_t)(b - TOKEN_BASE);
                if (tidx < TOKEN_COUNT) {
                    const char *kw = ORIC_BASIC_TOKENS[tidx];
                    append_ascii(outBuf, outChars, &pos, kw);
                    if (b == 0x9Cu) { /* REM */
                        inRem = true;
                    } else if (b == 0x91u) { /* DATA */
                        inData = true;
                    }
                } else {
                    char tokHex[12];
                    snprintf(tokHex, sizeof(tokHex), "[#%02X]", b);
                    append_ascii(outBuf, outChars, &pos, tokHex);
                }
            } else if (b >= 32u && b < 127u) {
                append_wchar(outBuf, outChars, &pos, (wchar_t)b);
            } else if (b == '\t') {
                append_wchar(outBuf, outChars, &pos, L' ');
            } else if (b != 0) {
                char ctrl[8];
                snprintf(ctrl, sizeof(ctrl), "\\x%02X", b);
                append_ascii(outBuf, outChars, &pos, ctrl);
            }
        }

        append_wchar(outBuf, outChars, &pos, L'\r');
        append_wchar(outBuf, outChars, &pos, L'\n');
        linesDecoded++;

        offset = nextOffset;
        curMemAddr = nextMemAddr;
    }

    return linesDecoded > 0;
}
