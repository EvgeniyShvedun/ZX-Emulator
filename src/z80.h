#define CF                  0x01            // Carry
#define NF                  0x02            // ADD(0) or SUB(1)
#define PF                  0x04            // Parity and Overflow
#define F3                  0x08            // Bit 3 of result
#define HF                  0x10            // Half carry used DAA
#define F5                  0x20            // Bit 5 of result
#define ZF                  0x40            // Zero
#define SF                  0x80            // Negative. Bit 7 of result.

#define LD_IR_PF_CLK        18              // Time while PF-flag was not change (Undocumented behievior: LD A, I and LD A, R).
#define Z80_TRAP_OPCODE     0x5B            // LD E, E. Opcode for "trap on exec".

#define Reg16(reg16)\
    union {\
        unsigned short reg16;\
        struct {\
            unsigned char reg16##l;\
            unsigned char reg16##h;\
        };\
    };

#define Reg88(h8, l8)\
    union {\
        unsigned short h8##l8;\
        union {\
            struct {\
                unsigned char h8##l8##l;\
                unsigned char h8##l8##h;\
            };\
            struct {\
                unsigned char l8;\
                unsigned char h8;\
            };\
        };\
    };

#define time(t)\
    clk += t

#define NOP\
    time(4);

#define LD_R_R(r0, r1)\
    r0 = r1;\
    time(4);

#define LD_XR_R(r16, r8)\
    p_memory->write_byte(r16, r8, clk);\
    time(7);
// Use memptr. Only LD (BC/DE), A.
#define LD_XR_A(r16)\
    memptrl = r16##l + 1;\
    memptrh = a;\
    LD_XR_R(r16, a);
#define LD_XR_N(r16){\
    unsigned char byte = p_memory->read_byte(pc++, clk);\
    p_memory->write_byte(r16, byte, clk);\
    time(10);\
}

#define LD_R_XR(r8, r16)\
    r8 = p_memory->read_byte(r16, clk);\
    time(7);
// Use memptr. Only for LD A, (BC/DE).
#define LD_A_XR(r16)\
    memptr = r16 + 1;\
    LD_R_XR(a, r16);
#define LD_RR_RR(rr0, rr1)\
    rr0 = rr1;\
    time(6);
#define LD_RR_NN(r16)\
    r16##l = p_memory->read_byte(pc++, clk);\
    r16##h = p_memory->read_byte(pc++, clk);\
    time(10);
// Increment memptr only once.
#define LD_MM_RR(r16)\
    memptrl = p_memory->read_byte(pc++, clk);\
    memptrh = p_memory->read_byte(pc++, clk);\
    p_memory->write_byte(memptr++, r16##l, clk);\
    p_memory->write_byte(memptr, r16##h, clk);\
    time(16);
#define LD_RR_MM(r16)\
    memptrl = p_memory->read_byte(pc++, clk);\
    memptrh = p_memory->read_byte(pc++, clk);\
    r16##l = p_memory->read_byte(memptr++, clk);\
    r16##h = p_memory->read_byte(memptr, clk);\
    time(16);
#define LD_MM_A\
     memptrl = p_memory->read_byte(pc++, clk);\
     memptrh = p_memory->read_byte(pc++, clk);\
     p_memory->write_byte(memptr++, a, clk);\
     memptrh = a;\
     time(13);
#define LD_A_MM\
    memptrl = p_memory->read_byte(pc++, clk);\
    memptrh = p_memory->read_byte(pc++, clk);\
    a = p_memory->read_byte(memptr++, clk);\
    time(13);

#define inc8(value)\
    f = flag_inc[value++] | (f & CF);
#define dec8(value)\
    f = flag_dec[value--] | (f & CF);

#define INC_RR(r16)\
    r16++;\
    time(6);
#define DEC_RR(r16)\
    r16--;\
    time(6);
#define INC(r8)\
    inc8(r8);\
    time(4);
#define DEC(r8)\
    dec8(r8);\
    time(4);
#define INC_XR(r16){\
    unsigned char byte = p_memory->read_byte(r16, clk);\
    inc8(byte);\
    p_memory->write_byte(r16, byte, clk);\
    time(11);\
}
#define DEC_XR(r16){\
    unsigned char byte = p_memory->read_byte(r16, clk);\
    dec8(byte);\
    p_memory->write_byte(r16, byte, clk);\
    time(11);\
}

#define ADD16(rr0, rr1)\
    memptr = rr0 + 1;\
    f &= (SF | ZF | PF);\
    f |= ((((rr0 & 0xFFF) + (rr1 & 0xFFF)) >> 8) & HF);\
    f |= (((unsigned int)rr0 + rr1) >> 16) & CF;\
    f |= ((rr0 + rr1) >> 8) & (F3 | F5);\
    rr0 += rr1;\
    time(11);


#define JR_N \
    pc += (signed char)p_memory->read_byte(pc, clk) + 1;\
    memptr = pc;\
    time(12);

#define JR_CND_N(cnd)\
    if (cnd){\
        JR_N\
    }else{\
        pc++;\
        time(7);\
    }

// 8 bit arichmetic.
#define add8(value){\
    unsigned char byte = value;\
    f = flag_adc[a + byte * 0x100];\
    a += byte;\
}
#define adc8(value){\
    unsigned char byte = value;\
    unsigned char cf = f & CF;\
    f = flag_adc[a + byte * 0x100 + 0x10000 * cf];\
    a += byte + cf;\
}
#define sub8(value){\
    unsigned char byte = value;\
    f = flag_sbc[a * 0x100 + byte];\
    a -= byte;\
}
#define sbc8(value){\
    unsigned char byte = value;\
    unsigned char cf = f & CF;\
    f = flag_sbc[a * 0x100 + byte + 0x10000 * cf];\
    a -= byte + cf;\
}
#define and8(value)\
    a &= value;\
    f = flag_sz53p[a] | HF;
#define xor8(value)\
    a ^= value;\
    f = flag_sz53p[a];
#define or8(value)\
    a |= value;\
    f = flag_sz53p[a];
#define cp8(value)\
    f = flag_cp[a * 0x100 + value];

#define ADD(r8)\
    add8(r8);\
    time(4);
#define ADD_XR(r16)\
    add8(p_memory->read_byte(r16, clk));\
    time(7);
#define ADD_XS(r16)\
    memptr = (signed char)p_memory->read_byte(pc++, clk);\
    memptr += r16;\
    add8(p_memory->read_byte(memptr, clk));\
    time(15);

#define ADC(r8)\
    adc8(r8);\
    time(4);
#define ADC_XR(r16)\
    adc8(p_memory->read_byte(r16, clk));\
    time(7);
#define ADC_XS(r16)\
    memptr = (signed char)p_memory->read_byte(pc++, clk);\
    memptr += r16;\
    adc8(p_memory->read_byte(memptr, clk));\
    time(15);

#define SUB(r8)\
    sub8(r8);\
    time(4);
#define SUB_XR(r16)\
    sub8(p_memory->read_byte(r16, clk));\
    time(7);
#define SUB_XS(r16)\
    memptr = (signed char)p_memory->read_byte(pc++, clk);\
    memptr += r16;\
    sub8(p_memory->read_byte(memptr, clk));\
    time(15);

#define SBC(r8)\
    sbc8(r8);\
    time(4);
#define SBC_XR(r16)\
    sbc8(p_memory->read_byte(r16, clk));\
    time(7);
#define SBC_XS(r16)\
    memptr = (signed char)p_memory->read_byte(pc++, clk);\
    memptr += r16;\
    sbc8(p_memory->read_byte(memptr, clk));\
    time(15);

#define RLA {\
        unsigned char byte = a >> 7;\
        a = (a << 1) | (f & CF);\
        f = (f & (SF | ZF | PF)) | (a & (F3 | F5)) | byte;\
    }\
    time(4);

#define RRA {\
        unsigned char byte = a & CF;\
        a = (a >> 1) | (f << 7);\
        f = (f & (SF | ZF | PF)) | (a & (F3 | F5)) | byte;\
    }\
    time(4);

#define DAA {\
        unsigned char byte;\
        if (f & HF || (a & 0xF) > 9)\
            byte = 0x06;\
        else\
            byte = 0x00;\
        if (f & CF || (a > 0x99))\
            byte |= 0x60;\
        unsigned char tmp = a;\
        if (f & NF){\
            f &= NF;\
            a += -byte;\
        }else\
            a += byte;\
        f |= flag_parity[a];\
        f |= (a ^ tmp) & HF;\
        f |= a & (SF | F3 | F5);\
        if (!a)\
            f |= ZF;\
        if (byte & 0x60)\
            f |= CF;\
    }\
    time(4);

#define AND(r8)\
    and8(r8);\
    time(4);
#define AND_XR(r16)\
    and8(p_memory->read_byte(r16, clk));\
    time(7);
#define AND_XS(r16)\
    memptr = (signed char)p_memory->read_byte(pc++, clk);\
    memptr += r16;\
    and8(p_memory->read_byte(memptr, clk));\
    time(15);

#define XOR(r8)\
    xor8(r8);\
    time(4);
#define XOR_XR(r16)\
    xor8(p_memory->read_byte(r16, clk));\
    time(7);
#define XOR_XS(r16)\
    memptr = (signed char)p_memory->read_byte(pc++, clk);\
    memptr += r16;\
    xor8(p_memory->read_byte(memptr, clk));\
    time(15);

#define OR(r8)\
    or8(r8);\
    time(4);
#define OR_XR(r16)\
    or8(p_memory->read_byte(r16, clk));\
    time(7);
#define OR_XS(r16)\
    memptr = (signed char)p_memory->read_byte(pc++, clk);\
    memptr += r16;\
    or8(p_memory->read_byte(memptr, clk));\
    time(15);

#define CP(r8)\
    cp8(r8);\
    time(4);
#define CP_XR(r16)\
    cp8(p_memory->read_byte(r16, clk));\
    time(7);
#define CP_XS(r16)\
    memptr = (signed char)p_memory->read_byte(pc++, clk);\
    memptr += r16;\
    cp8(p_memory->read_byte(memptr, clk));\
    time(15);

#define PUSH(r16)\
    p_memory->write_byte(--sp, r16##h, clk);\
    p_memory->write_byte(--sp, r16##l, clk);\
    time(11);

#define POP(r16)\
    r16##l = p_memory->read_byte(sp++, clk);\
    r16##h = p_memory->read_byte(sp++, clk);\
    time(10);

#define EX_RR_RR(rr0, rr1)\
    rr0 ^= rr1;\
    rr1 ^= rr0;\
    rr0 ^= rr1;\
    time(4);

// EX (SP), RR
#define EX_SP_RR(r16)\
    memptrl = p_memory->read_byte(sp, clk);\
    p_memory->write_byte(sp, r16##l, clk);\
    r16##l = memptrl;\
    memptrh = p_memory->read_byte(sp + 1, clk);\
    p_memory->write_byte(sp + 1, r16##h, clk);\
    r16##h = memptrh;\
    time(19);

#define JP_NN\
    memptrl = p_memory->read_byte(pc++, clk);\
    memptrh = p_memory->read_byte(pc, clk);\
    pc = memptr;\
    time(10);

#define JP_RR(r16)\
    pc = r16;\
    time(4);

#define JP_CND_NN(cnd)\
    memptrl = p_memory->read_byte(pc++, clk);\
    memptrh = p_memory->read_byte(pc, clk);\
    if (cnd)\
        pc = memptr;\
    else\
        pc++;\
    time(10);

#define CALL_NN\
    memptrl = p_memory->read_byte(pc++, clk);\
    memptrh = p_memory->read_byte(pc++, clk);\
    p_memory->write_byte(--sp, pch, clk);\
    p_memory->write_byte(--sp, pcl, clk);\
    pc = memptr;\
    time(17);
#define CALL_CND_NN(cnd)\
    memptrl = p_memory->read_byte(pc++, clk);\
    memptrh = p_memory->read_byte(pc++, clk);\
    if (cnd){\
        p_memory->write_byte(--sp, pch, clk);\
        p_memory->write_byte(--sp, pcl, clk);\
        pc = memptr;\
        time(17);\
    }else{\
        time(10);\
    }
#define RST(ptr)\
    p_memory->write_byte(--sp, pch, clk);\
    p_memory->write_byte(--sp, pcl, clk);\
    pc = memptr = ptr;\
    time(11);

#define RET\
    memptrl = p_memory->read_byte(sp++, clk);\
    memptrh = p_memory->read_byte(sp++, clk);\
    pc = memptr;\
    time(10);
#define RET_CND(cnd)\
    if (cnd){\
        memptrl = p_memory->read_byte(sp++, clk);\
        memptrh = p_memory->read_byte(sp++, clk);\
        pc = memptr;\
        time(11);\
    }else{\
        time(5);\
    }

#define OUT_N_A\
    memptrl = p_memory->read_byte(pc++, clk);\
    memptrh = a;\
    p_io->write(memptr++, a, clk);\
    time(11);

#define IN_A_N\
    memptrh = a;\
    memptrl = p_memory->read_byte(pc++, clk);\
    a = p_io->read(memptr++, clk);\
    time(11);
// ............

// CB PREFIX. Clock time with all prefixes.
#define RLC(r8)\
    r8 = (r8 << 1) | (r8 >> 7);\
    f = flag_sz53p[r8] | (r8 & CF);\
    time(8);
#define RLC_XR(r16){\
        unsigned char byte = p_memory->read_byte(r16, clk);\
        byte = (byte << 1) | (byte >> 7);\
        f = flag_sz53p[byte] | (byte & CF);\
        p_memory->write_byte(r16, byte, clk);\
    }\
    time(15);

#define RRC(r8)\
    f = r8 & CF;\
    r8 = (r8 >> 1) | (r8 << 7);\
    f |= flag_sz53p[r8];\
    time(8);
#define RRC_XR(r16){\
        unsigned char byte = p_memory->read_byte(r16, clk);\
        f = byte & CF;\
        byte = (byte >> 1) | (byte << 7);\
        f |= flag_sz53p[byte];\
        p_memory->write_byte(r16, byte, clk);\
    }\
    time(15);

#define RL(r8){\
        unsigned char cf = r8 >> 7;\
        r8 = (r8 << 1) | (f & CF);\
        f = flag_sz53p[r8] | cf;\
    }\
    time(8);
#define RL_XR(r16){\
        unsigned char byte = p_memory->read_byte(r16, clk);\
        unsigned char cf = byte >> 7;\
        byte = (byte << 1) | (f & CF);\
        f = flag_sz53p[byte] | cf;\
        p_memory->write_byte(r16, byte, clk);\
    }\
    time(15);

#define RR(r8){\
        unsigned char cf = r8 & CF;\
        r8 = (r8 >> 1) | (f << 7);\
        f = flag_sz53p[r8] | cf;\
    }\
    time(8);
#define RR_XR(r16){\
        unsigned char byte = p_memory->read_byte(r16, clk);\
        unsigned char cf = byte & CF;\
        byte = (byte >> 1) | (f << 7);\
        f = flag_sz53p[byte] | cf;\
        p_memory->write_byte(r16, byte, clk);\
    }\
    time(15);

#define SLA(r8)\
    f = r8 >> 7;\
    r8 <<= 1;\
    f |= flag_sz53p[r8];\
    time(8);

#define SLA_XR(r16){\
        unsigned char byte = p_memory->read_byte(r16, clk);\
        f = flag_sz53p[byte << 1] | (byte >> 7);\
        p_memory->write_byte(r16, byte << 1, clk);\
    }\
    time(15);

#define SRA(r8)\
    f = r8 & CF;\
    r8 = (r8 & 0x80) | (r8 >> 1);\
    f |= flag_sz53p[r8];\
    time(8);
#define SRA_XR(r16){\
        unsigned char byte = p_memory->read_byte(r16, clk);\
        f = byte & CF;\
        byte = (byte & 0x80) | (byte >> 1);\
        f |= flag_sz53p[byte];\
        p_memory->write_byte(r16, byte, clk);\
    }\
    time(15);

#define SLL(r8)\
    f = r8 >> 7;\
    r8 = (r8 << 1) | 0x01;\
    f |= flag_sz53p[r8];\
    time(8);
#define SLL_XR(r16){\
        unsigned char byte = p_memory->read_byte(r16, clk);\
        f = byte >> 7;\
        byte = (byte << 1) | 0x01;\
        f |= flag_sz53p[byte];\
        p_memory->write_byte(r16, byte, clk);\
    }\
    time(15);

#define SRL(r8)\
    f = r8 & CF;\
    r8 >>= 1;\
    f |= flag_sz53p[r8];\
    time(8);
#define SRL_XR(r16){\
        unsigned char byte = p_memory->read_byte(r16, clk);\
        f = (byte & CF) | flag_sz53p[byte >> 1];\
        p_memory->write_byte(r16, byte >> 1, clk);\
    }\
    time(15);

#define BIT(n, r8)\
    f = flag_sz53p[r8 & (1 << n)] | HF | (r8 & (F3 | F5)) | (f & CF);\
    time(8);
#define BIT_XR(n, r16){\
        unsigned char byte = p_memory->read_byte(r16, clk);\
        f = flag_sz53p[byte & (1 << n)] | HF | (f & CF);\
        f &= ~(F3 | F5);\
        f |= memptrh & (F3 | F5);\
    }\
    time(12);

#define RES(n, r8)\
    r8 &= ~(1 << n);\
    time(8);
#define RES_XR(n, r16){\
        unsigned char byte = p_memory->read_byte(r16, clk);\
        p_memory->write_byte(r16, byte & ~(1 << n), clk);\
    }\
    time(15);

#define SET(n, r8)\
    r8 |= (1 << n);\
    time(8);
#define SET_XR(n, r16){\
        unsigned char byte = p_memory->read_byte(r16, clk);\
        p_memory->write_byte(r16, byte | (1 << n), clk);\
    }\
    time(15);


// DD/FD PREFIX.
// Instruction set clock time without DD/FD prefix time.
// LD (IX+S), N
#define LD_XS_N(r16)\
    memptr = (signed char)p_memory->read_byte(pc++, clk);\
    memptr += r16;\
    p_memory->write_byte(memptr, p_memory->read_byte(pc++, clk), clk);\
    time(15);
// LD B, (IX+S)
#define LD_R_XS(r8, r16)\
    memptr = (signed char)p_memory->read_byte(pc++, clk);\
    memptr += r16;\
    r8 = p_memory->read_byte(memptr, clk);\
    time(15);
// LD (IX+S), B
#define LD_XS_R(r16, r8)\
    memptr = (signed char)p_memory->read_byte(pc++, clk);\
    memptr += r16;\
    p_memory->write_byte(memptr, r8, clk);\
    time(15);

// INC (IX+S)
#define INC_XS(r16){\
    memptr = (signed char)p_memory->read_byte(pc++, clk);\
    memptr += r16;\
    unsigned char byte = p_memory->read_byte(memptr, clk);\
    inc8(byte);\
    p_memory->write_byte(memptr, byte, clk);\
    time(19);\
}
#define DEC_XS(r16){\
    memptr = (signed char)p_memory->read_byte(pc++, clk);\
    memptr += r16;\
    unsigned char byte = p_memory->read_byte(memptr, clk);\
    dec8(byte);\
    p_memory->write_byte(memptr, byte, clk);\
    time(19);\
}

// DD/FD CB PREFIX.
// Instruction set clock time without DD/FD prefix time.
// RLC (IX + S), B
#define RLC_XS_R(r16, r8){\
        memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
        memptr += r16;\
        unsigned char byte = p_memory->read_byte(memptr, clk);\
        r8 = (byte << 1) | (byte >> 7);\
    }\
    f = flag_sz53p[r8] | (r8 & CF);\
    p_memory->write_byte(memptr, r8, clk);\
    time(19); // 23
#define RLC_XS(r16){\
        memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
        memptr += r16;\
        unsigned char byte = p_memory->read_byte(memptr, clk);\
        byte = (byte << 1) | (byte >> 7);\
        f = flag_sz53p[byte] | (byte & CF);\
        p_memory->write_byte(memptr, byte, clk);\
    }\
    time(19);
              
#define RRC_XS_R(r16, r8){\
        memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
        memptr += r16;\
        unsigned char byte = p_memory->read_byte(memptr, clk);\
        r8 = (byte >> 1) | (byte << 7);\
        f = flag_sz53p[r8] | (byte & CF);\
        p_memory->write_byte(memptr, r8, clk);\
    }\
    time(19);
#define RRC_XS(r16){\
        memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
        memptr += r16;\
        unsigned char byte = p_memory->read_byte(memptr, clk);\
        f = byte & CF;\
        byte = (byte >> 1) | (byte << 7);\
        f |= flag_sz53p[byte];\
        p_memory->write_byte(memptr, byte, clk);\
    }\
    time(19);

#define RL_XS_R(r16, r8){\
        memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
        memptr += r16;\
        unsigned char byte = p_memory->read_byte(memptr, clk);\
        r8 = (byte << 1) | (f & CF);\
        f = flag_sz53p[r8] | (byte >> 7);\
        p_memory->write_byte(memptr, r8, clk);\
    }\
    time(19);
#define RL_XS(r16){\
        memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
        memptr += r16;\
        unsigned char byte = p_memory->read_byte(memptr, clk);\
        unsigned char cf = byte >> 7;\
        byte = (byte << 1) | (f & CF);\
        f = flag_sz53p[byte] | cf;\
        p_memory->write_byte(memptr, byte, clk);\
    }\
    time(19);
 
#define RR_XS_R(r16, r8){\
        memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
        memptr += r16;\
        unsigned char byte = p_memory->read_byte(memptr, clk);\
        r8 = (byte >> 1) | (f << 7);\
        f = flag_sz53p[r8] | (byte & CF);\
        p_memory->write_byte(memptr, r8, clk);\
    }\
    time(19);
#define RR_XS(r16){\
        memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
        memptr += r16;\
        unsigned char byte = p_memory->read_byte(memptr, clk);\
        unsigned char cf = byte & CF;\
        byte = (byte >> 1) | (f << 7);\
        f = flag_sz53p[byte] | cf;\
        p_memory->write_byte(memptr, byte, clk);\
    }\
    time(19);


#define SLA_XS_R(r16, r8){\
        memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
        memptr += r16;\
        unsigned char byte = p_memory->read_byte(memptr, clk);\
        r8 = byte << 1;\
        f = flag_sz53p[r8] | (byte >> 7);\
        p_memory->write_byte(memptr, r8, clk);\
    }\
    time(19);
#define SLA_XS(r16){\
        memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
        memptr += r16;\
        unsigned char byte = p_memory->read_byte(memptr, clk);\
        f = byte >> 7;\
        byte <<= 1;\
        f |= flag_sz53p[byte];\
        p_memory->write_byte(memptr, byte, clk);\
    }\
    time(19);

#define SRA_XS_R(r16, r8){\
        memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
        memptr += r16;\
        unsigned char byte = p_memory->read_byte(memptr, clk);\
        r8 = (byte & 0x80) | (byte >> 1);\
        f = flag_sz53p[r8] | (byte & CF);\
        p_memory->write_byte(memptr, r8, clk);\
    }\
    time(19);


#define SRA_XS(r16){\
        memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
        memptr += r16;\
        unsigned char byte = p_memory->read_byte(memptr, clk);\
        f = byte & CF;\
        byte = (byte & 0x80) | (byte >> 1);\
        f |= flag_sz53p[byte];\
        p_memory->write_byte(memptr, byte, clk);\
    }\
    time(19);

#define SLL_XS_R(r16, r8){\
        memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
        memptr += r16;\
        unsigned char byte = p_memory->read_byte(memptr, clk);\
        r8 = (byte << 1) | 0x01;\
        f = (byte >> 7) | flag_sz53p[r8];\
        p_memory->write_byte(memptr, r8, clk);\
    }\
    time(19);
#define SLL_XS(r16){\
        memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
        memptr += r16;\
        unsigned char byte = p_memory->read_byte(memptr, clk);\
        f = byte >> 7;\
        byte = (byte << 1) | 0x01;\
        f |= flag_sz53p[byte];\
        p_memory->write_byte(memptr, byte, clk);\
    }\
    time(19);

#define SRL_XS_R(r16, r8){\
        memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
        memptr += r16;\
        unsigned char byte = p_memory->read_byte(memptr, clk);\
        r8 = byte >> 1;\
        f = flag_sz53p[r8] | (byte & CF);\
        p_memory->write_byte(memptr, r8, clk);\
    }\
    time(19);
#define SRL_XS(r16){\
        memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
        memptr += r16;\
        unsigned char byte = p_memory->read_byte(memptr, clk);\
        f = byte & CF;\
        byte >>= 1;\
        f |= flag_sz53p[byte];\
        p_memory->write_byte(memptr, byte, clk);\
    }\
    time(19);

#define BIT_XS(n, r16){\
        memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
        memptr += r16;\
        unsigned char byte = p_memory->read_byte(memptr, clk);\
        f = flag_sz53p[byte & (1 << n)] | HF | (f & CF);\
        f &= ~(F3 | F5);\
        f |= memptrh & (F3 | F5);\
    }\
    time(16);

#define RES_XS_R(n, r16, r8)\
    memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
    memptr += r16;\
    r8 = p_memory->read_byte(memptr, clk);\
    r8 &= ~(1 << n);\
    p_memory->write_byte(memptr, r8, clk);\
    time(19);

#define RES_XS(n, r16){\
        memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
        memptr += r16;\
        unsigned char byte = p_memory->read_byte(memptr, clk);\
        byte &= ~(1 << n);\
        p_memory->write_byte(memptr, byte, clk);\
    }\
    time(19);

#define SET_XS_R(n, r16, r8)\
    memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
    memptr += r16;\
    r8 = p_memory->read_byte(memptr, clk);\
    r8 |= (1 << n);\
    p_memory->write_byte(memptr, r8, clk);\
    time(19);

#define SET_XS(n, r16){\
        memptr = (signed char)p_memory->read_byte(pc - 2, clk);\
        memptr += r16;\
        unsigned char byte = p_memory->read_byte(memptr, clk);\
        byte |= (1 << n);\
        p_memory->write_byte(memptr, byte, clk);\
    }\
    time(19);

// ED PREFIX.
#define IN_R_RR(r8, r16)\
    r8 = p_io->read(r16, clk);\
    f = flag_sz53p[r8] | (f & CF);\
    memptr = r16 + 1;\
    time(8);

#define OUT_RR_R(r16, r8)\
    p_io->write(r16, r8, clk);\
    memptr = r16 + 1;\
    time(8);

#define SBC16(rr0, rr1){\
        unsigned short w = rr0 - rr1 - (f & CF);\
        f = ((w >> 8) & (SF | F3 | F5)) | NF | ((((rr0 & 0xFFF) - (rr1 & 0xFFF) - (f & CF)) >> 8) & HF);\
        if (w > rr0)\
            f |= CF;\
        if (((rr0 ^ rr1) & 0x8000) and ((rr0 ^ w) & 0x8000))\
            f |= PF;\
        if (!w)\
            f |= ZF;\
        memptr = rr0 + 1;\
        rr0 = w;\
    }\
    time(11);

#define ADC16(rr0, rr1){\
        unsigned short w = rr0 + rr1 + (f & CF);\
        f = ((w >> 8) & (SF | F3 | F5)) | ((((rr0 & 0xFFF) + (rr1 & 0xFFF) + (f & CF)) >> 8) & HF);\
        if (w < rr0)\
            f |= CF;\
        if (!((rr0 ^ rr1) & 0x8000) and ((rr0 ^ w) & 0x8000))\
            f |= PF;\
        if (!w)\
            f |= ZF;\
        memptr = rr0 + 1;\
        rr0 = w;\
    }\
    time(11);

#define NEG_A\
    f = flag_sbc[a];\
    a = -a;\
    time(4);

#define RRD {\
        unsigned char byte = p_memory->read_byte(hl, clk);\
        p_memory->write_byte(hl, (a << 4) | (byte >> 4), clk);\
        a = (a & 0xF0) | (byte & 0x0F);\
    }\
    f = flag_sz53p[a] | (f & CF);\
    memptr = hl + 1;\
    time(14);

#define RLD {\
        unsigned char byte = p_memory->read_byte(hl, clk);\
        p_memory->write_byte(hl, (a & 0x0F) | (byte << 4), clk);\
        a = (a & 0xF0) | (byte >> 4);\
    }\
    f = flag_sz53p[a] | (f & CF);\
    memptr = hl + 1;\
    time(14);

#define LDI {\
        unsigned char byte = p_memory->read_byte(hl++, clk);\
        p_memory->write_byte(de++, byte, clk);\
        f &= ~(NF | PF | F3 | HF | F5);\
        f |= ((byte + a) & F3) | (((byte + a) << 4) & F5);\
    }\
    if (--bc)\
        f |= PF;\
    time(12);

#define CPI\
    f = flag_cpb[a * 0x100 + p_memory->read_byte(hl++, clk)] | (f & CF);\
    if (--bc)\
        f |= PF;\
    memptr++;\
    time(12);

#define INI {\
        memptr = bc + 1;\
        unsigned char byte = p_io->read(bc, clk);\
        unsigned char tmp = byte + c + 1;\
        p_memory->write_byte(hl++, byte, clk);\
        f = flag_sz53p[--b] & ~PF;\
        f |= (byte >> 6) & NF;\
        f |= flag_parity[(tmp & 0x7) ^ b];\
        if (tmp < byte)\
            f |= HF | CF;\
    }\
    time(12);

#define OUTI {\
        unsigned char byte = p_memory->read_byte(hl++, clk);\
        unsigned char tmp = byte + l;\
        b--;\
        p_io->write(bc, byte, clk);\
        f = flag_sz53p[b] & ~PF;\
        f |= flag_parity[(tmp & 0x7) ^ b];\
        f |= (byte >> 6) & NF;\
        if (tmp < byte)\
            f |= HF | CF;\
        memptr = bc + 1;\
    }\
    time(12);

#define LDD {\
        unsigned char byte = p_memory->read_byte(hl--, clk);\
        p_memory->write_byte(de--, byte, clk);\
        f &= ~(NF | PF | F3 | HF | F5);\
        f |= ((byte + a) & F3) | (((byte + a) << 4) & F5);\
    }\
    if (--bc)\
        f |= PF;\
    time(12);


#define CPD \
    f = flag_cpb[a * 0x100 + p_memory->read_byte(hl++, clk)] | (f & CF);\
    if (--bc)\
        f |= PF;\
    memptr--;\
    time(12);

#define IND {\
        memptr = bc - 1;\
        unsigned char byte = p_io->read(bc, clk);\
        unsigned char tmp = byte + c - 1;\
        p_memory->write_byte(hl--, byte, clk);\
        f = flag_sz53p[--b] & ~PF;\
        f |= flag_parity[(tmp & 0x7) ^ b];\
        f |= (byte >> 6) & NF;\
        if (tmp < byte)\
            f |= HF | CF;\
    }\
    time(12);

#define OUTD \
    b--;\
    memptr = bc - 1;\
    {\
        unsigned char byte = p_memory->read_byte(hl--, clk);\
        unsigned char tmp = byte + l;\
        p_io->write(bc, byte, clk);\
        f = flag_sz53p[b] & ~PF;\
        f |= flag_parity[(tmp & 0x7) ^ b];\
        f |= (byte >> 6) & NF;\
        if (tmp < byte)\
            f |= HF | CF;\
    }\
    time(12);

#define LDIR {\
        unsigned char byte = p_memory->read_byte(hl++, clk);\
        p_memory->write_byte(de++, byte, clk);\
        f &= ~(NF | PF | F3 | HF | F5);\
        f |= ((byte + a) & F3) | (((byte + a) << 4) & F5);\
    }\
    if (--bc){\
        f |= PF;\
        pc -= 2;\
        memptr = pc + 1;\
        time(17);\
    }else{\
        time(12);\
    }

#define CPIR \
    f = flag_cpb[a * 0x100 + p_memory->read_byte(hl++, clk)] | (f & CF);\
    if (--bc){\
        f |= PF;\
        if (!(f & ZF)){\
            pc -= 2;\
            memptr = pc;\
            time(5);\
        }\
    }\
    memptr++;\
    time(12);

#define INIR {\
        memptr = bc + 1;\
        unsigned char byte = p_io->read(bc, clk);\
        unsigned char tmp = byte + c + 1;\
        p_memory->write_byte(hl++, byte, clk);\
        f = flag_sz53p[--b] & ~PF;\
        f |= flag_parity[(tmp & 0x7) ^ b];\
        f |= (byte >> 6) & NF;\
        if (tmp < byte)\
            f |= HF | CF;\
    }\
    if (b){\
        pc -= 2;\
        time(17);\
    }else{\
        time(12);\
    }

#define OUTIR {\
        b--;\
        memptr = bc + 1;\
        unsigned char byte = p_memory->read_byte(hl++, clk);\
        unsigned char tmp = byte + l;\
        p_io->write(bc, byte, clk);\
        f = flag_sz53p[b] & ~PF;\
        f |= flag_parity[(tmp & 0x7) ^ b];\
        f |= (byte >> 6) & NF;\
        if (tmp < byte)\
            f |= HF | CF;\
    }\
    if (b){\
        pc -= 2;\
        time(17);\
    }else{\
        time(12);\
    }

#define LDDR {\
        unsigned char byte = p_memory->read_byte(hl--, clk);\
        p_memory->write_byte(de--, byte, clk);\
        f &= ~(NF | PF | F3 | HF | F5);\
        f |= ((byte + a) & F3) | (((byte + a) << 4) & F5);\
    }\
    if (--bc){\
        f |= PF;\
        pc -= 2;\
        memptr = pc + 1;\
        time(17);\
    }else{\
        time(12);\
    }

#define CPDR \
    memptr--;\
    f = flag_cpb[a * 0x100 + p_memory->read_byte(hl++, clk)] | (f & CF);\
    if (--bc){\
        f |= PF;\
        if (!(f & ZF)){\
            pc -= 2;\
            memptr = pc + 1;\
            time(5);\
        }\
    }\
    time(12);

#define INDR {\
        memptr = bc - 1;\
        unsigned char byte = p_io->read(bc, clk);\
        unsigned char tmp = byte + c - 1;\
        p_memory->write_byte(hl--, byte, clk);\
        f = flag_sz53p[--b] & ~PF;\
        f |= flag_parity[(tmp & 0x7) ^ b];\
        f |= (byte >> 6) & NF;\
        if (tmp < byte)\
            f |= (HF | CF);\
    }\
    if (b){\
        pc -= 2;\
        time(17);\
    }else{\
        time(12);\
    }

#define OUTDR {\
        b--;\
        memptr = bc - 1;\
        unsigned char byte = p_memory->read_byte(hl--, clk);\
        unsigned char tmp = byte + l;\
        p_io->write(bc, byte, clk);\
        f = flag_sz53p[b] & ~PF;\
        f |= flag_parity[(tmp & 0x7) ^ b];\
        f |= (byte >> 6) & NF;\
        if (tmp < byte)\
            f |= HF | CF;\
    }\
    if (b){\
        pc -= 2;\
        time(17);\
    }else{\
        time(12);\
    }

class Z80 {
    public:
        Z80();
        void reset();
        void frame(ULA *p_memory, IO *p_io, int frame_clk);
        void interrupt(ULA *p_memory);

        void NMI();

        Reg88(b, c)
        Reg88(d, e)
        Reg88(h, l)
        Reg88(a, f)
        struct {
            Reg88(b, c)
            Reg88(d, e)
            Reg88(h, l)
            Reg88(a, f)
        } alt;
        Reg16(pc)
        Reg16(sp)
        Reg16(ir)
        Reg16(ix)
        Reg16(iy)
        Reg16(memptr)               // Undocumented.

        int clk;                    // Instructon clock state.
        unsigned char im;           // Interrupt mode.
        unsigned char iff1;         // Interrupts is enabled.
        unsigned char iff2;         // Save's iff1 when NMI active.
        unsigned char r8bit;        // Eight bit of the R register.
    protected:

        unsigned char flag_inc[0x100];
        unsigned char flag_dec[0x100];
        unsigned char flag_parity[0x100];
        unsigned char flag_adc[0x20000];
        unsigned char flag_sbc[0x20000];
        unsigned char flag_sz53p[0x100];
        unsigned char flag_cp[0x10000];
        unsigned char flag_cpb[0x10000];
#ifdef DEBUG
        bool trace = false;
#endif
};
