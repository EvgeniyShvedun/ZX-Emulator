
namespace Disasm {
    enum Opcode {
        ADC, ADD, AND, BIT, CALL, CCF, CP, CPD,
        CPDR, CPI, CPIR, CPL, DAA, DB, DEC, DI,
        DJNZ, EI, EX, EXX, HLT, IM, IN, INC,
        IND, INDR, INI, INIR, JP, JR, LD, LDD,
        LDDR, LDI, LDIR, NEG, NOP, OR, OTDR, OTIR,
        OUT, OUTD, OUTI, POP, PUSH, RES, RET, RETI,
        RETN, RL, RLA, RLC, RLCA, RLD, RR, RRA,
        RRC, RRCA, RRD, RST, SBC, SCF, SET, SLA,
        SLL, SRA, SRL, SUB, XOR
    };
    struct MM_Decode {
        Opcode id;
        const char *operand;
    };
    char* hex(char *dst, u8 byte);
    char* dec(char *dst, u16 word);
    char* copy(char *dst, const char *src);
    u16 decode(char *opcode, char *operand, char *info,  u16 ptr, Memory *memory, Z80_State &cpu);
};
