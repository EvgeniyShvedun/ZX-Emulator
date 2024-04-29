#define time(t)\
    clk += t

#define LD_R_R(r0, r1)\
    r0 = r1;\
    time(4);

#define LD_XR_R(r16, r8)\
    memory->write_byte(r16, r8, clk);\
    time(7);
// LD (BC/DE), A - uses memptr.
#define LD_XR_A(r16)\
    memptrl = r16##l + 1;\
    memptrh = a;\
    LD_XR_R(r16, a);
#define LD_XR_N(r16){\
    u8 byte = memory->read_byte(pc++);\
    memory->write_byte(r16, byte, clk);\
    time(10);\
}

#define LD_R_XR(r8, r16)\
    r8 = memory->read_byte(r16);\
    time(7);
// LD A, (BC/DE) - uses memptr.
#define LD_A_XR(r16)\
    memptr = r16 + 1;\
    LD_R_XR(a, r16);
#define LD_RR_RR(rr0, rr1)\
    rr0 = rr1;\
    time(6);
#define LD_RR_NN(r16)\
    r16##l = memory->read_byte(pc++);\
    r16##h = memory->read_byte(pc++);\
    time(10);
// increment memptr once.
#define LD_MM_RR(r16)\
    memptrl = memory->read_byte(pc++);\
    memptrh = memory->read_byte(pc++);\
    memory->write_byte(memptr++, r16##l, clk);\
    memory->write_byte(memptr, r16##h, clk);\
    time(16);
#define LD_RR_MM(r16)\
    memptrl = memory->read_byte(pc++);\
    memptrh = memory->read_byte(pc++);\
    r16##l = memory->read_byte(memptr++);\
    r16##h = memory->read_byte(memptr);\
    time(16);
#define LD_MM_A\
     memptrl = memory->read_byte(pc++);\
     memptrh = memory->read_byte(pc++);\
     memory->write_byte(memptr++, a, clk);\
     memptrh = a;\
     time(13);
#define LD_A_MM\
    memptrl = memory->read_byte(pc++);\
    memptrh = memory->read_byte(pc++);\
    a = memory->read_byte(memptr++);\
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
    u8 byte = memory->read_byte(r16);\
    inc8(byte);\
    memory->write_byte(r16, byte, clk);\
    time(11);\
}
#define DEC_XR(r16){\
    u8 byte = memory->read_byte(r16);\
    dec8(byte);\
    memory->write_byte(r16, byte, clk);\
    time(11);\
}

#define ADD16(rr0, rr1)\
    memptr = rr0 + 1;\
    f &= (SF | ZF | PF);\
    f |= ((((rr0 & 0xFFF) + (rr1 & 0xFFF)) >> 8) & HF);\
    f |= (((s32)rr0 + rr1) >> 16) & CF;\
    f |= ((rr0 + rr1) >> 8) & (F3 | F5);\
    rr0 += rr1;\
    time(11);


#define JR_N \
    pc += (s8)memory->read_byte(pc) + 1;\
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
    u8 byte = value;\
    f = flag_adc[a + byte * 0x100];\
    a += byte;\
}
#define adc8(value){\
    u8 byte = value;\
    u8 cf = f & CF;\
    f = flag_adc[a + byte * 0x100 + 0x10000 * cf];\
    a += byte + cf;\
}
#define sub8(value){\
    u8 byte = value;\
    f = flag_sbc[a * 0x100 + byte];\
    a -= byte;\
}
#define sbc8(value){\
    u8 byte = value;\
    u8 cf = f & CF;\
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
    add8(memory->read_byte(r16));\
    time(7);
#define ADD_XS(r16)\
    memptr = (s8)memory->read_byte(pc++);\
    memptr += r16;\
    add8(memory->read_byte(memptr));\
    time(15);

#define ADC(r8)\
    adc8(r8);\
    time(4);
#define ADC_XR(r16)\
    adc8(memory->read_byte(r16));\
    time(7);
#define ADC_XS(r16)\
    memptr = (s8)memory->read_byte(pc++);\
    memptr += r16;\
    adc8(memory->read_byte(memptr));\
    time(15);

#define SUB(r8)\
    sub8(r8);\
    time(4);
#define SUB_XR(r16)\
    sub8(memory->read_byte(r16));\
    time(7);
#define SUB_XS(r16)\
    memptr = (s8)memory->read_byte(pc++);\
    memptr += r16;\
    sub8(memory->read_byte(memptr));\
    time(15);

#define SBC(r8)\
    sbc8(r8);\
    time(4);
#define SBC_XR(r16)\
    sbc8(memory->read_byte(r16));\
    time(7);
#define SBC_XS(r16)\
    memptr = (s8)memory->read_byte(pc++);\
    memptr += r16;\
    sbc8(memory->read_byte(memptr));\
    time(15);

#define RLA {\
        u8 byte = a >> 7;\
        a = (a << 1) | (f & CF);\
        f = (f & (SF | ZF | PF)) | (a & (F3 | F5)) | byte;\
    }\
    time(4);

#define RRA {\
        u8 byte = a & CF;\
        a = (a >> 1) | (f << 7);\
        f = (f & (SF | ZF | PF)) | (a & (F3 | F5)) | byte;\
    }\
    time(4);

#define DAA {\
        u8 byte;\
        if (f & HF || (a & 0xF) > 9)\
            byte = 0x06;\
        else\
            byte = 0x00;\
        if (f & CF || (a > 0x99))\
            byte |= 0x60;\
        u8 tmp = a;\
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
    and8(memory->read_byte(r16));\
    time(7);
#define AND_XS(r16)\
    memptr = (s8)memory->read_byte(pc++);\
    memptr += r16;\
    and8(memory->read_byte(memptr));\
    time(15);

#define XOR(r8)\
    xor8(r8);\
    time(4);
#define XOR_XR(r16)\
    xor8(memory->read_byte(r16));\
    time(7);
#define XOR_XS(r16)\
    memptr = (s8)memory->read_byte(pc++);\
    memptr += r16;\
    xor8(memory->read_byte(memptr));\
    time(15);

#define OR(r8)\
    or8(r8);\
    time(4);
#define OR_XR(r16)\
    or8(memory->read_byte(r16));\
    time(7);
#define OR_XS(r16)\
    memptr = (s8)memory->read_byte(pc++);\
    memptr += r16;\
    or8(memory->read_byte(memptr));\
    time(15);

#define CP(r8)\
    cp8(r8);\
    time(4);
#define CP_XR(r16)\
    cp8(memory->read_byte(r16));\
    time(7);
#define CP_XS(r16)\
    memptr = (s8)memory->read_byte(pc++);\
    memptr += r16;\
    cp8(memory->read_byte(memptr));\
    time(15);

#define PUSH(r16)\
    memory->write_byte(--sp, r16##h, clk);\
    memory->write_byte(--sp, r16##l, clk);\
    time(11);

#define POP(r16)\
    r16##l = memory->read_byte(sp++);\
    r16##h = memory->read_byte(sp++);\
    time(10);

#define EX_RR_RR(rr0, rr1)\
    rr0 ^= rr1;\
    rr1 ^= rr0;\
    rr0 ^= rr1;\
    time(4);

// EX (SP), RR
#define EX_SP_RR(r16)\
    memptrl = memory->read_byte(sp);\
    memory->write_byte(sp, r16##l, clk);\
    r16##l = memptrl;\
    memptrh = memory->read_byte(sp + 1);\
    memory->write_byte(sp + 1, r16##h, clk);\
    r16##h = memptrh;\
    time(19);

#define JP_NN\
    memptrl = memory->read_byte(pc++);\
    memptrh = memory->read_byte(pc);\
    pc = memptr;\
    time(10);

#define JP_RR(r16)\
    pc = r16;\
    time(4);

#define JP_CND_NN(cnd)\
    memptrl = memory->read_byte(pc++);\
    memptrh = memory->read_byte(pc);\
    if (cnd)\
        pc = memptr;\
    else\
        pc++;\
    time(10);

#define CALL_NN\
    memptrl = memory->read_byte(pc++);\
    memptrh = memory->read_byte(pc++);\
    memory->write_byte(--sp, pch, clk);\
    memory->write_byte(--sp, pcl, clk);\
    pc = memptr;\
    time(17);
#define CALL_CND_NN(cnd)\
    memptrl = memory->read_byte(pc++);\
    memptrh = memory->read_byte(pc++);\
    if (cnd){\
        memory->write_byte(--sp, pch, clk);\
        memory->write_byte(--sp, pcl, clk);\
        pc = memptr;\
        time(17);\
    }else{\
        time(10);\
    }
#define RST(ptr)\
    memory->write_byte(--sp, pch, clk);\
    memory->write_byte(--sp, pcl, clk);\
    pc = memptr = ptr;\
    time(11);

#define RET\
    memptrl = memory->read_byte(sp++);\
    memptrh = memory->read_byte(sp++);\
    pc = memptr;\
    time(10);
#define RET_CND(cnd)\
    if (cnd){\
        memptrl = memory->read_byte(sp++);\
        memptrh = memory->read_byte(sp++);\
        pc = memptr;\
        time(11);\
    }else{\
        time(5);\
    }

#define OUT_N_A\
    memptrl = memory->read_byte(pc++);\
    memptrh = a;\
    io->write(memptr++, a, clk);\
    time(11);

#define IN_A_N\
    memptrh = a;\
    memptrl = memory->read_byte(pc++);\
    io->read(memptr++, &a, clk);\
    time(11);

// CB prefix. t-clk set with prefix time.
#define RLC(r8)\
    r8 = (r8 << 1) | (r8 >> 7);\
    f = flag_sz53p[r8] | (r8 & CF);\
    time(8);
#define RLC_XR(r16){\
        u8 byte = memory->read_byte(r16);\
        byte = (byte << 1) | (byte >> 7);\
        f = flag_sz53p[byte] | (byte & CF);\
        memory->write_byte(r16, byte, clk);\
    }\
    time(15);

#define RRC(r8)\
    f = r8 & CF;\
    r8 = (r8 >> 1) | (r8 << 7);\
    f |= flag_sz53p[r8];\
    time(8);
#define RRC_XR(r16){\
        u8 byte = memory->read_byte(r16);\
        f = byte & CF;\
        byte = (byte >> 1) | (byte << 7);\
        f |= flag_sz53p[byte];\
        memory->write_byte(r16, byte, clk);\
    }\
    time(15);

#define RL(r8){\
        u8 cf = r8 >> 7;\
        r8 = (r8 << 1) | (f & CF);\
        f = flag_sz53p[r8] | cf;\
    }\
    time(8);
#define RL_XR(r16){\
        u8 byte = memory->read_byte(r16);\
        u8 cf = byte >> 7;\
        byte = (byte << 1) | (f & CF);\
        f = flag_sz53p[byte] | cf;\
        memory->write_byte(r16, byte, clk);\
    }\
    time(15);

#define RR(r8){\
        u8 cf = r8 & CF;\
        r8 = (r8 >> 1) | (f << 7);\
        f = flag_sz53p[r8] | cf;\
    }\
    time(8);
#define RR_XR(r16){\
        u8 byte = memory->read_byte(r16);\
        u8 cf = byte & CF;\
        byte = (byte >> 1) | (f << 7);\
        f = flag_sz53p[byte] | cf;\
        memory->write_byte(r16, byte, clk);\
    }\
    time(15);

#define SLA(r8)\
    f = r8 >> 7;\
    r8 <<= 1;\
    f |= flag_sz53p[r8];\
    time(8);

#define SLA_XR(r16){\
        u8 byte = memory->read_byte(r16);\
        f = flag_sz53p[byte << 1] | (byte >> 7);\
        memory->write_byte(r16, byte << 1, clk);\
    }\
    time(15);

#define SRA(r8)\
    f = r8 & CF;\
    r8 = (r8 & 0x80) | (r8 >> 1);\
    f |= flag_sz53p[r8];\
    time(8);
#define SRA_XR(r16){\
        u8 byte = memory->read_byte(r16);\
        f = byte & CF;\
        byte = (byte & 0x80) | (byte >> 1);\
        f |= flag_sz53p[byte];\
        memory->write_byte(r16, byte, clk);\
    }\
    time(15);

#define SLL(r8)\
    f = r8 >> 7;\
    r8 = (r8 << 1) | 0x01;\
    f |= flag_sz53p[r8];\
    time(8);
#define SLL_XR(r16){\
        u8 byte = memory->read_byte(r16);\
        f = byte >> 7;\
        byte = (byte << 1) | 0x01;\
        f |= flag_sz53p[byte];\
        memory->write_byte(r16, byte, clk);\
    }\
    time(15);

#define SRL(r8)\
    f = r8 & CF;\
    r8 >>= 1;\
    f |= flag_sz53p[r8];\
    time(8);
#define SRL_XR(r16){\
        u8 byte = memory->read_byte(r16);\
        f = (byte & CF) | flag_sz53p[byte >> 1];\
        memory->write_byte(r16, byte >> 1, clk);\
    }\
    time(15);

#define BIT(n, r8)\
    f = flag_sz53p[r8 & (1 << n)] | HF | (r8 & (F3 | F5)) | (f & CF);\
    time(8);
#define BIT_XR(n, r16){\
        u8 byte = memory->read_byte(r16);\
        f = flag_sz53p[byte & (1 << n)] | HF | (f & CF);\
        f &= ~(F3 | F5);\
        f |= memptrh & (F3 | F5);\
    }\
    time(12);

#define RES(n, r8)\
    r8 &= ~(1 << n);\
    time(8);
#define RES_XR(n, r16){\
        u8 byte = memory->read_byte(r16);\
        memory->write_byte(r16, byte & ~(1 << n), clk);\
    }\
    time(15);

#define SET(n, r8)\
    r8 |= (1 << n);\
    time(8);
#define SET_XR(n, r16){\
        u8 byte = memory->read_byte(r16);\
        memory->write_byte(r16, byte | (1 << n), clk);\
    }\
    time(15);

// -------- DD/FD prefix --------
// T-clk set without prefix time.

// LD (IX+S), N
#define LD_XS_N(r16)\
    memptr = (s8)memory->read_byte(pc++);\
    memptr += r16;\
    memory->write_byte(memptr, memory->read_byte(pc++), clk);\
    time(15);
// LD B, (IX+S)
#define LD_R_XS(r8, r16)\
    memptr = (s8)memory->read_byte(pc++);\
    memptr += r16;\
    r8 = memory->read_byte(memptr);\
    time(15);
// LD (IX+S), B
#define LD_XS_R(r16, r8)\
    memptr = (s8)memory->read_byte(pc++);\
    memptr += r16;\
    memory->write_byte(memptr, r8, clk);\
    time(15);

// INC (IX+S)
#define INC_XS(r16){\
    memptr = (s8)memory->read_byte(pc++);\
    memptr += r16;\
    u8 byte = memory->read_byte(memptr);\
    inc8(byte);\
    memory->write_byte(memptr, byte, clk);\
    time(19);\
}
#define DEC_XS(r16){\
    memptr = (s8)memory->read_byte(pc++);\
    memptr += r16;\
    u8 byte = memory->read_byte(memptr);\
    dec8(byte);\
    memory->write_byte(memptr, byte, clk);\
    time(19);\
}

// ------- DD/FD CB prefix -------
// T-clk set without prefix time.

// RLC (IX + S), B
#define RLC_XS_R(r16, r8){\
        memptr = (s8)memory->read_byte(pc - 2);\
        memptr += r16;\
        u8 byte = memory->read_byte(memptr);\
        r8 = (byte << 1) | (byte >> 7);\
    }\
    f = flag_sz53p[r8] | (r8 & CF);\
    memory->write_byte(memptr, r8, clk);\
    time(19); // 23
#define RLC_XS(r16){\
        memptr = (s8)memory->read_byte(pc - 2);\
        memptr += r16;\
        u8 byte = memory->read_byte(memptr);\
        byte = (byte << 1) | (byte >> 7);\
        f = flag_sz53p[byte] | (byte & CF);\
        memory->write_byte(memptr, byte, clk);\
    }\
    time(19);
              
#define RRC_XS_R(r16, r8){\
        memptr = (s8)memory->read_byte(pc - 2);\
        memptr += r16;\
        u8 byte = memory->read_byte(memptr);\
        r8 = (byte >> 1) | (byte << 7);\
        f = flag_sz53p[r8] | (byte & CF);\
        memory->write_byte(memptr, r8, clk);\
    }\
    time(19);
#define RRC_XS(r16){\
        memptr = (s8)memory->read_byte(pc - 2);\
        memptr += r16;\
        u8 byte = memory->read_byte(memptr);\
        f = byte & CF;\
        byte = (byte >> 1) | (byte << 7);\
        f |= flag_sz53p[byte];\
        memory->write_byte(memptr, byte, clk);\
    }\
    time(19);

#define RL_XS_R(r16, r8){\
        memptr = (s8)memory->read_byte(pc - 2);\
        memptr += r16;\
        u8 byte = memory->read_byte(memptr);\
        r8 = (byte << 1) | (f & CF);\
        f = flag_sz53p[r8] | (byte >> 7);\
        memory->write_byte(memptr, r8, clk);\
    }\
    time(19);
#define RL_XS(r16){\
        memptr = (s8)memory->read_byte(pc - 2);\
        memptr += r16;\
        u8 byte = memory->read_byte(memptr);\
        u8 cf = byte >> 7;\
        byte = (byte << 1) | (f & CF);\
        f = flag_sz53p[byte] | cf;\
        memory->write_byte(memptr, byte, clk);\
    }\
    time(19);
 
#define RR_XS_R(r16, r8){\
        memptr = (s8)memory->read_byte(pc - 2);\
        memptr += r16;\
        u8 byte = memory->read_byte(memptr);\
        r8 = (byte >> 1) | (f << 7);\
        f = flag_sz53p[r8] | (byte & CF);\
        memory->write_byte(memptr, r8, clk);\
    }\
    time(19);
#define RR_XS(r16){\
        memptr = (s8)memory->read_byte(pc - 2);\
        memptr += r16;\
        u8 byte = memory->read_byte(memptr);\
        u8 cf = byte & CF;\
        byte = (byte >> 1) | (f << 7);\
        f = flag_sz53p[byte] | cf;\
        memory->write_byte(memptr, byte, clk);\
    }\
    time(19);


#define SLA_XS_R(r16, r8){\
        memptr = (s8)memory->read_byte(pc - 2);\
        memptr += r16;\
        u8 byte = memory->read_byte(memptr);\
        r8 = byte << 1;\
        f = flag_sz53p[r8] | (byte >> 7);\
        memory->write_byte(memptr, r8, clk);\
    }\
    time(19);
#define SLA_XS(r16){\
        memptr = (s8)memory->read_byte(pc - 2);\
        memptr += r16;\
        u8 byte = memory->read_byte(memptr);\
        f = byte >> 7;\
        byte <<= 1;\
        f |= flag_sz53p[byte];\
        memory->write_byte(memptr, byte, clk);\
    }\
    time(19);

#define SRA_XS_R(r16, r8){\
        memptr = (s8)memory->read_byte(pc - 2);\
        memptr += r16;\
        u8 byte = memory->read_byte(memptr);\
        r8 = (byte & 0x80) | (byte >> 1);\
        f = flag_sz53p[r8] | (byte & CF);\
        memory->write_byte(memptr, r8, clk);\
    }\
    time(19);


#define SRA_XS(r16){\
        memptr = (s8)memory->read_byte(pc - 2);\
        memptr += r16;\
        u8 byte = memory->read_byte(memptr);\
        f = byte & CF;\
        byte = (byte & 0x80) | (byte >> 1);\
        f |= flag_sz53p[byte];\
        memory->write_byte(memptr, byte, clk);\
    }\
    time(19);

#define SLL_XS_R(r16, r8){\
        memptr = (s8)memory->read_byte(pc - 2);\
        memptr += r16;\
        u8 byte = memory->read_byte(memptr);\
        r8 = (byte << 1) | 0x01;\
        f = (byte >> 7) | flag_sz53p[r8];\
        memory->write_byte(memptr, r8, clk);\
    }\
    time(19);
#define SLL_XS(r16){\
        memptr = (s8)memory->read_byte(pc - 2);\
        memptr += r16;\
        u8 byte = memory->read_byte(memptr);\
        f = byte >> 7;\
        byte = (byte << 1) | 0x01;\
        f |= flag_sz53p[byte];\
        memory->write_byte(memptr, byte, clk);\
    }\
    time(19);

#define SRL_XS_R(r16, r8){\
        memptr = (s8)memory->read_byte(pc - 2);\
        memptr += r16;\
        u8 byte = memory->read_byte(memptr);\
        r8 = byte >> 1;\
        f = flag_sz53p[r8] | (byte & CF);\
        memory->write_byte(memptr, r8, clk);\
    }\
    time(19);
#define SRL_XS(r16){\
        memptr = (s8)memory->read_byte(pc - 2);\
        memptr += r16;\
        u8 byte = memory->read_byte(memptr);\
        f = byte & CF;\
        byte >>= 1;\
        f |= flag_sz53p[byte];\
        memory->write_byte(memptr, byte, clk);\
    }\
    time(19);

#define BIT_XS(n, r16){\
        memptr = (s8)memory->read_byte(pc - 2);\
        memptr += r16;\
        u8 byte = memory->read_byte(memptr);\
        f = flag_sz53p[byte & (1 << n)] | HF | (f & CF);\
        f &= ~(F3 | F5);\
        f |= memptrh & (F3 | F5);\
    }\
    time(16);

#define RES_XS_R(n, r16, r8)\
    memptr = (s8)memory->read_byte(pc - 2);\
    memptr += r16;\
    r8 = memory->read_byte(memptr);\
    r8 &= ~(1 << n);\
    memory->write_byte(memptr, r8, clk);\
    time(19);

#define RES_XS(n, r16){\
        memptr = (s8)memory->read_byte(pc - 2);\
        memptr += r16;\
        u8 byte = memory->read_byte(memptr);\
        byte &= ~(1 << n);\
        memory->write_byte(memptr, byte, clk);\
    }\
    time(19);

#define SET_XS_R(n, r16, r8)\
    memptr = (s8)memory->read_byte(pc - 2);\
    memptr += r16;\
    r8 = memory->read_byte(memptr);\
    r8 |= (1 << n);\
    memory->write_byte(memptr, r8, clk);\
    time(19);

#define SET_XS(n, r16){\
        memptr = (s8)memory->read_byte(pc - 2);\
        memptr += r16;\
        u8 byte = memory->read_byte(memptr);\
        byte |= (1 << n);\
        memory->write_byte(memptr, byte, clk);\
    }\
    time(19);

// -------- ED prefix --------
// T-clk set with prefix time.
#define IN_R_RR(r8, r16)\
    io->read(r16, &r8, clk);\
    f = flag_sz53p[r8] | (f & CF);\
    memptr = r16 + 1;\
    time(8);

#define OUT_RR_R(r16, r8)\
    io->write(r16, r8, clk);\
    memptr = r16 + 1;\
    time(8);

#define SBC16(rr0, rr1){\
        u16 w = rr0 - rr1 - (f & CF);\
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
        u16 w = rr0 + rr1 + (f & CF);\
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
        u8 byte = memory->read_byte(hl);\
        memory->write_byte(hl, (a << 4) | (byte >> 4), clk);\
        a = (a & 0xF0) | (byte & 0x0F);\
    }\
    f = flag_sz53p[a] | (f & CF);\
    memptr = hl + 1;\
    time(14);

#define RLD {\
        u8 byte = memory->read_byte(hl);\
        memory->write_byte(hl, (a & 0x0F) | (byte << 4), clk);\
        a = (a & 0xF0) | (byte >> 4);\
    }\
    f = flag_sz53p[a] | (f & CF);\
    memptr = hl + 1;\
    time(14);

#define LDI {\
        u8 byte = memory->read_byte(hl++);\
        memory->write_byte(de++, byte, clk);\
        f &= ~(NF | PF | F3 | HF | F5);\
        f |= ((byte + a) & F3) | (((byte + a) << 4) & F5);\
    }\
    if (--bc)\
        f |= PF;\
    time(12);

#define CPI\
    f = flag_cpb[a * 0x100 + memory->read_byte(hl++)] | (f & CF);\
    if (--bc)\
        f |= PF;\
    memptr++;\
    time(12);

#define INI {\
        u8 byte;\
        memptr = bc + 1;\
        io->read(bc, &byte, clk);\
        u8 tmp = byte + c + 1;\
        memory->write_byte(hl++, byte, clk);\
        f = flag_sz53p[--b] & ~PF;\
        f |= (byte >> 6) & NF;\
        f |= flag_parity[(tmp & 0x7) ^ b];\
        if (tmp < byte)\
            f |= HF | CF;\
    }\
    time(12);

#define OUTI {\
        u8 byte = memory->read_byte(hl++);\
        u8 tmp = byte + l;\
        b--;\
        io->write(bc, byte, clk);\
        f = flag_sz53p[b] & ~PF;\
        f |= flag_parity[(tmp & 0x7) ^ b];\
        f |= (byte >> 6) & NF;\
        if (tmp < byte)\
            f |= HF | CF;\
        memptr = bc + 1;\
    }\
    time(12);

#define LDD {\
        u8 byte = memory->read_byte(hl--);\
        memory->write_byte(de--, byte, clk);\
        f &= ~(NF | PF | F3 | HF | F5);\
        f |= ((byte + a) & F3) | (((byte + a) << 4) & F5);\
    }\
    if (--bc)\
        f |= PF;\
    time(12);


#define CPD \
    f = flag_cpb[a * 0x100 + memory->read_byte(hl++)] | (f & CF);\
    if (--bc)\
        f |= PF;\
    memptr--;\
    time(12);

#define IND {\
        u8 byte, tmp;\
        memptr = bc - 1;\
        io->read(bc, &byte, clk);\
        tmp = byte + c - 1;\
        memory->write_byte(hl--, byte, clk);\
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
        u8 byte = memory->read_byte(hl--);\
        u8 tmp = byte + l;\
        io->write(bc, byte, clk);\
        f = flag_sz53p[b] & ~PF;\
        f |= flag_parity[(tmp & 0x7) ^ b];\
        f |= (byte >> 6) & NF;\
        if (tmp < byte)\
            f |= HF | CF;\
    }\
    time(12);

#define LDIR {\
        u8 byte = memory->read_byte(hl++);\
        memory->write_byte(de++, byte, clk);\
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
    f = flag_cpb[a * 0x100 + memory->read_byte(hl++)] | (f & CF);\
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
        u8 byte, tmp;\
        memptr = bc + 1;\
        io->read(bc, &byte, clk);\
        tmp = byte + c + 1;\
        memory->write_byte(hl++, byte, clk);\
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
        u8 byte = memory->read_byte(hl++);\
        u8 tmp = byte + l;\
        io->write(bc, byte, clk);\
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
        u8 byte = memory->read_byte(hl--);\
        memory->write_byte(de--, byte, clk);\
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
    f = flag_cpb[a * 0x100 + memory->read_byte(hl++)] | (f & CF);\
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
        u8 byte, tmp;\
        memptr = bc - 1;\
        io->read(bc, &byte, clk);\
        tmp = byte + c - 1;\
        memory->write_byte(hl--, byte, clk);\
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
        u8 byte = memory->read_byte(hl--);\
        u8 tmp = byte + l;\
        io->write(bc, byte, clk);\
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
