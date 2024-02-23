
#define LINE_WIDTH          36
#define OPCODE_POS          24

namespace Disasm {
    enum OPCODE {
        zADC,  zADD,  zAND,  zBIT,  zCALL, zCCF,  zCP,   zCPD,
        zCPDR, zCPI,  zCPIR, zCPL,  zDAA,  zDB,   zDEC,  zDI,
        zDJNZ, zEI,   zEX,   zEXX,  zHLT,  zIM,   zIN,   zINC,
        zIND,  zINDR, zINI,  zINIR, zJP,   zJR,   zLD,   zLDD,
        zLDDR, zLDI,  zLDIR, zNEG,  zNOP,  zOR,   zOTDR, zOTIR,
        zOUT,  zOUTD, zOUTI, zPOP,  zPUSH, zRES,  zRET,  zRETI,
        zRETN, zRL,   zRLA,  zRLC,  zRLCA, zRLD,  zRR,   zRRA,
        zRRC,  zRRCA, zRRD,  zRST,  zSBC,  zSCF,  zSET,  zSLA,
        zSLL,  zSRA,  zSRL,  zSUB,  zXOR
    };
    enum REG {
        IX, IY
    };
    struct Table {
        OPCODE opcode_id;
        const char *p_data;
    };
    unsigned short line(char *p_dst, unsigned short pointer, Z80 *p_cpu, Memory *p_memory, bool info);
};
