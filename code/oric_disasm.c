#include "oric_core.h"
#include <stdio.h>
#include <string.h>

typedef enum {
    AM_IMP = 0,
    AM_ACC,
    AM_IMM,
    AM_ZP,
    AM_ZPX,
    AM_ZPY,
    AM_ABS,
    AM_ABX,
    AM_ABY,
    AM_IND,
    AM_IZX,
    AM_IZY,
    AM_REL,
    AM_ILL
} AddrMode6502;

typedef struct {
    const char *mnem;
    AddrMode6502 mode;
} Opcode6502;

static const Opcode6502 OPCODES[256] = {
    /* 0x00 */ {"BRK",AM_IMP},{"ORA",AM_IZX},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"ORA",AM_ZP}, {"ASL",AM_ZP}, {"???",AM_ILL},
    /* 0x08 */ {"PHP",AM_IMP},{"ORA",AM_IMM},{"ASL",AM_ACC},{"???",AM_ILL},{"???",AM_ILL},{"ORA",AM_ABS},{"ASL",AM_ABS},{"???",AM_ILL},
    /* 0x10 */ {"BPL",AM_REL},{"ORA",AM_IZY},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"ORA",AM_ZPX},{"ASL",AM_ZPX},{"???",AM_ILL},
    /* 0x18 */ {"CLC",AM_IMP},{"ORA",AM_ABY},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"ORA",AM_ABX},{"ASL",AM_ABX},{"???",AM_ILL},
    /* 0x20 */ {"JSR",AM_ABS},{"AND",AM_IZX},{"???",AM_ILL},{"???",AM_ILL},{"BIT",AM_ZP}, {"AND",AM_ZP}, {"ROL",AM_ZP}, {"???",AM_ILL},
    /* 0x28 */ {"PLP",AM_IMP},{"AND",AM_IMM},{"ROL",AM_ACC},{"???",AM_ILL},{"BIT",AM_ABS},{"AND",AM_ABS},{"ROL",AM_ABS},{"???",AM_ILL},
    /* 0x30 */ {"BMI",AM_REL},{"AND",AM_IZY},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"AND",AM_ZPX},{"ROL",AM_ZPX},{"???",AM_ILL},
    /* 0x38 */ {"SEC",AM_IMP},{"AND",AM_ABY},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"AND",AM_ABX},{"ROL",AM_ABX},{"???",AM_ILL},
    /* 0x40 */ {"RTI",AM_IMP},{"EOR",AM_IZX},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"EOR",AM_ZP}, {"LSR",AM_ZP}, {"???",AM_ILL},
    /* 0x48 */ {"PHA",AM_IMP},{"EOR",AM_IMM},{"LSR",AM_ACC},{"???",AM_ILL},{"JMP",AM_ABS},{"EOR",AM_ABS},{"LSR",AM_ABS},{"???",AM_ILL},
    /* 0x50 */ {"BVC",AM_REL},{"EOR",AM_IZY},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"EOR",AM_ZPX},{"LSR",AM_ZPX},{"???",AM_ILL},
    /* 0x58 */ {"CLI",AM_IMP},{"EOR",AM_ABY},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"EOR",AM_ABX},{"LSR",AM_ABX},{"???",AM_ILL},
    /* 0x60 */ {"RTS",AM_IMP},{"ADC",AM_IZX},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"ADC",AM_ZP}, {"ROR",AM_ZP}, {"???",AM_ILL},
    /* 0x68 */ {"PLA",AM_IMP},{"ADC",AM_IMM},{"ROR",AM_ACC},{"???",AM_ILL},{"JMP",AM_IND},{"ADC",AM_ABS},{"ROR",AM_ABS},{"???",AM_ILL},
    /* 0x70 */ {"BVS",AM_REL},{"ADC",AM_IZY},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"ADC",AM_ZPX},{"ROR",AM_ZPX},{"???",AM_ILL},
    /* 0x78 */ {"SEI",AM_IMP},{"ADC",AM_ABY},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"ADC",AM_ABX},{"ROR",AM_ABX},{"???",AM_ILL},
    /* 0x80 */ {"???",AM_ILL},{"STA",AM_IZX},{"???",AM_ILL},{"???",AM_ILL},{"STY",AM_ZP}, {"STA",AM_ZP}, {"STX",AM_ZP}, {"???",AM_ILL},
    /* 0x88 */ {"DEY",AM_IMP},{"???",AM_ILL},{"TXA",AM_IMP},{"???",AM_ILL},{"STY",AM_ABS},{"STA",AM_ABS},{"STX",AM_ABS},{"???",AM_ILL},
    /* 0x90 */ {"BCC",AM_REL},{"STA",AM_IZY},{"???",AM_ILL},{"???",AM_ILL},{"STY",AM_ZPX},{"STA",AM_ZPX},{"STX",AM_ZPY},{"???",AM_ILL},
    /* 0x98 */ {"TYA",AM_IMP},{"STA",AM_ABY},{"TXS",AM_IMP},{"???",AM_ILL},{"???",AM_ILL},{"STA",AM_ABX},{"???",AM_ILL},{"???",AM_ILL},
    /* 0xA0 */ {"LDY",AM_IMM},{"LDA",AM_IZX},{"LDX",AM_IMM},{"???",AM_ILL},{"LDY",AM_ZP}, {"LDA",AM_ZP}, {"LDX",AM_ZP}, {"???",AM_ILL},
    /* 0xA8 */ {"TAY",AM_IMP},{"LDA",AM_IMM},{"TAX",AM_IMP},{"???",AM_ILL},{"LDY",AM_ABS},{"LDA",AM_ABS},{"LDX",AM_ABS},{"???",AM_ILL},
    /* 0xB0 */ {"BCS",AM_REL},{"LDA",AM_IZY},{"???",AM_ILL},{"???",AM_ILL},{"LDY",AM_ZPX},{"LDA",AM_ZPX},{"LDX",AM_ZPY},{"???",AM_ILL},
    /* 0xB8 */ {"CLV",AM_IMP},{"LDA",AM_ABY},{"TSX",AM_IMP},{"???",AM_ILL},{"LDY",AM_ABX},{"LDA",AM_ABX},{"LDX",AM_ABY},{"???",AM_ILL},
    /* 0xC0 */ {"CPY",AM_IMM},{"CMP",AM_IZX},{"???",AM_ILL},{"???",AM_ILL},{"CPY",AM_ZP}, {"CMP",AM_ZP}, {"DEC",AM_ZP}, {"???",AM_ILL},
    /* 0xC8 */ {"INY",AM_IMP},{"CMP",AM_IMM},{"DEX",AM_IMP},{"???",AM_ILL},{"CPY",AM_ABS},{"CMP",AM_ABS},{"DEC",AM_ABS},{"???",AM_ILL},
    /* 0xD0 */ {"BNE",AM_REL},{"CMP",AM_IZY},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"CMP",AM_ZPX},{"DEC",AM_ZPX},{"???",AM_ILL},
    /* 0xD8 */ {"CLD",AM_IMP},{"CMP",AM_ABY},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"CMP",AM_ABX},{"DEC",AM_ABX},{"???",AM_ILL},
    /* 0xE0 */ {"CPX",AM_IMM},{"SBC",AM_IZX},{"???",AM_ILL},{"???",AM_ILL},{"CPX",AM_ZP}, {"SBC",AM_ZP}, {"INC",AM_ZP}, {"???",AM_ILL},
    /* 0xE8 */ {"INX",AM_IMP},{"SBC",AM_IMM},{"NOP",AM_IMP},{"???",AM_ILL},{"CPX",AM_ABS},{"SBC",AM_ABS},{"INC",AM_ABS},{"???",AM_ILL},
    /* 0xF0 */ {"BEQ",AM_REL},{"SBC",AM_IZY},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"SBC",AM_ZPX},{"INC",AM_ZPX},{"???",AM_ILL},
    /* 0xF8 */ {"SED",AM_IMP},{"SBC",AM_ABY},{"???",AM_ILL},{"???",AM_ILL},{"???",AM_ILL},{"SBC",AM_ABX},{"INC",AM_ABX},{"???",AM_ILL}
};

static int instr_len(AddrMode6502 m) {
    switch (m) {
    case AM_IMP:
    case AM_ACC:
    case AM_ILL:
        return 1;
    case AM_IMM:
    case AM_ZP:
    case AM_ZPX:
    case AM_ZPY:
    case AM_IZX:
    case AM_IZY:
    case AM_REL:
        return 2;
    case AM_ABS:
    case AM_ABX:
    case AM_ABY:
    case AM_IND:
        return 3;
    }
    return 1;
}

void oric_disassemble_6502(const uint8_t *data,
                           size_t size,
                           uint16_t baseAddr,
                           size_t startOffset,
                           size_t maxLines,
                           wchar_t *outBuf,
                           size_t outChars) {
    if (!outBuf || outChars == 0) return;
    outBuf[0] = L'\0';
    if (!data || size == 0 || startOffset >= size) return;

    size_t pos = 0;
    size_t off = startOffset;
    size_t lines = 0;
    if (maxLines == 0) maxLines = 400;

    while (off < size && lines < maxLines && pos + 80 < outChars) {
        uint8_t op = data[off];
        Opcode6502 info = OPCODES[op];
        int len = instr_len(info.mode);
        if (off + (size_t)len > size) {
            len = 1;
            info.mode = AM_ILL;
        }

        uint16_t pc = (uint16_t)(baseAddr + (uint16_t)off);
        char bytesStr[16];
        if (len == 1) {
            snprintf(bytesStr, sizeof(bytesStr), "%02X      ", op);
        } else if (len == 2) {
            snprintf(bytesStr, sizeof(bytesStr), "%02X %02X   ", op, data[off + 1]);
        } else {
            snprintf(bytesStr, sizeof(bytesStr), "%02X %02X %02X", op, data[off + 1], data[off + 2]);
        }

        char asmStr[32];
        uint8_t b1 = (len >= 2) ? data[off + 1] : 0;
        uint16_t w = (len >= 3) ? (uint16_t)(data[off + 1] | ((uint16_t)data[off + 2] << 8)) : 0;

        switch (info.mode) {
        case AM_IMP: snprintf(asmStr, sizeof(asmStr), "%s", info.mnem); break;
        case AM_ACC: snprintf(asmStr, sizeof(asmStr), "%s A", info.mnem); break;
        case AM_IMM: snprintf(asmStr, sizeof(asmStr), "%s #$%02X", info.mnem, b1); break;
        case AM_ZP:  snprintf(asmStr, sizeof(asmStr), "%s $%02X", info.mnem, b1); break;
        case AM_ZPX: snprintf(asmStr, sizeof(asmStr), "%s $%02X,X", info.mnem, b1); break;
        case AM_ZPY: snprintf(asmStr, sizeof(asmStr), "%s $%02X,Y", info.mnem, b1); break;
        case AM_ABS: snprintf(asmStr, sizeof(asmStr), "%s $%04X", info.mnem, w); break;
        case AM_ABX: snprintf(asmStr, sizeof(asmStr), "%s $%04X,X", info.mnem, w); break;
        case AM_ABY: snprintf(asmStr, sizeof(asmStr), "%s $%04X,Y", info.mnem, w); break;
        case AM_IND: snprintf(asmStr, sizeof(asmStr), "%s ($%04X)", info.mnem, w); break;
        case AM_IZX: snprintf(asmStr, sizeof(asmStr), "%s ($%02X,X)", info.mnem, b1); break;
        case AM_IZY: snprintf(asmStr, sizeof(asmStr), "%s ($%02X),Y", info.mnem, b1); break;
        case AM_REL: {
            int8_t rel = (int8_t)b1;
            uint16_t target = (uint16_t)(pc + 2 + rel);
            snprintf(asmStr, sizeof(asmStr), "%s $%04X", info.mnem, target);
            break;
        }
        case AM_ILL:
        default:
            snprintf(asmStr, sizeof(asmStr), ".BYTE $%02X", op);
            break;
        }

        int n = swprintf(outBuf + pos, outChars - pos, L"$%04X:  %hs  %hs\r\n",
                         pc, bytesStr, asmStr);
        if (n <= 0) break;
        pos += (size_t)n;
        off += (size_t)len;
        lines++;
    }
}

void oric_format_hex_dump(const uint8_t *data,
                          size_t size,
                          uint16_t baseAddr,
                          size_t maxBytes,
                          wchar_t *outBuf,
                          size_t outChars) {
    if (!outBuf || outChars == 0) return;
    outBuf[0] = L'\0';
    if (!data || size == 0) return;

    size_t limit = (maxBytes > 0 && maxBytes < size) ? maxBytes : size;
    size_t pos = 0;

    for (size_t off = 0; off < limit && pos + 84 < outChars; off += 16) {
        uint16_t addr = (uint16_t)(baseAddr + (uint16_t)off);
        char hexPart[64];
        char ascPart[20];
        size_t hp = 0;
        size_t ap = 0;

        for (size_t i = 0; i < 16; i++) {
            if (off + i < limit) {
                uint8_t c = data[off + i];
                hp += (size_t)snprintf(hexPart + hp, sizeof(hexPart) - hp,
                                       (i == 7) ? "%02X  " : "%02X ", c);
                ascPart[ap++] = (c >= 32 && c < 127) ? (char)c : '.';
            } else {
                hp += (size_t)snprintf(hexPart + hp, sizeof(hexPart) - hp,
                                       (i == 7) ? "    " : "   ");
                ascPart[ap++] = ' ';
            }
        }
        ascPart[ap] = '\0';

        int n = swprintf(outBuf + pos, outChars - pos, L"%04X: %hs |%hs|\r\n",
                         addr, hexPart, ascPart);
        if (n <= 0) break;
        pos += (size_t)n;
    }
}
