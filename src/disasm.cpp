#include <cstddef>
#include <limits.h>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include "types.h"
#include "utils.h"
#include "config.h"
#include "device.h"
#include "memory.h"
#include "ula.h"
#include "z80.h"
#include "disasm.h"

namespace Disasm {
    const char *flags[] = { "NZ", "Z", "NC", "C", "PO", "PE", "P", "M" };
    const char *opr_8[] = { "B", "C", "D", "E", "H", "L", "(HL)", "A" };
    const char *reg_16a[] = { "BC", "DE", "HL", "SP" };
    const char *reg_16b[] = { "BC", "DE", "HL", "AF" };
    const char *reg_16i[] = { "IX", "IY" };
    const char *name[] = {
        "ADC", "ADD", "AND", "BIT", "CALL", "CCF", "CP", "CPD",
        "CPDR", "CPI", "CPIR", "CPL", "DAA", "DB", "DEC", "DI",
        "DJNZ", "EI", "EX", "EXX", "HALT", "IM", "IN", "INC",
        "IND", "INDR", "INI", "INIR", "JP", "JR", "LD", "LDD",
        "LDDR", "LDI", "LDIR", "NEG", "NOP", "OR", "OUTDR", "OTIR",
        "OUT", "OUTD", "OUTI", "POP", "PUSH", "RES", "RET", "RETI",
        "RETN", "RL", "RLA", "RLC", "RLCA", "RLD", "RR", "RRA",
        "RRC", "RRCA", "RRD", "RST", "SBC", "SCF", "SET", "SLA",
        "SLL", "SRA", "SRL", "SUB", "XOR"
    };

    struct MM_Decode no_prefix[0x100] = {
        { NOP, NULL }, { LD, "p, w" }, { LD, "(BC), A" }, { INC, "p" }, { INC, "y" }, { DEC, "y" }, { LD, "B, b" }, { RLCA, NULL },
        { EX, "AF, AF'" }, { ADD, "HL, p" }, { LD, "A, (BC)" }, { DEC, "p" }, { INC, "y" }, { DEC, "y" }, { LD, "C, b" }, { RRCA, NULL },
        { DJNZ, "l" }, { LD, "p, w" }, { LD, "(DE), A" }, { INC, "p" }, { INC, "y" }, { DEC, "y" }, { LD, "D, b" }, { RLA, NULL },
        { JR, "c, l" }, { ADD, "HL, p" }, { LD, "A, (DE)" }, { DEC, "p" }, { INC, "y" }, { DEC, "y" }, { LD, "E, b" }, { RRA, NULL },
        { JR, "c, l" }, { LD, "p, w" }, { LD, "(w), HL" }, { INC, "p" }, { INC, "y" }, { DEC, "y" }, { LD, "H, b" }, { DAA, NULL },
        { JR, "c, l" }, { ADD, "HL, p" }, { LD, "HL, (w)" }, { DEC, "p" }, { INC, "y" }, { DEC, "y" }, { LD, "L, b" }, { CPL, NULL },
        { JR, "c, l" }, { LD, "p, w" }, { LD, "(w), A" }, { INC, "p" }, { INC, "y" }, { DEC, "y" }, { LD, "(HL), b" }, { SCF, NULL },
        { JR, "c, l" }, { ADD, "HL, p" }, { LD, "A, (w)" }, { DEC, "p" }, { INC, "y" }, { DEC, "y" }, { LD, "A, b" }, { CCF, NULL },
        { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" },
        { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" },
        { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" },
        { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" },
        { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" },
        { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" },
        { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { HLT, NULL }, { LD, "y, z" },
        { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" }, { LD, "y, z" },
        { ADD, "A, z" }, { ADD, "A, z" }, { ADD, "A, z" }, { ADD, "A, z" }, { ADD, "A, z" }, { ADD, "A, z" }, { ADD, "A, z" }, { ADD, "A, z" },
        { ADC, "A, z" }, { ADC, "A, z" }, { ADC, "A, z" }, { ADC, "A, z" }, { ADC, "A, z" }, { ADC, "A, z" }, { ADC, "A, z" }, { ADC, "A, z" },
        { SUB, "z" }, { SUB, "z" }, { SUB, "z" }, { SUB, "z" }, { SUB, "z" }, { SUB, "z" }, { SUB, "z" }, { SUB, "z" },
        { SBC, "A, z" }, { SBC, "A, z" }, { SBC, "A, z" }, { SBC, "A, z" }, { SBC, "A, z" }, { SBC, "A, z" }, { SBC, "A, z" }, { SBC, "A, z" },
        { AND, "z" }, { AND, "z" }, { AND, "z" }, { AND, "z" }, { AND, "z" }, { AND, "z" }, { AND, "z" }, { AND, "z" },
        { XOR, "z" }, { XOR, "z" }, { XOR, "z" }, { XOR, "z" }, { XOR, "z" }, { XOR, "z" }, { XOR, "z" }, { XOR, "z" },
        { OR, "z" }, { OR, "z" }, { OR, "z" }, { OR, "z" }, { OR, "z" }, { OR, "z" }, { OR, "z" }, { OR, "z" },
        { CP, "z" }, { CP, "z" }, { CP, "z" }, { CP, "z" }, { CP, "z" }, { CP, "z" }, { CP, "z" }, { CP, "z" },
        { RET, "f" }, { POP, "i" }, { JP, "f, w" }, { JP, "w" }, { CALL, "f, w" }, { PUSH, "i" }, { ADD, "A, b" }, { RST, "t" },
        { RET, "f" }, { RET, NULL }, { JP, "f, w" }, { DB, NULL }, { CALL, "f, w" }, { CALL, "w" }, { ADC, "A, b" }, { RST, "t" },
        { RET, "f" }, { POP, "i" }, { JP, "f, w" }, { OUT, "(b), A" }, { CALL, "f, w" }, { PUSH, "i" }, { SUB, "b" }, { RST, "t" },
        { RET, "f" }, { EXX, NULL }, { JP, "f, w" }, { IN, "A, (b)" }, { CALL, "f, w" }, { DB, NULL }, { SBC, "A, b" }, { RST, "t" },
        { RET, "f" }, { POP, "i" }, { JP, "f, w" }, { EX, "(SP), HL" }, { CALL, "f, w" }, { PUSH, "i" }, { AND, "b" }, { RST, "t" },
        { RET, "f" }, { JP, "(HL)" }, { JP, "f, w" }, { EX, "DE, HL" }, { CALL, "f, w" }, { DB, NULL }, { XOR, "b" }, { RST, "t" },
        { RET, "f" }, { POP, "i" }, { JP, "f, w" }, { DI, NULL }, { CALL, "f, w" }, { PUSH, "i" }, { OR, "b" }, { RST, "t" },
        { RET, "f" }, { LD, "SP, HL" }, { JP, "f, w" }, { EI, NULL }, { CALL, "f, w" }, { DB, NULL }, { CP, "b" }, { RST, "t" }
    };
    struct MM_Decode cb_prefix[0x100] = {
        { RLC, "z" }, { RLC, "z" }, { RLC, "z" }, { RLC, "z" }, 
        { RLC, "z" }, { RLC, "z" }, { RLC, "z" }, { RLC, "z" },
        { RRC, "z" }, { RRC, "z" }, { RRC, "z" }, { RRC, "z" },
        { RRC, "z" }, { RRC, "z" }, { RRC, "z" }, { RRC, "z" },
        { RL, "z" }, { RL, "z" }, { RL, "z" }, { RL, "z" },
        { RL, "z" }, { RL, "z" }, { RL, "z" }, { RL, "z" },
        { RR, "z" }, { RR, "z" }, { RR, "z" }, { RR, "z" },
        { RR, "z" }, { RR, "z" }, { RR, "z" }, { RR, "z" },
        { SLA, "z" }, { SLA, "z" }, { SLA, "z" }, { SLA, "z" },
        { SLA, "z" }, { SLA, "z" }, { SLA, "z" }, { SLA, "z" },
        { SRA, "z" }, { SRA, "z" }, { SRA, "z" }, { SRA, "z" },
        { SRA, "z" }, { SRA, "z" }, { SRA, "z" }, { SRA, "z" },
        { SLL, "z" }, { SLL, "z" }, { SLL, "z" }, { SLL, "z" },
        { SLL, "z" }, { SLL, "z" }, { SLL, "z" }, { SLL, "z" },
        { SRL, "z" }, { SRL, "z" }, { SRL, "z" }, { SRL, "z" },
        { SRL, "z" }, { SRL, "z" }, { SRL, "z" }, { SRL, "z" },
        { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" },
        { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" },
        { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" },
        { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" },
        { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" },
        { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" },
        { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" },
        { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" },
        { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" },
        { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" },
        { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" },
        { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" },
        { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" },
        { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" },
        { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" },
        { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" }, { BIT, "s, z" },
        { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" },
        { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" },
        { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" },
        { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" },
        { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" },
        { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" },
        { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" },
        { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" },
        { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" },
        { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" },
        { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" },
        { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" },
        { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" },
        { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" },
        { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" },
        { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" }, { RES, "s, z" },
        { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" },
        { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" },
        { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" },
        { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" },
        { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" },
        { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" },
        { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" },
        { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" },
        { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" },
        { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" },
        { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" },
        { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" },
        { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" },
        { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" },
        { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" },
        { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" }, { SET, "s, z" }
    };
    struct MM_Decode ddfd_prefix[0x100] = {
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { ADD, "x, p" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { ADD, "x, p" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { LD, "x, w" }, { LD, "(w), x" }, { INC, "x" },
        { INC, "xh" }, { DEC, "xh" }, { LD, "xh, b" }, { DB, "?" },
        { DB, "?" }, { ADD, "x, x" }, { LD, "x, (w)" }, { DEC, "x" },
        { INC, "xl" }, { DEC, "xl" }, { LD, "xl, b" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { INC, "X" }, { DEC, "X" }, { LD, "X, b" }, { DB, "?" },
        { DB, "?" }, { ADD, "x, p" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { LD, "y, xh" }, { LD, "y, xl" }, { LD, "y, X" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { LD, "y, xh" }, { LD, "y, xl" }, { LD, "y, X" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { LD, "y, xh" }, { LD, "y, xl" }, { LD, "y, X" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { LD, "y, xh" }, { LD, "y, xl" }, { LD, "y, X" }, { DB, "?" },
        { LD, "xh, z" }, { LD, "xh, z" }, { LD, "xh, z" }, { LD, "xh, z" },
        { LD, "xh, xh" }, { LD, "xh, xl" }, { LD, "y, X" }, { LD, "xh, z" },
        { LD, "xl, z" }, { LD, "xl, z" }, { LD, "xl, z" }, { LD, "xl, z" },
        { LD, "xl, xh" }, { LD, "xl, xl" }, { LD, "y, X" }, { LD, "xl, z" },
        { LD, "X, z" }, { LD, "X, z" }, { LD, "X, z" }, { LD, "X, z" },
        { LD, "X, z" }, { LD, "X, z" }, { DB, "?" }, { LD, "X, z" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { LD, "y, xh" }, { LD, "y, xl" }, { LD, "y, X" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { ADD, "A, xh" }, { ADD, "A, xl" }, { ADD, "A, X" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { ADC, "A, xh" }, { ADC, "A, xl" }, { ADC, "A, X" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { SUB, "xh" }, { SUB, "xl" }, { SUB, "X" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { SBC, "A, xh" }, { SBC, "A, xl" }, { SBC, "A, X" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { AND, "xh" }, { AND, "xl" }, { AND, "X" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { XOR, "xh" }, { XOR, "xl" }, { XOR, "X" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { OR, "xh" }, { OR, "xl" }, { OR, "X" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { CP, "xh" }, { CP, "xl" }, { CP, "X" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, NULL },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { POP, "x" }, { DB, "?" }, { EX, "(SP), x" },
        { DB, "?" }, { PUSH, "x" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { JP, "(x)" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { LD, "SP, x" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" }
    };
    struct MM_Decode ed_prefix[0x100] = {
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { IN, "y, (C)" }, { OUT, "(C), y" }, { SBC, "HL, p" }, { LD, "(w), p" },
        { NEG, NULL }, { RETN, NULL }, { IM, "0" }, { LD, "I, A" },
        { IN, "y, (C)" }, { OUT, "(C), y" }, { ADC, "HL, p" }, { LD, "p, (w)" },
        { NEG, NULL }, { RETI, NULL }, { IM, "0" }, { LD, "R, A" },
        { IN, "y, (C)" }, { OUT, "(C), y" }, { SBC, "HL, p" }, { LD, "(w), p" },
        { NEG, NULL }, { RETN, NULL }, { IM, "1" }, { LD, "A, I" },
        { IN, "y, (C)" }, { OUT, "(C), y" }, { ADC, "HL, p" }, { LD, "p, (w)" },
        { NEG, NULL }, { RETI, NULL }, { IM, "2" }, { LD, "A, R" },
        { IN, "y, (C)" }, { OUT, "(C), y" }, { SBC, "HL, p" }, { LD, "(w), p" },
        { NEG, NULL }, { RETN, NULL }, { IM, "0" }, { RRD, "A, (HL)" },
        { IN, "y, (V)" }, { OUT, "(C), y" }, { ADC, "HL, p" }, { LD, "p, (w)" },
        { NEG, NULL }, { RETI, NULL }, { IM, "0" }, { RLD, "A, (HL)" },
        { IN, "0, (C)" }, { OUT, "(C), 0" }, { SBC, "HL, p" }, { LD, "(w), p" },
        { NEG, NULL }, { RETN, NULL }, { IM, "1" }, { DB, "?" },
        { IN, "y, (C)" }, { OUT, "(C), y" }, { ADC, "HL, p" }, { LD, "p, (w)" },
        { NEG, NULL }, { RETI, NULL }, { IM, "2" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { LDI, NULL }, { CPI, NULL }, { INI, NULL }, { OUTI, NULL },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { LDD, NULL }, { CPD, NULL }, { IND, NULL }, { OUTD, NULL },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { LDIR, NULL }, { CPIR, NULL }, { INIR, NULL }, { OTIR, NULL },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { LDDR, NULL }, { CPDR, NULL }, { INDR, NULL }, { OTDR, NULL },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" },
        { DB, "?" }, { DB, "?" }, { DB, "?" }, { DB, "?" }
    };
    struct MM_Decode ddfdcb_prefix[0x100] = {
        { RLC,"Y, z" }, { RLC,"Y, z" }, { RLC,"Y, z" }, { RLC,"Y, z" },
        { RLC,"Y, z" }, { RLC,"Y, z" }, { RLC,"Y" }, { RLC,"Y, z" },
        { RRC,"Y, z" }, { RRC,"Y, z" }, { RRC,"Y, z" }, { RRC,"Y, z" },
        { RRC,"Y, z" }, { RRC,"Y, z" }, { RRC,"Y" }, { RRC,"Y, z" },
        { RL,"Y, z" }, { RL,"Y, z" }, { RL,"Y, z" }, { RL,"Y, z" },
        { RL,"Y, z" }, { RL,"Y, z" }, { RL,"Y" }, { RL,"Y, z" },
        { RR,"Y, z" }, { RR,"Y, z" }, { RR,"Y, z" }, { RR,"Y, z" },
        { RR,"Y, z" }, { RR,"Y, z" }, { RR,"Y" }, { RR,"Y, z" },
        { SLA,"Y, z" }, { SLA,"Y, z" }, { SLA,"Y, z" }, { SLA,"Y, z" },
        { SLA,"Y, z" }, { SLA,"Y, z" }, { SLA,"Y" }, { SLA,"Y, z" },
        { SRA,"Y, z" }, { SRA,"Y, z" }, { SRA,"Y, z" }, { SRA,"Y, z" },
        { SRA,"Y, z" }, { SRA,"Y, z" }, { SRA,"Y" }, { SRA,"Y, z" },
        { SLL,"Y, z" }, { SLL,"Y, z" }, { SLL,"Y, z" }, { SLL,"Y, z" },
        { SLL,"Y, z" }, { SLL,"Y, z" }, { SLL,"Y" }, { SLL,"Y, z" },
        { SRL,"Y, z" }, { SRL,"Y, z" }, { SRL,"Y, z" }, { SRL,"Y, z" },
        { SRL,"Y, z" }, { SRL,"Y, z" }, { SRL,"Y" }, { SRL,"Y, z" },
        { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y, z" },
        { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y" }, { BIT,"s, Y, z" },
        { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y, z" },
        { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y" }, { BIT,"s, Y, z" },
        { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y, z" },
        { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y" }, { BIT,"s, Y, z" },
        { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y, z" },
        { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y" }, { BIT,"s, Y, z" },
        { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y, z" },
        { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y" }, { BIT,"s, Y, z" },
        { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y, z" },
        { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y" }, { BIT,"s, Y, z" },
        { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y, z" },
        { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y" }, { BIT,"s, Y, z" },
        { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y, z" },
        { BIT,"s, Y, z" }, { BIT,"s, Y, z" }, { BIT,"s, Y" }, { BIT,"s, Y, z" },
        { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y, z" },
        { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y" }, { RES,"s, Y, z" },
        { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y, z" },
        { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y" }, { RES,"s, Y, z" },
        { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y, z" },
        { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y" }, { RES,"s, Y, z" },
        { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y, z" },
        { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y" }, { RES,"s, Y, z" },
        { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y, z" },
        { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y" }, { RES,"s, Y, z" },
        { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y, z" },
        { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y" }, { RES,"s, Y, z" },
        { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y, z" },
        { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y" }, { RES,"s, Y, z" },
        { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y, z" },
        { RES,"s, Y, z" }, { RES,"s, Y, z" }, { RES,"s, Y" }, { RES,"s, Y, z" },
        { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y, z" },
        { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y" }, { SET,"s, Y, z" },
        { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y, z" },
        { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y" }, { SET,"s, Y, z" },
        { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y, z" },
        { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y" }, { SET,"s, Y, z" },
        { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y, z" },
        { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y" }, { SET,"s, Y, z" },
        { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y, z" },
        { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y" }, { SET,"s, Y, z" },
        { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y, z" },
        { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y" }, { SET,"s, Y, z" },
        { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y, z" },
        { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y" }, { SET,"s, Y, z" },
        { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y, z" },
        { SET,"s, Y, z" }, { SET,"s, Y, z" }, { SET,"s, Y" }, { SET,"s, Y, z" }
    };

    char* hex(char *dst, u8 byte){
        byte = (byte >> 4) | (byte << 4);
        if (byte < 0x10)
            *dst++ = '0';
        do {
            *dst++ = byte % 0x10 <= 0x09 ? byte % 0x10 + '0' : byte % 0x10 + 'A' - 0x0A;
        }while (byte /= 0x10);
        return dst;
    }

    char* dec(char *dst, u16 num){
        char *start = dst;
        do {
            *start++ = '0' + num % 10;
        }while (num /= 10);
        while (dst != start)
            *dst++ = *start--;
        return dst;
    }
    char* copy(char *dst, const char *src){
        while (*src)
            *dst++ = *src++;
        return dst;
    }
            
    u16 decode(char *opcode, char *operand, char *info, u16 ptr, Memory *memory, Z80_State &cpu){
        const char *reg_ix = NULL;
        MM_Decode *decode = NULL;
        s8 offset = 0;
        u8 byte = memory->read_byte_ex(ptr++);
        //printf("PTR: %04x -> %02x\n", ptr -1, byte);
        switch (byte){
            case 0xCB:
                decode = &cb_prefix[byte];
                break;
            case 0xDD:
            case 0xFD:
                reg_ix = byte == 0xDD ? reg_16i[0] : reg_16i[1];
                byte = memory->read_byte_ex(ptr++);
                if (byte == 0xCB){
                    offset = (s8)memory->read_byte_ex(ptr++);
                    byte = memory->read_byte_ex(ptr++);
                    decode = &ddfdcb_prefix[byte];
                }else
                    decode = ddfd_prefix[byte].id == Opcode::DB ? &no_prefix[byte] : &ddfd_prefix[byte];
                break;
            case 0xED:
                byte = memory->read_byte_ex(ptr++);
                decode = &ed_prefix[byte];
                break;
            default:
                decode = &no_prefix[byte];
                break;
        }
        opcode = copy(opcode, name[decode->id]);
        if (decode->operand){
            for (int i = 0; decode->operand[i]; i++){
                switch (decode->operand[i]){
                    case 'p':
                        operand = copy(operand, reg_16a[((byte >> 4) & 0x03)]);
                        break;
                    case 'i':
                        operand = copy(operand, reg_16b[((byte >> 4) & 0x03)]);
                        break;
                    case 'b': // LD B, #NN
                        *operand++ = '#';
                        operand = hex(operand, memory->read_byte_ex(ptr++));
                        break;
                    case 'w': // LD BC, #NNNN
                        *operand++ = '#';
                        operand = hex(operand, memory->read_byte_ex(ptr + 1));
                        operand = hex(operand, memory->read_byte_ex(ptr));
                        ptr += 2;
                        break;
                   case 'l': // DJNZ, JR.
                        *operand++ = '#';
                        offset = (s8)memory->read_byte_ex(ptr++);
                        operand = hex(operand, (ptr + offset) >> 8);
                        operand = hex(operand, (ptr + offset) & 0xFF);
                        break;
                   case 'y':
                        operand = copy(operand, opr_8[(byte >> 3) & 0x07]);
                        break;
                   case 'z':
                        operand = copy(operand, opr_8[byte & 0x07]);
                        break;
                   case 'f':
                        operand = copy(operand, flags[(byte >> 3) & 0x07]);
                        break;
                   case 'c':
                        operand = copy(operand, flags[((byte >> 3) & 0x07) - 4]); //?
                        break;
                   case 't':
                        *operand++ = '#';
                        operand = hex(operand, byte & (0x07 << 3));
                        break;
                   case 's':
                        operand = dec(operand, (byte >> 3) & 0x07);
                        break;
                   case 'x':
                        operand = copy(operand, reg_ix);
                        break;
                   case 'X': // Read offset
                        offset = (s8)memory->read_byte_ex(ptr++);
                        break;
                   case 'Y': // (IX + s)
                        *operand++ = '(';
                        operand = copy(operand, reg_ix);
                        *operand++ = ' ';
                        *operand++ = offset < 0 ? '-' : '+';
                        *operand++ = '#';
                        operand = hex(operand, offset & 0x7F);
                        *operand++ = ')';
                        break;
                   default:
                        *operand++ = decode->operand[i];
                        break;
                }
            }
        }
        *opcode = 0x00;
        *operand = 0x00;
        return ptr;
    }
};
