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
#include "z80_macros.h"
#include "z80.h"

Z80::Z80() {
    for (s32 i = 0; i < 0x100; i++){
        // INC
        flag_inc[i] = (i + 1) & (F3 | F5 | SF);
        if (((i + 1) & 0x0F) == 0x00)
            flag_inc[i] |= HF;
        if ((i + 1) == 0x80)
            flag_inc[i] |= PF;
        if (!((i + 1) & 0xFF))
            flag_inc[i] |= ZF;
        // DEC
        flag_dec[i] = ((i - 1) & (F3 | F5 | SF)) | NF;
        if (((i - 1) & 0x0F) == 0x0F)
            flag_dec[i] |= HF;
        if ((i - 1) == 0x7F)
            flag_dec[i] |= PF;
        if (!(i - 1))
            flag_dec[i] |= ZF;
        // Parity
        s32 j = i, p = 0x00;
        for (s32 k = 0; k < 8; k++){
            p += j & 0x01;
            j >>= 1;
        }
        flag_parity[i] = !(p & 0x01) ? PF : 0x00;
        // Check and set\reset: SF ZF F5 F3 PF
        // Clear: NF HF CF
        flag_sz53p[i] = i & (SF | F3 | F5);// | CF);
        flag_sz53p[i] |= flag_parity[i];
        if (!i)
            flag_sz53p[i] |= ZF;
    }
    for (s32 c = 0; c < 2; c++){
        for (s32 i = 0; i < 0x100; i++){
            for (s32 j = 0; j < 0x100; j++){
                // ADD, ADC
                s32 r = i + j + c;
                u8 f = r & (F3 | F5 | SF);
                if (r & 0x100)
                    f |= CF;
                if (!(r & 0xFF))
                    f |= ZF;
                if ((((i ^ j) & 0x80) == 0x00) && ((r ^ i) & 0x80))
                    f |= PF;
                if (((i & 0xF) + (j & 0xF) + c) & 0x10)
                    f |= HF;
                flag_adc[i * 0x100 + j + c * 0x10000] = f;
                // SUB, SBC
                r = i - j - c;
                f = (r & (F3 | F5 | SF)) | NF;
                if (r & 0x100)
                    f |= CF;
                if (!(r & 0xFF))
                    f |= ZF;
                if (((i ^ j) & 0x80) && ((r ^ i) & 0x80))
                    f |= PF;
                if (((i & 0xF) - (j & 0xF) - c) & 0x10)
                    f |= HF;
                flag_sbc[i * 0x100 + j + c * 0x10000] = f;
            }
        }
    }
    for (s32 i = 0; i < 0x10000; i++){ // For CP instruction F3 and F5 taked from argument, not from register A. (check)
        flag_cp[i] = (flag_sbc[i] & ~(F3 | F5)) | (i & (F3 | F5));
        u8 byte = (i >> 8) - (i & 0xFF) - ((flag_sbc[i] & HF) >> 4);
        flag_cpb[i] = (flag_sbc[i] & ~(F3 | F5 | PF | CF)) + (byte & F3) + ((byte << 4) & F5);
    }
    reset();
}

void Z80::reset(){
    pc = 0x0000;
    sp = 0x0000;
    bc = 0x0000;
    de = 0x0000;
    hl = 0x0000;
    af = 0x0000;
    alt.bc = 0x0000;
    alt.de = 0x0000;
    alt.hl = 0x0000;
    alt.af = 0x0000;
    ir = 0x0000;
    ix = 0x0000;
    iy = 0x0000;
    memptr = 0x0000;
    im = 2;
    iff1 = 1;
    iff2 = 0;
    r8bit = 0;
    clk = 0;
}

void Z80::interrupt(ULA *memory){
    if (!iff1)
        return;
    iff1 = iff2 = 0x00;
    irl++;
    if (memory->read_byte(pc) == 0x76) // HALT
        pc++;
    if (im < 2){
        memptr = 0x0038;
        time(13);
    }else{
        u16 vector = ir | 0xFF;
        memptrl = memory->read_byte(vector++);
        memptrh = memory->read_byte(vector);
        time(19);
    }
    memory->write_byte(--sp, pch, clk);
    memory->write_byte(--sp, pcl, clk);
    pc = memptr;
}

void Z80::NMI(){
}

void Z80::step_over(ULA *memory, IO *io, s32 frame_clk){
}

void Z80::step_into(ULA *memory, IO *io, s32 frame_clk){
    frame(memory, io, clk);
    if (clk > frame_clk){
        interrupt(memory);
        clk -= frame_clk;
    }
}

void Z80::frame(ULA *memory, IO *io, s32 frame_clk){
    while (clk < frame_clk){
        irl++;
        switch (memory->read_byte_ex(pc++)){
            case 0x00: // NOP
                time(4);
                break;
            case 0x01: // LD BC, NN
                LD_RR_NN(bc);
                break;
            case 0x02: // LD (BC), A
                LD_XR_A(bc);
                break;
            case 0x03: // INC BC
                INC_RR(bc);
                break;
            case 0x04: // INC B
                INC(b);
                break;
            case 0x05: // DEC B
                DEC(b);
                break;
            case 0x06: // LD B, N
                LD_R_XR(b, pc++);
                break;
            case 0x07: // RLCA
                a = (a << 1) | a >> 7;
                f = (f & (SF | ZF | PF)) | (a & (F5 | F3 | CF));
                time(4);
                break;
            case 0x08: // EX AF, AF'
                af ^= alt.af;
                alt.af ^= af;
                af ^= alt.af;
                time(4);
                break;
            case 0x09: // ADD HL, BC
                ADD16(hl, bc);
                break;
            case 0x0A: // LD A, (BC)
                LD_A_XR(bc);
                break;
            case 0x0B: // DEC BC
                DEC_RR(bc);
                break;
            case 0x0C: // INC C
                INC(c);
                break;
            case 0x0D: // DEC C
                DEC(c);
                break;
            case 0x0E: // LD C, N
                LD_R_XR(c, pc++);
                break;
            case 0x0F: // RRCA
                f = (f & (SF | ZF | PF)) | (a & CF);
                a = (a >> 1) | (a << 7);
                f |= (a & (F5 | F3));
                time(4);
                break;
            case 0x10: // DJNZ N
                if (--b){
                    pc += (s8)memory->read_byte(pc) + 1;
                    memptr = pc;
                    time(13);
                }else{
                    pc++;
                    time(8);
                }
                break;
            case 0x11: // LD DE, NN
                LD_RR_NN(de);
                break;
            case 0x12: // LD (DE), A
                LD_XR_A(de);
                break;
            case 0x13: // INC DE
                INC_RR(de);
                break;
            case 0x14: // INC D
                INC(d);
                break;
            case 0x15: // DEC D
                DEC(d);
                break;
            case 0x16: // LD D, N
                LD_R_XR(d, pc++);
                break;
            case 0x17: // RLA
                RLA;
                break;
            case 0x18: // JR N
                JR_N;
                break;
            case 0x19: // ADD HL, DE
                ADD16(hl, de);
                break;
            case 0x1A: // LD A, (DE)
                LD_A_XR(de);
                break;
            case 0x1B: // DEC DE
                DEC_RR(de);
                break;
            case 0x1C: // INC E
                INC(e);
                break;
            case 0x1D: // DEC E
                DEC(e);
                break;
            case 0x1E: // LD E, N
                LD_R_XR(e, pc++);
                break;
            case 0x1F: // RRA
                RRA;
                break;
            case 0x20: // JR NZ, N
                JR_CND_N(!(f & ZF));
                break;
            case 0x21: // LD HL, NN
                LD_RR_NN(hl);
                break;
            case 0x22: // LD (NN), HL
                LD_MM_RR(hl);
                break;
            case 0x23: // INC HL
                INC_RR(hl);
                break;
            case 0x24: // INC H
                INC(h);
                break;
            case 0x25: // DEC H
                DEC(h);
                break;
            case 0x26: // LD H, N
                LD_R_XR(h, pc++);
                break;
            case 0x27: // DAA
                DAA;
                break;
            case 0x28: // JR Z, N
                JR_CND_N(f & ZF);
                break;
            case 0x29: // ADD HL, HL
                ADD16(hl, hl);
                break;
            case 0x2A: // LD HL, (NN)
                LD_RR_MM(hl);
                break;
            case 0x2B: // DEC HL
                DEC_RR(hl);
                break;
            case 0x2C: // INC L
                INC(l);
                break;
            case 0x2D: // DEC L
                DEC(l);
                break;
            case 0x2E: // LD L, N
                LD_R_XR(l, pc++);
                break;
            case 0x2F: // CPL
                a ^= 0xFF;
                f = (f & (SF | ZF | PF | CF)) | HF | NF | (a & (F3 | F5));
                time(4);
                break;
            case 0x30: // JR NC, N
                JR_CND_N(!(f & CF))
                break;
            case 0x31: // LD SP, NN
                LD_RR_NN(sp);
                break;
            case 0x32: // LD (NN), A
                LD_MM_A;
                break;
            case 0x33: // INC SP
                INC_RR(sp);
                break;
            case 0x34: // INC (HL)
                INC_XR(hl);
                break;
            case 0x35: // DEC (HL)
                DEC_XR(hl);
                break;
            case 0x36: // LD (HL), N
                LD_XR_N(hl);
                break;
            case 0x37: // SCF
                f = (f & (SF | ZF | PF)) | CF | (a & (F3 | F5));
                time(4);
                break;
            case 0x38: // JR C, N
                JR_CND_N(f & CF);
                break;
            case 0x39: // ADD HL, SP
                ADD16(hl, sp);
                break;
            case 0x3A: // LD A, (NN)
                LD_A_MM;
                break;
            case 0x3B: // DEC SP
                DEC_RR(sp);
                break;
            case 0x3C: // INC A
                INC(a);
                break;
            case 0x3D: // DEC A
                DEC(a);
                break;
            case 0x3E: // LD A, N
                LD_R_XR(a, pc++);
                break;
            case 0x3F: // CCF
                f = ((f & ~(NF | HF)) | ((f << 4) & HF) | (a & (F3 | F5))) ^ CF;
                time(4);
                break;
            case 0x40: // LD B, B
                time(4);
                break;
            case 0x41: // LD B, C
                LD_R_R(b, c);
                break;
            case 0x42: // LD B, D
                LD_R_R(b, d);
                break;
            case 0x43: // LD B, E
                LD_R_R(b, e);
                break;
            case 0x44: // LD B, H
                LD_R_R(b, h);
                break;
            case 0x45: // LD B, L
                LD_R_R(b, l);
                break;
            case 0x46: // LD B, (HL)
                LD_R_XR(b, hl);
                break;
            case 0x47: // LD B, A
                LD_R_R(b, a);
                break;
            case 0x48: // LD C, B
                LD_R_R(c, b);
                break;
            case 0x49: // LD C, C
                time(4);
                break;
            case 0x4A: // LD C, D
                LD_R_R(c, d);
                break;
            case 0x4B: // LD C, E
                LD_R_R(c, e);
                break;
            case 0x4C: // LD C, H
                LD_R_R(c, h);
                break;
            case 0x4D: // LD C, L
                LD_R_R(c, l);
                break;
            case 0x4E: // LD C, (HL)
                LD_R_XR(c, hl);
                break;
            case 0x4F: // LD C, A
                LD_R_R(c, a);
                break;
            case 0x50: // LD D, B
                LD_R_R(d, b);
                break;
            case 0x51: // LD D, C
                LD_R_R(d, c);
                break;
            case 0x52: // LD D, D
                time(4);
                break;
            case 0x53: // LD D, E
                LD_R_R(d, e);
                break;
            case 0x54: // LD D, H
                LD_R_R(d, h);
                break;
            case 0x55: // LD D, L
                LD_R_R(d, l);
                break;
            case 0x56: // LD D, (HL)
                LD_R_XR(d, hl);
                break;
            case 0x57: // LD D, A
                LD_R_R(d, a);
                break;
            case 0x58: // LD E, B
                LD_R_R(e, b);
                break;
            case 0x59: // LD E, C
                LD_R_R(e, c);
                break;
            case 0x5A: // LD E, D
                LD_R_R(e, d);
                break;
            case 0x5B: // LD E, E
                if (memory->trap_trdos(pc - 1)){
                    pc--;
                    irl--;
                }else
                    time(4);
                break;
            case 0x5C: // LD E, H
                LD_R_R(e, h);
                break;
            case 0x5D: // LD E, L
                LD_R_R(e, l);
                break;
            case 0x5E: // LD E, (HL)
                LD_R_XR(e, hl);
                break;
            case 0x5F: // LD E, A
                LD_R_R(e, a);
                break;
            case 0x60: // LD H, B
                LD_R_R(h, b);
                break;
            case 0x61: // LD H, C
                LD_R_R(h, c);
                break;
            case 0x62: // LD H, D
                LD_R_R(h, d);
                break;
            case 0x63: // LD H, E
                LD_R_R(h, e);
                break;
            case 0x64: // LD H, H
                time(4);
                break;
            case 0x65: // LD H, L
                LD_R_R(h, l);
                break;
            case 0x66: // LD H, (HL)
                LD_R_XR(h, hl);
                break;
            case 0x67: // LD H, A
                LD_R_R(h, a);
                break;
            case 0x68: // LD L, B
                LD_R_R(l, b);
                break;
            case 0x69: // LD L, C
                LD_R_R(l, c);
                break;
            case 0x6A: // LD L, D
                LD_R_R(l, d);
                break;
            case 0x6B: // LD L, E
                LD_R_R(l, e);
                break;
            case 0x6C: // LD L, H
                LD_R_R(l, h);
                break;
            case 0x6D: // LD L, L
                time(4);
                break;
            case 0x6E: // LD L, (HL);
                LD_R_XR(l, hl);
                break;
            case 0x6F: // LD L, A
                LD_R_R(l, a);
                break;
            case 0x70: // LD (HL), B
                LD_XR_R(hl, b);
                break;
            case 0x71: // LD (HL), C
                LD_XR_R(hl, c);
                break;
            case 0x72: // LD (HL), D
                LD_XR_R(hl, d);
                break;
            case 0x73: // LD (HL), E
                LD_XR_R(hl, e);
                break;
            case 0x74: // LD (HL), H
                LD_XR_R(hl, h);
                break;
            case 0x75: // LD (HL), L
                LD_XR_R(hl, l);
                break;
            case 0x76: // HALT
                pc--;
                time(4);
                break;
            case 0x77: // LD (HL), A
                LD_XR_R(hl, a);
                break;
            case 0x78: // LD A, B
                LD_R_R(a, b);
                break;
            case 0x79: // LD A, C
                LD_R_R(a, c);
                break;
            case 0x7A: // LD A, D
                LD_R_R(a, d);
                break;
            case 0x7B: // LD A, E
                LD_R_R(a, e);
                break;
            case 0x7C: // LD A, H
                LD_R_R(a, h);
                break;
            case 0x7D: // LD A, L
                LD_R_R(a, l);
                break;
            case 0x7E: // LD A, (HL)
                LD_R_XR(a, hl);
                break;
            case 0x7F: // LD A, A
                time(4);
                break;
            case 0x80: // ADD A, B
                ADD(b);
                break;
            case 0x81: // ADD A, C
                ADD(c);
                break;
            case 0x82: // ADD A, D
                ADD(d);
                break;
            case 0x83: // ADD A, E
                ADD(e);
                break;
            case 0x84: // ADD A, H
                ADD(h);
                break;
            case 0x85: // ADD A, L
                ADD(l);
                break;
            case 0x86: // ADD A, (HL)
                ADD_XR(hl);
                break;
            case 0x87: // ADD A, A
                ADD(a);
                break;
            case 0x88: // ADC A, B
                ADC(b);
                break;
            case 0x89: // ADC A, C
                ADC(c);
                break;
            case 0x8A: // ADC A, D
                ADC(d);
                break;
            case 0x8B: // ADC A, E
                ADC(e);
                break;
            case 0x8C: // ADC A, H
                ADC(h);
                break;
            case 0x8D: // ADC A, L
                ADC(l);
                break;
            case 0x8E: // ADC A, (HL)
                ADC_XR(hl);
                break;
            case 0x8F: // ADC A, A
                ADC(a);
                break;
            case 0x90: // SUB A, B
                SUB(b);
                break;
            case 0x91: // SUB A, C
                SUB(c);
                break;
            case 0x92: // SUB A, D
                SUB(d);
                break;
            case 0x93: // SUB A, E
                SUB(e);
                break;
            case 0x94: // SUB A, H
                SUB(h);
                break;
            case 0x95: // SUB A, L
                SUB(l);
                break;
            case 0x96: // SUB A, (HL)
                SUB_XR(hl);
                break;
            case 0x97: // SUB A, A
                SUB(a);
                break;
            case 0x98: // SBC A, B
                SBC(b);
                break;
            case 0x99: // SBC A, C
                SBC(c);
                break;
            case 0x9A: // SBC A, D
                SBC(d);
                break;
            case 0x9B: // SBC A, E
                SBC(e);
                break;
            case 0x9C: // SBC A, H
                SBC(h);
                break;
            case 0x9D: // SBC A, L
                SBC(l);
                break;
            case 0x9E: // SBC A, (HL)
                SBC_XR(hl);
                break;
            case 0x9F: // SBC A, A
                SBC(a);
                break;
            case 0xA0: // AND B
                AND(b);
                break;
            case 0xA1: // AND C
                AND(c);
                break;
            case 0xA2: // AND D
                AND(d);
                break;
            case 0xA3: // AND E
                AND(e);
                break;
            case 0xA4: // AND H
                AND(h);
                break;
            case 0xA5: // AND L
                AND(l);
                break;
            case 0xA6: // AND (HL)
                AND_XR(hl);
                break;
            case 0xA7: // AND A
                AND(a);
                break;
            case 0xA8: // XOR B
                XOR(b);
                break;
            case 0xA9: // XOR C
                XOR(c);
                break;
            case 0xAA: // XOR D
                XOR(d);
                break;
            case 0xAB: // XOR E
                XOR(e);
                break;
            case 0xAC: // XOR H
                XOR(h);
                break;
            case 0xAD: // XOR L
                XOR(l);
                break;
            case 0xAE: // XOR (HL)
                XOR_XR(hl);
                break;
            case 0xAF: // XOR A
                XOR(a);
                break;
            case 0xB0: // OR B
                OR(b);
                break;
            case 0xB1: // OR C
                OR(c);
                break;
            case 0xB2: // OR D
                OR(d);
                break;
            case 0xB3: // OR E
                OR(e);
                break;
            case 0xB4: // OR H
                OR(h);
                break;
            case 0xB5: // OR L
                OR(l);
                break;
            case 0xB6: // OR (HL)
                OR_XR(hl);
                break;
            case 0xB7: // OR A
                OR(a);
                break;
            case 0xB8: // CP B
                CP(b);
                break;
            case 0xB9: // CP C
                CP(c);
                break;
            case 0xBA: // CP D
                CP(d);
                break;
            case 0xBB: // CP E
                CP(e);
                break;
            case 0xBC: // CP H
                CP(h);
                break;
            case 0xBD: // CP L
                CP(l);
                break;
            case 0xBE: // CP (HL)
                CP_XR(hl);
                break;
            case 0xBF: // CP A
                CP(a);
                break;
            case 0xC0: // RET NZ
                RET_CND(!(f & ZF));
                break;
            case 0xC1: // POP BC
                POP(bc);
                break;
            case 0xC2: // JP NZ, NN
                JP_CND_NN(!(f & ZF));
                break;
            case 0xC3: // JP NN
                JP_NN;
                break;
            case 0xC4: // CALL NZ, NN
                CALL_CND_NN(!(f & ZF));
                break;
            case 0xC5: // PUSH BC
                PUSH(bc);
                break;
            case 0xC6: // ADD A, N
                ADD_XR(pc++);
                break;
            case 0xC7: // RST 0x00
                RST(0x00);
                break;
            case 0xC8: // RET Z
                RET_CND(f & ZF);
                break;
            case 0xC9: // RET
                RET;
                break;
            case 0xCA: // JP Z, NN
                JP_CND_NN(f & ZF);
                break;
            case 0xCB: // --------------- CB prefix --------------
                irl++;
                switch(memory->read_byte_ex(pc++)){
                    case 0x00: // RLC B
                        RLC(b);
                        break;
                    case 0x01: // RLC C
                        RLC(c);
                        break;
                    case 0x02: // RLC D
                        RLC(d);
                        break;
                    case 0x03: // RLC E
                        RLC(e);
                        break;
                    case 0x04: // RLC H
                        RLC(h);
                        break;
                    case 0x05: // RLC L
                        RLC(l);
                        break;
                    case 0x06: // RLC (HL)
                        RLC_XR(hl);
                        break;
                    case 0x07: // RLC A
                        RLC(a);
                        break;
                    case 0x08: // RRC B
                        RRC(b);
                        break;
                    case 0x09: // RRC C
                        RRC(c);
                        break;
                    case 0x0A: // RRC D
                        RRC(d);
                        break;
                    case 0x0B: // RRC E
                        RRC(e);
                        break;
                    case 0x0C: // RRC H
                        RRC(h);
                        break;
                    case 0x0D: // RRC L
                        RRC(l);
                        break;
                    case 0x0E: // RRC (HL)
                        RRC_XR(hl);
                        break;
                    case 0x0F: // RRC A
                        RRC(a);
                        break;
                    case 0x10: // RL B
                        RL(b);
                        break;
                    case 0x11: // RL C
                        RL(c);
                        break;
                    case 0x12: // RL D
                        RL(d);
                        break;
                    case 0x13: // RL E
                        RL(e);
                        break;
                    case 0x14: // RL H
                        RL(h);
                        break;
                    case 0x15: // RL L
                        RL(l);
                        break;
                    case 0x16: // RL (HL)
                        RL_XR(hl);
                        break;
                    case 0x17: // RL A
                        RL(a);
                        break;
                    case 0x18: // RR B
                        RR(b);
                        break;
                    case 0x19: // RR C
                        RR(c);
                        break;
                    case 0x1A: // RR D
                        RR(d);
                        break;
                    case 0x1B: // RR E
                        RR(e);
                        break;
                    case 0x1C: // RR H
                        RR(h);
                        break;
                    case 0x1D: // RR L
                        RR(l);
                        break;
                    case 0x1E: // RR (HL)
                        RR_XR(hl);
                        break;
                    case 0x1F: // RR A
                        RR(a);
                        break;
                    case 0x20: // SLA B
                        SLA(b);
                        break;
                    case 0x21: // SLA C
                        SLA(c);
                        break;
                    case 0x22: // SLA D
                        SLA(d);
                        break;
                    case 0x23: // SLA E
                        SLA(e);
                        break;
                    case 0x24: // SLA H
                        SLA(h);
                        break;
                    case 0x25: // SLA L
                        SLA(l);
                        break;
                    case 0x26: // SLA (HL)
                        SLA_XR(hl);
                        break;
                    case 0x27: // SLA A
                        SLA(a);
                        break;
                    case 0x28: // SRA B
                        SRA(b);
                        break;
                    case 0x29: // SRA C
                        SRA(c);
                        break;
                    case 0x2A: // SRA D
                        SRA(d);
                        break;
                    case 0x2B: // SRA E
                        SRA(e);
                        break;
                    case 0x2C: // SRA H
                        SRA(h);
                        break;
                    case 0x2D: // SRA L
                        SRA(l);
                        break;
                    case 0x2E: // SRA (HL)
                        SRA_XR(hl);
                        break;
                    case 0x2F: // SRA A
                        SRA(a);
                        break;
                    case 0x30: // SLL B
                        SLL(b);
                        break;
                    case 0x31: // SLL C
                        SLL(c);
                        break;
                    case 0x32: // SLL D
                        SLL(d);
                        break;
                    case 0x33: // SLL E
                        SLL(e);
                        break;
                    case 0x34: // SLL H
                        SLL(h);
                        break;
                    case 0x35: // SLL L
                        SLL(l);
                        break;
                    case 0x36: // SLL (HL)
                        SLL_XR(hl);
                        break;
                    case 0x37: // SLL A
                        SLL(a);
                        break;
                    case 0x38: // SRL B
                        SRL(b);
                        break;
                    case 0x39: // SRL C
                        SRL(c);
                        break;
                    case 0x3A: // SRL D
                        SRL(d);
                        break;
                    case 0x3B: // SRL E
                        SRL(e);
                        break;
                    case 0x3C: // SRL H
                        SRL(h);
                        break;
                    case 0x3D: // SRL L
                        SRL(l);
                        break;
                    case 0x3E: // SRL (HL)
                        SRL_XR(hl);
                        break;
                    case 0x3F: // SRL A
                        SRL(a);
                        break;
                    case 0x40: // BIT 0, B
                        BIT(0, b);
                        break;
                    case 0x41: // BIT 0, C
                        BIT(0, c);
                        break;
                    case 0x42: // BIT 0, D
                        BIT(0, d);
                        break;
                    case 0x43: // BIT 0, E
                        BIT(0, e);
                        break;
                    case 0x44: // BIT 0, H
                        BIT(0, h);
                        break;
                    case 0x45: // BIT 0, L
                        BIT(0, l);
                        break;
                    case 0x46: // BIT 0, (HL)
                        BIT_XR(0, hl);
                        break;
                    case 0x47: // BIT 0, A
                        BIT(0, a);
                        break;
                    case 0x48: // BIT 1, B
                        BIT(1, b);
                        break;
                    case 0x49: // BIT 1, C
                        BIT(1, c);
                        break;
                    case 0x4A: // BIT 1, D
                        BIT(1, d);
                        break;
                    case 0x4B: // BIT 1, E
                        BIT(1, e);
                        break;
                    case 0x4C: // BIT 1, H
                        BIT(1, h);
                        break;
                    case 0x4D: // BIT 1, L
                        BIT(1, l);
                        break;
                    case 0x4E: // BIT 1, (HL)
                        BIT_XR(1, hl);
                        break;
                    case 0x4F: // BIT 1, A
                        BIT(1, a);
                        break;
                    case 0x50: // BIT 2, B
                        BIT(2, b);
                        break;
                    case 0x51: // BIT 2, C
                        BIT(2, c);
                        break;
                    case 0x52: // BIT 2, D
                        BIT(2, d);
                        break;
                    case 0x53: // BIT 2, E
                        BIT(2, e);
                        break;
                    case 0x54: // BIT 2, H
                        BIT(2, h);
                        break;
                    case 0x55: // BIT 2, L
                        BIT(2, l);
                        break;
                    case 0x56: // BIT 2, (HL)
                        BIT_XR(2, hl);
                        break;
                    case 0x57: // BIT 2, A
                        BIT(2, a);
                        break;
                    case 0x58: // BIT 3, B
                        BIT(3, b);
                        break;
                    case 0x59: // BIT 3, C
                        BIT(3, c);
                        break;
                    case 0x5A: // BIT 3, D
                        BIT(3, d);
                        break;
                    case 0x5B: // BIT 3, E
                        BIT(3, e);
                        break;
                    case 0x5C: // BIT 3, H
                        BIT(3, h);
                        break;
                    case 0x5D: // BIT 3, L
                        BIT(3, l);
                        break;
                    case 0x5E: // BIT 3, (HL)
                        BIT_XR(3, hl);
                        break;
                    case 0x5F: // BIT 3, A
                        BIT(3, a);
                        break;
                    case 0x60: // BIT 4, B
                        BIT(4, b);
                        break;
                    case 0x61: // BIT 4, C
                        BIT(4, c);
                        break;
                    case 0x62: // BIT 4, D
                        BIT(4, d);
                        break;
                    case 0x63: // BIT 4, E
                        BIT(4, e);
                        break;
                    case 0x64: // BIT 4, H
                        BIT(4, h);
                        break;
                    case 0x65: // BIT 4, L
                        BIT(4, l);
                        break;
                    case 0x66: // BIT 4, (HL)
                        BIT_XR(4, hl);
                        break;
                    case 0x67: // BIT 4, A
                        BIT(4, a);
                        break;
                    case 0x68: // BIT 5, B
                        BIT(5, b);
                        break;
                    case 0x69: // BIT 5, C
                        BIT(5, c);
                        break;
                    case 0x6A: // BIT 5, D
                        BIT(5, d);
                        break;
                    case 0x6B: // BIT 5, E
                        BIT(5, e);
                        break;
                    case 0x6C: // BIT 5, H
                        BIT(5, h);
                        break;
                    case 0x6D: // BIT 5, L
                        BIT(5, l);
                        break;
                    case 0x6E: // BIT 5, (HL)
                        BIT_XR(5, hl);
                        break;
                    case 0x6F: // BIT 5, A
                        BIT(5, a);
                        break;
                    case 0x70: // BIT 6, B
                        BIT(6, b);
                        break;
                    case 0x71: // BIT 6, C
                        BIT(6, c);
                        break;
                    case 0x72: // BIT 6, D
                        BIT(6, d);
                        break;
                    case 0x73: // BIT 6, E
                        BIT(6, e);
                        break;
                    case 0x74: // BIT 6, H
                        BIT(6, h);
                        break;
                    case 0x75: // BIT 6, L
                        BIT(6, l);
                        break;
                    case 0x76: // BIT 6, (HL)
                        BIT_XR(6, hl);
                        break;
                    case 0x77: // BIT 6, A
                        BIT(6, a);
                        break;
                    case 0x78: // BIT 7, B
                        BIT(7, b);
                        break;
                    case 0x79: // BIT 7, C
                        BIT(7, c);
                        break;
                    case 0x7A: // BIT 7, D
                        BIT(7, d);
                        break;
                    case 0x7B: // BIT 7, E
                        BIT(7, e);
                        break;
                    case 0x7C: // BIT 7, H
                        BIT(7, h);
                        break;
                    case 0x7D: // BIT 7, L
                        BIT(7, l);
                        break;
                    case 0x7E: // BIT 7, (HL)
                        BIT_XR(7, hl);
                        break;
                    case 0x7F: // BIT 7, A
                        BIT(7, a);
                        break;
                    case 0x80: // RES 0, B
                        RES(0, b);
                        break;
                    case 0x81: // RES 0, C
                        RES(0, c);
                        break;
                    case 0x82: // RES 0, D
                        RES(0, d);
                        break;
                    case 0x83: // RES 0, E
                        RES(0, e);
                        break;
                    case 0x84: // RES 0, H
                        RES(0, h);
                        break;
                    case 0x85: // RES 0, L
                        RES(0, l);
                        break;
                    case 0x86: // RES 0, (HL)
                        RES_XR(0, hl);
                        break;
                    case 0x87: // RES 0, A
                        RES(0, a);
                        break;
                    case 0x88: // RES 1, B
                        RES(1, b);
                        break;
                    case 0x89: // RES 1, C
                        RES(1, c);
                        break;
                    case 0x8A: // RES 1, D
                        RES(1, d);
                        break;
                    case 0x8B: // RES 1, E
                        RES(1, e);
                        break;
                    case 0x8C: // RES 1, H
                        RES(1, h);
                        break;
                    case 0x8D: // RES 1, L
                        RES(1, l);
                        break;
                    case 0x8E: // RES 1, (HL)
                        RES_XR(1, hl);
                        break;
                    case 0x8F: // RES 1, A
                        RES(1, a);
                        break;
                    case 0x90: // RES 2, B
                        RES(2, b);
                        break;
                    case 0x91: // RES 2, C
                        RES(2, c);
                        break;
                    case 0x92: // RES 2, D
                        RES(2, d);
                        break;
                    case 0x93: // RES 2, E
                        RES(2, e);
                        break;
                    case 0x94: // RES 2, H
                        RES(2, h);
                        break;
                    case 0x95: // RES 2, L
                        RES(2, l);
                        break;
                    case 0x96: // RES 2, (HL)
                        RES_XR(2, hl);
                        break;
                    case 0x97: // RES 2, A
                        RES(2, a);
                        break;
                    case 0x98: // RES 3, B
                        RES(3, b);
                        break;
                    case 0x99: // RES 3, C
                        RES(3, c);
                        break;
                    case 0x9A: // RES 3, D
                        RES(3, d);
                        break;
                    case 0x9B: // RES 3, E
                        RES(3, e);
                        break;
                    case 0x9C: // RES 3, H
                        RES(3, h);
                        break;
                    case 0x9D: // RES 3, L
                        RES(3, l);
                        break;
                    case 0x9E: // RES 3, (HL)
                        RES_XR(3, hl);
                        break;
                    case 0x9F: // RES 3, A
                        RES(3, a);
                        break;
                    case 0xA0: // RES 4, B
                        RES(4, b);
                        break;
                    case 0xA1: // RES 4, C
                        RES(4, c);
                        break;
                    case 0xA2: // RES 4, D
                        RES(4, d);
                        break;
                    case 0xA3: // RES 4, E
                        RES(4, e);
                        break;
                    case 0xA4: // RES 4, H
                        RES(4, h);
                        break;
                    case 0xA5: // RES 4, L
                        RES(4, l);
                        break;
                    case 0xA6: // RES 4, (HL)
                        RES_XR(4, hl);
                        break;
                    case 0xA7: // RES 4, A
                        RES(4, a);
                        break;
                    case 0xA8: // RES 5, B
                        RES(5, b);
                        break;
                    case 0xA9: // RES 5, C
                        RES(5, c);
                        break;
                    case 0xAA: // RES 5, D
                        RES(5, d);
                        break;
                    case 0xAB: // RES 5, E
                        RES(5, e);
                        break;
                    case 0xAC: // RES 5, H
                        RES(5, h);
                        break;
                    case 0xAD: // RES 5, L
                        RES(5, l);
                        break;
                    case 0xAE: // RES 5, (HL)
                        RES_XR(5, hl);
                        break;
                    case 0xAF: // RES 5, A
                        RES(5, a);
                        break;
                    case 0xB0: // RES 6, B
                        RES(6, b);
                        break;
                    case 0xB1: // RES 6, C
                        RES(6, c);
                        break;
                    case 0xB2: // RES 6, D
                        RES(6, d);
                        break;
                    case 0xB3: // RES 6, E
                        RES(6, e);
                        break;
                    case 0xB4: // RES 6, H
                        RES(6, h);
                        break;
                    case 0xB5: // RES 6, L
                        RES(6, l);
                        break;
                    case 0xB6: // RES 6, (HL)
                        RES_XR(6, hl);
                        break;
                    case 0xB7: // RES 6, A
                        RES(6, a);
                        break;
                    case 0xB8: // RES 7, B
                        RES(7, b);
                        break;
                    case 0xB9: // RES 7, C
                        RES(7, c);
                        break;
                    case 0xBA: // RES 7, D
                        RES(7, d);
                        break;
                    case 0xBB: // RES 7, E
                        RES(7, e);
                        break;
                    case 0xBC: // RES 7, H
                        RES(7, h);
                        break;
                    case 0xBD: // RES 7, L
                        RES(7, l);
                        break;
                    case 0xBE: // RES 7, (HL)
                        RES_XR(7, hl);
                        break;
                    case 0xBF: // RES 7, A
                        RES(7, a);
                        break;
                    case 0xC0: // SET 0, B
                        SET(0, b);
                        break;
                    case 0xC1: // SET 0, C
                        SET(0, c);
                        break;
                    case 0xC2: // SET 0, D
                        SET(0, d);
                        break;
                    case 0xC3: // SET 0, E
                        SET(0, e);
                        break;
                    case 0xC4: // SET 0, H
                        SET(0, h);
                        break;
                    case 0xC5: // SET 0, L
                        SET(0, l);
                        break;
                    case 0xC6: // SET 0, (HL)
                        SET_XR(0, hl);
                        break;
                    case 0xC7: // SET 0, A
                        SET(0, a);
                        break;
                    case 0xC8: // SET 1, B
                        SET(1, b);
                        break;
                    case 0xC9: // SET 1, C
                        SET(1, c);
                        break;
                    case 0xCA: // SET 1, D
                        SET(1, d);
                        break;
                    case 0xCB: // SET 1, E
                        SET(1, e);
                        break;
                    case 0xCC: // SET 1, H
                        SET(1, h);
                        break;
                    case 0xCD: // SET 1, L
                        SET(1, l);
                        break;
                    case 0xCE: // SET 1, (HL)
                        SET_XR(1, hl);
                        break;
                    case 0xCF: // SET 1, A
                        SET(1, a);
                        break;
                    case 0xD0: // SET 2, B
                        SET(2, b);
                        break;
                    case 0xD1: // SET 2, C
                        SET(2, c);
                        break;
                    case 0xD2: // SET 2, D
                        SET(2, d);
                        break;
                    case 0xD3: // SET 2, E
                        SET(2, e);
                        break;
                    case 0xD4: // SET 2, H
                        SET(2, h);
                        break;
                    case 0xD5: // SET 2, L
                        SET(2, l);
                        break;
                    case 0xD6: // SET 2, (HL)
                        SET_XR(2, hl);
                        break;
                    case 0xD7: // SET 2, A
                        SET(2, a);
                        break;
                    case 0xD8: // SET 3, B
                        SET(3, b);
                        break;
                    case 0xD9: // SET 3, C
                        SET(3, c);
                        break;
                    case 0xDA: // SET 3, D
                        SET(3, d);
                        break;
                    case 0xDB: // SET 3, E
                        SET(3, e);
                        break;
                    case 0xDC: // SET 3, H
                        SET(3, h);
                        break;
                    case 0xDD: // SET 3, L
                        SET(3, l);
                        break;
                    case 0xDE: // SET 3, (HL)
                        SET_XR(3, hl);
                        break;
                    case 0xDF: // SET 3, A
                        SET(3, a);
                        break;
                    case 0xE0: // SET 4, B
                        SET(4, b);
                        break;
                    case 0xE1: // SET 4, C
                        SET(4, c);
                        break;
                    case 0xE2: // SET 4, D
                        SET(4, d);
                        break;
                    case 0xE3: // SET 4, E
                        SET(4, e);
                        break;
                    case 0xE4: // SET 4, H
                        SET(4, h);
                        break;
                    case 0xE5: // SET 4, L
                        SET(4, l);
                        break;
                    case 0xE6: // SET 4, (HL)
                        SET_XR(4, hl);
                        break;
                    case 0xE7: // SET 4, A
                        SET(4, a);
                        break;
                    case 0xE8: // SET 5, B
                        SET(5, b);
                        break;
                    case 0xE9: // SET 5, C
                        SET(5, c);
                        break;
                    case 0xEA: // SET 5, D
                        SET(5, d);
                        break;
                    case 0xEB: // SET 5, E
                        SET(5, e);
                        break;
                    case 0xEC: // SET 5, H
                        SET(5, h);
                        break;
                    case 0xED: // SET 5, L
                        SET(5, l);
                        break;
                    case 0xEE: // SET 5, (HL)
                        SET_XR(5, hl);
                        break;
                    case 0xEF: // SET 5, A
                        SET(5, a);
                        break;
                    case 0xF0: // SET 6, B
                        SET(6, b);
                        break;
                    case 0xF1: // SET 6, C
                        SET(6, c);
                        break;
                    case 0xF2: // SET 6, D
                        SET(6, d);
                        break;
                    case 0xF3: // SET 6, E
                        SET(6, e);
                        break;
                    case 0xF4: // SET 6, H
                        SET(6, h);
                        break;
                    case 0xF5: // SET 6, L
                        SET(6, l);
                        break;
                    case 0xF6: // SET 6, (HL)
                        SET_XR(6, hl);
                        break;
                    case 0xF7: // SET 6, A
                        SET(6, a);
                        break;
                    case 0xF8: // SET 7, B
                        SET(7, b);
                        break;
                    case 0xF9: // SET 7, C
                        SET(7, c);
                        break;
                    case 0xFA: // SET 7, D
                        SET(7, d);
                        break;
                    case 0xFB: // SET 7, E
                        SET(7, e);
                        break;
                    case 0xFC: // SET 7, H
                        SET(7, h);
                        break;
                    case 0xFD: // SET 7, L
                        SET(7, l);
                        break;
                    case 0xFE: // SET 7, (HL)
                        SET_XR(7, hl);
                        break;
                    case 0xFF: // SET 7, A
                        SET(7, a);
                        break;
                }
                break;
            case 0xCC: // CALL Z, NN
                CALL_CND_NN(f & ZF);
                break;
            case 0xCD: // CALL NN
                CALL_NN;
                break;
            case 0xCE: // ADC A, N
                ADC_XR(pc++);
                break;
            case 0xCF: // RST 8
                RST(0x08);
                break;
            case 0xD0: // RET NC
                RET_CND(!(f & CF));
                break;
            case 0xD1: // POP DE
                POP(de);
                break;
            case 0xD2: // JP NC, NN
                JP_CND_NN(!(f & CF));
                break;
            case 0xD3: // OUT (N), A
                OUT_N_A;
                break;
            case 0xD4: // CALL NC, NN
                CALL_CND_NN(!(f & CF));
                break;
            case 0xD5: // PUSH DE
                PUSH(de);
                break;
            case 0xD6: // SUB A, N
                SUB_XR(pc++);
                break;
            case 0xD7: // RST 10
                RST(0x10);
                break;
            case 0xD8: // RET C
                RET_CND(f & CF);
                break;
            case 0xD9: // EXX
                bc ^= alt.bc;
                alt.bc ^= bc;
                bc ^= alt.bc;
                de ^= alt.de;
                alt.de ^= de;
                de ^= alt.de;
                hl ^= alt.hl;
                alt.hl ^= hl;
                hl ^= alt.hl;
                time(4);
                break;
            case 0xDA: // JP C, NN
                JP_CND_NN(f & CF);
                break;
            case 0xDB: // IN A, (N)
                IN_A_N;
                break;
            case 0xDC: // CALL C, NN
                CALL_CND_NN(f & CF);
                break;
            case 0xDD: // --------------- DD prefix --------------
                irl++;
                time(4);
                switch(memory->read_byte_ex(pc++)){
                    case 0x00: // NOP
                        time(4);
                        break;
                    case 0x01: // LD BC, NN
                        LD_RR_NN(bc);
                        break;
                    case 0x02: // LD (BC), A
                        LD_XR_A(bc);
                        break;
                    case 0x03: // INC BC
                        INC_RR(bc);
                        break;
                    case 0x04: // INC B
                        INC(b);
                        break;
                    case 0x05: // DEC B
                        DEC(b);
                        break;
                    case 0x06: // LD B, N
                        LD_R_XR(b, pc++);
                        break;
                    case 0x07: // RLCA
                        a = (a << 1) | a >> 7;
                        f = (f & (SF | ZF | PF)) | (a & (F5 | F3 | CF));
                        time(4);
                        break;
                    case 0x08: // EX AF, AF'
                        af ^= alt.af;
                        alt.af ^= af;
                        af ^= alt.af;
                        time(4);
                        break;
                    case 0x09: // ADD IX, BC
                        ADD16(ix, bc);
                        break;
                    case 0x0A: // LD A, (BC)
                        LD_A_XR(bc);
                        break;
                    case 0x0B: // DEC BC
                        DEC_RR(bc);
                        break; 
                    case 0x0C: // INC C
                        INC(c);
                        break;
                    case 0x0D: // DEC C
                        DEC(c);
                        break;
                    case 0x0E: // LD C, N
                        LD_R_XR(c, pc++);
                        break;
                    case 0x0F: // RRCA
                        f = (f & (SF | ZF | PF)) | (a & CF);
                        a = (a >> 1) | (a << 7);
                        f |= (a & (F5 | F3));
                        time(4);
                        break;
                    case 0x10: // DJNZ N
                        if (--b){
                            pc += (s8)memory->read_byte(pc) + 1;
                            memptr = pc;
                            time(13);
                        }else{
                            pc++;
                            time(8);
                        }
                        break;
                    case 0x11: // LD DE, NN
                        LD_RR_NN(de);
                        break;
                    case 0x12: // LD (DE), A
                        LD_XR_A(de);
                        break;
                    case 0x13: // INC DE
                        INC_RR(de);
                        break;
                    case 0x14: // INC D
                        INC(d);
                        break;
                    case 0x15: // DEC D
                        DEC(d);
                        break;
                    case 0x16: // LD D, N
                        LD_R_XR(d, pc++);
                        break;
                    case 0x17: // RLA
                        RLA;
                        break;
                    case 0x18: // JR N
                        JR_N;
                        break;
                    case 0x19: // ADD IX, DE
                        ADD16(ix, de);
                        break;
                    case 0x1A: // LD A, (DE)
                        LD_A_XR(de);
                        break;
                    case 0x1B: // DEC DE
                        DEC_RR(de);
                        break;
                    case 0x1C: // INC E
                        INC(e);
                        break;
                    case 0x1D: // DEC E
                        DEC(e);
                        break; 
                    case 0x1E: // LD E, N
                        LD_R_XR(e, pc++);
                        break;
                    case 0x1F: // RRA
                        RRA;
                        break;
                    case 0x20: // JR NZ, N
                        JR_CND_N(!(f & ZF));
                        break;
                    case 0x21: // LD IX, NN
                        LD_RR_NN(ix);
                        break;
                    case 0x22: // LD (NN), IX
                        LD_MM_RR(ix);
                        break;
                    case 0x23: // INC IX
                        INC_RR(ix);
                        break;
                    case 0x24: // INC IXH
                        INC(ixh);
                        break;
                    case 0x25: // DEC IXH
                        DEC(ixh);
                        break;
                    case 0x26: // LD IXH, N
                        LD_R_XR(ixh, pc++);
                        break;
                    case 0x27: // DAA Check
                        DAA;
                        break;
                    case 0x28: // JR Z, N
                        JR_CND_N(f & ZF);
                        break;
                    case 0x29: // ADD IX, IX
                        ADD16(ix, ix);
                        break;
                    case 0x2A: // LD IX, (NN)
                        LD_RR_MM(ix);
                        break;
                    case 0x2B: // DEC IX
                        DEC_RR(ix);
                        break;
                    case 0x2C: // INC IXL
                        INC(ixl);
                        break;
                    case 0x2D: // DEC IXL
                        DEC(ixl);
                        break;
                    case 0x2E: // LD IXL, N
                        LD_R_XR(ixl, pc++);
                        break;
                    case 0x2F: // CPL
                        a ^= 0xFF;
                        f = (f & (SF | ZF | PF | CF)) | HF | NF | (a & (F3 | F5));
                        time(4);
                        break;
                    case 0x30: // JR NC, N
                        JR_CND_N(!(f & CF))
                        break;
                    case 0x31: // LD SP, NN
                        LD_RR_NN(sp);
                        break;
                    case 0x32: // LD (NN), A
                        LD_MM_A;
                        break;
                    case 0x33: // INC SP
                        INC_RR(sp);
                        break;
                    case 0x34: // INC (IX + s)
                        INC_XS(ix);
                        break;
                    case 0x35: // DEC (IX + s)
                        DEC_XS(ix);
                        break;
                    case 0x36: // LD (IX + s), N
                        LD_XS_N(ix)
                        break;
                    case 0x37: // SCF
                        f = (f & (SF | ZF | PF)) | CF | (a & (F3 | F5));
                        time(4);
                        break;
                    case 0x38: // JR C, N
                        JR_CND_N(f & CF);
                        break;
                    case 0x39: // ADD IX, SP
                        ADD16(ix, sp);
                        break;
                    case 0x3A: // LD A, (NN)
                        LD_A_MM;
                        break;
                    case 0x3B: // DEC SP
                        DEC_RR(sp);
                        break;
                    case 0x3C: // INC A
                        INC(a);
                        break;
                    case 0x3D: // DEC A
                        DEC(a);
                        break;
                    case 0x3E: // LD A, N
                        LD_R_XR(a, pc++);
                        break;
                    case 0x3F: // CCF
                        f = ((f & ~(NF | HF)) | ((f << 4) & HF) | (a & (F3 | F5))) ^ CF;
                        time(4);
                        break;
                    case 0x40: // LD B, B
                        time(4);
                        break;
                    case 0x41: // LD B, C
                        LD_R_R(b, c);
                        break;
                    case 0x42: // LD B, D
                        LD_R_R(b, d);
                        break;
                    case 0x43: // LD B, E
                        LD_R_R(b, e);
                        break;
                    case 0x44: // LD B, IXH
                        LD_R_R(b, ixh);
                        break;
                    case 0x45: // LD B, IXL
                        LD_R_R(b, ixl);
                        break;
                    case 0x46: // LD B, (IX + s)
                        LD_R_XS(b, ix);
                        break;
                    case 0x47: // LD B, A
                        LD_R_R(b, a);
                        break;
                    case 0x48: // LD C, B
                        LD_R_R(c, b);
                        break;
                    case 0x49: // LD C, C
                        time(4);
                        break;
                    case 0x4A: // LD C, D
                        LD_R_R(c, d);
                        break;
                    case 0x4B: // LD C, E
                        LD_R_R(c, e);
                        break;
                    case 0x4C: // LD C, IXH
                        LD_R_R(c, ixh);
                        break;
                    case 0x4D: // LD C, IXL
                        LD_R_R(c, ixl);
                        break;
                    case 0x4E: // LD C, (IX + s)
                        LD_R_XS(c, ix);
                        break;
                    case 0x4F: // LD C, A
                        LD_R_R(c, a);
                        break;
                    case 0x50: // LD D, B
                        LD_R_R(d, b);
                        break;
                    case 0x51: // LD D, C
                        LD_R_R(d, c);
                        break;
                    case 0x52: // LD D, D
                        time(4);
                        break;
                    case 0x53: // LD D, E
                        LD_R_R(d, e);
                        break;
                    case 0x54: // LD D, IXH
                        LD_R_R(d, ixh);
                        break;
                    case 0x55: // LD D, IXL
                        LD_R_R(d, ixl);
                        break;
                    case 0x56: // LD D, (IX + s)
                        LD_R_XS(d, ix);
                        break;
                    case 0x57: // LD D, A
                        LD_R_R(d, a);
                        break;
                    case 0x58: // LD E, B
                        LD_R_R(e, b);
                        break;
                    case 0x59: // LD E, C
                        LD_R_R(e, c);
                        break;
                    case 0x5A: // LD E, D
                        LD_R_R(e, d);
                        break;
                    case 0x5B: // LD E, E
                        time(4);
                        break;
                    case 0x5C: // LD E, IXH
                        LD_R_R(e, ixh);
                        break;
                    case 0x5D: // LD E, IXL
                        LD_R_R(e, ixl);
                        break;
                    case 0x5E: // LD E, (IX + s)
                        LD_R_XS(e, ix);
                        break;
                    case 0x5F: // LD E, A
                        LD_R_R(e, a);
                        break;
                    case 0x60: // LD IXH, B
                        LD_R_R(ixh, b);
                        break;
                    case 0x61: // LD IXH, C
                        LD_R_R(ixh, c);
                        break;
                    case 0x62: // LD IXH, D
                        LD_R_R(ixh, d);
                        break;
                    case 0x63: // LD IXH, E
                        LD_R_R(ixh, e);
                        break;
                    case 0x64: // LD IXH, IXH
                        time(4);
                        break;
                    case 0x65: // LD IXH, IXL
                        LD_R_R(ixh, ixl);
                        break;
                    case 0x66: // LD H, (IX + s)
                        LD_R_XS(h, ix);
                        break;
                    case 0x67: // LD IXH, A
                        LD_R_R(ixh, a);
                        break;
                    case 0x68: // LD IXL, B
                        LD_R_R(ixl, b);
                        break;
                    case 0x69: // LD IXL, C
                        LD_R_R(ixl, c);
                        break;
                    case 0x6A: // LD IXL, D
                        LD_R_R(ixl, d);
                        break;
                    case 0x6B: // LD IXL, E
                        LD_R_R(ixl, e);
                        break;
                    case 0x6C: // LD IXL, IXH
                        LD_R_R(ixl, ixh);
                        break;
                    case 0x6D: // LD IXL, IXL
                        time(4);
                        break;
                    case 0x6E: // LD L, (IX + s);
                        LD_R_XS(l, ix);
                        break;
                    case 0x6F: // LD IXL, A
                        LD_R_R(ixl, a);
                        break;
                    case 0x70: // LD (IX + s), B
                        LD_XS_R(ix, b);
                        break;
                    case 0x71: // LD (IX + s), C
                        LD_XS_R(ix, c);
                        break;
                    case 0x72: // LD (IX + s), D
                        LD_XS_R(ix, d);
                        break;
                    case 0x73: // LD (IX + s), E
                        LD_XS_R(ix, e);
                        break;
                    case 0x74: // LD (IX + s), H
                        LD_XS_R(ix, h);
                        break;
                    case 0x75: // LD (IX + s), L
                        LD_XS_R(ix, l);
                        break;
                    case 0x76: // HALT
                        pc--;
                        time(4);
                        break;
                    case 0x77: // LD (IX + s), A
                        LD_XS_R(ix, a);
                        break;
                    case 0x78: // LD A, B
                        LD_R_R(a, b);
                        break;
                    case 0x79: // LD A, C
                        LD_R_R(a, c);
                        break;
                    case 0x7A: // LD A, D
                        LD_R_R(a, d);
                        break;
                    case 0x7B: // LD A, E
                        LD_R_R(a, e);
                        break;
                    case 0x7C: // LD A, IXH
                        LD_R_R(a, ixh);
                        break;
                    case 0x7D: // LD A, IXL
                        LD_R_R(a, ixl);
                        break;
                    case 0x7E: // LD A, (IX + s)
                        LD_R_XS(a, ix);
                        break;
                    case 0x7F: // LD A, A
                        time(4);
                        break;
                    case 0x80: // ADD A, B
                        ADD(b);
                        break;
                    case 0x81: // ADD A, C
                        ADD(c);
                        break;
                    case 0x82: // ADD A, D
                        ADD(d);
                        break;
                    case 0x83: // ADD A, E
                        ADD(e);
                        break;
                    case 0x84: // ADD A, IXH
                        ADD(ixh);
                        break;
                    case 0x85: // ADD A, IXL
                        ADD(ixl);
                        break;
                    case 0x86: // ADD A, (IX + s)
                        ADD_XS(ix);
                        break;
                    case 0x87: // ADD A, A
                        ADD(a);
                        break;
                    case 0x88: // ADC A, B
                        ADC(b);
                        break;
                    case 0x89: // ADC A, C
                        ADC(c);
                        break;
                    case 0x8A: // ADC A, D
                        ADC(d);
                        break;
                    case 0x8B: // ADC A, E
                        ADC(e);
                        break;
                    case 0x8C: // ADC A, IXH
                        ADC(ixh);
                        break;
                    case 0x8D: // ADC A, IXL
                        ADC(ixl);
                        break;
                    case 0x8E: // ADC A, (IX + s)
                        ADC_XS(ix);
                        break;
                    case 0x8F: // ADC A, A
                        ADC(a);
                        break;
                    case 0x90: // SUB A, B
                        SUB(b);
                        break;
                    case 0x91: // SUB A, C
                        SUB(c);
                        break;
                    case 0x92: // SUB A, D
                        SUB(d);
                        break;
                    case 0x93: // SUB A, E
                        SUB(e);
                        break;
                    case 0x94: // SUB A, IXH
                        SUB(ixh);
                        break;
                    case 0x95: // SUB A, IXL
                        SUB(ixl);
                        break;
                    case 0x96: // SUB A, (IX + s)
                        SUB_XS(ix);
                        break;
                    case 0x97: // SUB A, A
                        SUB(a);
                        break;
                    case 0x98: // SBC A, B
                        SBC(b);
                        break;
                    case 0x99: // SBC A, C
                        SBC(c);
                        break;
                    case 0x9A: // SBC A, D
                        SBC(d);
                        break;
                    case 0x9B: // SBC A, E
                        SBC(e);
                        break;
                    case 0x9C: // SBC A, IXH
                        SBC(ixh);
                        break;
                    case 0x9D: // SBC A, IXL
                        SBC(ixl);
                        break;
                    case 0x9E: // SBC A, (IX + s)
                        SBC_XS(ix);
                        break;
                    case 0x9F: // SBC A, A
                        SBC(a);
                        break;
                    case 0xA0: // AND B
                        AND(b);
                        break;
                    case 0xA1: // AND C
                        AND(c);
                        break;
                    case 0xA2: // AND D
                        AND(d);
                        break;
                    case 0xA3: // AND E
                        AND(e);
                        break;
                    case 0xA4: // AND IXH
                        AND(ixh);
                        break;
                    case 0xA5: // AND IXL
                        AND(ixl);
                        break;
                    case 0xA6: // AND (IX + s)
                        AND_XS(ix);
                        break;
                    case 0xA7: // AND A
                        AND(a);
                        break;
                    case 0xA8: // XOR B
                        XOR(b);
                        break;
                    case 0xA9: // XOR C
                        XOR(c);
                        break;
                    case 0xAA: // XOR D
                        XOR(d);
                        break;
                    case 0xAB: // XOR E
                        XOR(e);
                        break;
                    case 0xAC: // XOR IXH
                        XOR(ixh);
                        break;
                    case 0xAD: // XOR IXL
                        XOR(ixl);
                        break;
                    case 0xAE: // XOR (IX + s)
                        XOR_XS(ix);
                        break;
                    case 0xAF: // XOR A
                        XOR(a);
                        break;
                    case 0xB0: // OR B
                        OR(b);
                        break;
                    case 0xB1: // OR C
                        OR(c);
                        break;
                    case 0xB2: // OR D
                        OR(d);
                        break;
                    case 0xB3: // OR E
                        OR(e);
                        break;
                    case 0xB4: // OR IXH
                        OR(ixh);
                        break;
                    case 0xB5: // OR IXL
                        OR(ixl);
                        break;
                    case 0xB6: // OR (IX + s)
                        OR_XS(ix);
                        break;
                    case 0xB7: // OR A
                        OR(a);
                        break;
                    case 0xB8: // CP B
                        CP(b);
                        break;
                    case 0xB9: // CP C
                        CP(c);
                        break;
                    case 0xBA: // CP D
                        CP(d);
                        break;
                    case 0xBB: // CP E
                        CP(e);
                        break;
                    case 0xBC: // CP IXH
                        CP(ixh);
                        break;
                    case 0xBD: // CP IXL
                        CP(ixl);
                        break;
                    case 0xBE: // CP (IX + s)
                        CP_XS(ix);
                        break;
                    case 0xBF: // CP A
                        CP(a);
                        break;
                    case 0xC0: // RET NZ
                        RET_CND(!(f & ZF));
                        break;
                    case 0xC1: // POP BC
                        POP(bc);
                        break;
                    case 0xC2: // JP NZ, NN
                        JP_CND_NN(!(f & ZF));
                        break;
                    case 0xC3: // JP NN
                        JP_NN;
                        break;
                    case 0xC4: // CALL NZ, NN
                        CALL_CND_NN(!(f & ZF));
                        break;
                    case 0xC5: // PUSH BC
                        PUSH(bc);
                        break;
                    case 0xC6: // ADD A, N
                        ADD_XR(pc++);
                        break;
                    case 0xC7: // RST 0x00
                        RST(0x00);
                        break;
                    case 0xC8: // RET Z
                        RET_CND(f & ZF);
                        break;
                    case 0xC9: // RET
                        RET;
                        break;
                    case 0xCA: // JP Z, NN
                        JP_CND_NN(f & ZF);
                        break;
                    case 0xCB: // --------------- DDCB prefix --------------
                        pc++;
                        switch(memory->read_byte_ex(pc++)){
                            case 0x00: // RLC (IX + s), B
                                RLC_XS_R(ix, b);
                                break;
                            case 0x01: // RLC (IX + s), C
                                RLC_XS_R(ix, c);
                                break;
                            case 0x02: // RLC (IX + s), D
                                RLC_XS_R(ix, d);
                                break;
                            case 0x03: // RLC (IX + s), E
                                RLC_XS_R(ix, e);
                                break;
                            case 0x04: // RLC (IX + s), H
                                RLC_XS_R(ix, h);
                                break;
                            case 0x05: // RLC (IX + s), L
                                RLC_XS_R(ix, l);
                                break;
                            case 0x06: // RLC (IX + s)
                                RLC_XS(ix);
                                break;
                            case 0x07: // RLC (IX + s), A
                                RLC_XS_R(ix, a);
                                break;
                            case 0x08: // RRC (IX + s), B
                                RRC_XS_R(ix, b);
                                break;
                            case 0x09: // RRC (IX + s), C
                                RRC_XS_R(ix, c);
                                break;
                            case 0x0A: // RRC (IX + s), D
                                RRC_XS_R(ix, d);
                                break;
                            case 0x0B: // RRC (IX + s), E
                                RRC_XS_R(ix, e);
                                break;
                            case 0x0C: // RRC (IX + s), H
                                RRC_XS_R(ix, h);
                                break;
                            case 0x0D: // RRC (IX + s), L
                                RRC_XS_R(ix, l);
                                break;
                            case 0x0E: // RRC (IX + s)
                                RRC_XS(ix);
                                break;
                            case 0x0F: // RRC (IX + s), A
                                RRC_XS_R(ix, a);
                                break;
                            case 0x10: // RL (IX + s), B
                                RL_XS_R(ix, b);
                                break;
                            case 0x11: // RL (IX + s), C
                                RL_XS_R(ix, c);
                                break;
                            case 0x12: // RL (IX + s), D
                                RL_XS_R(ix, d);
                                break;
                            case 0x13: // RL (IX + s), E
                                RL_XS_R(ix, e);
                                break;
                            case 0x14: // RL (IX + s), H
                                RL_XS_R(ix, h);
                                break;
                            case 0x15: // RL (IX + s), L
                                RL_XS_R(ix, l);
                                break;
                            case 0x16: // RL (IX + s)
                                RL_XS(ix);
                                break;
                            case 0x17: // RL (IX + s), A
                                RL_XS_R(ix, a);
                                break;
                            case 0x18: // RR (IX + s), B
                                RR_XS_R(ix, b);
                                break;
                            case 0x19: // RR (IX + s), C
                                RR_XS_R(ix, c);
                                break;
                            case 0x1A: // RR (IX + s), D
                                RR_XS_R(ix, d);
                                break;
                            case 0x1B: // RR (IX + s), E
                                RR_XS_R(ix, e);
                                break;
                            case 0x1C: // RR (IX + s), H
                                RR_XS_R(ix, h);
                                break;
                            case 0x1D: // RR (IX + s), L
                                RR_XS_R(ix, l);
                                break;
                            case 0x1E: // RR (IX + s)
                                RR_XS(ix);
                                break;
                            case 0x1F: // RR (IX + s), A
                                RR_XS_R(ix, a);
                                break;
                            case 0x20: // SLA (IX + s), B
                                SLA_XS_R(ix, b);
                                break;
                            case 0x21: // SLA (IX + s), C
                                SLA_XS_R(ix, c);
                                break;
                            case 0x22: // SLA (IX + s), D
                                SLA_XS_R(ix, d);
                                break;
                            case 0x23: // SLA (IX + s), E
                                SLA_XS_R(ix, e);
                                break;
                            case 0x24: // SLA (IX + s), H
                                SLA_XS_R(ix, h);
                                break;
                            case 0x25: // SLA (IX + s), L
                                SLA_XS_R(ix, l);
                                break;
                            case 0x26: // SLA (IX + s)
                                SLA_XS(ix);
                                break;
                            case 0x27: // SLA (IX + s), A
                                SLA_XS_R(ix, a);
                                break;
                            case 0x28: // SRA (IX + s), B
                                SRA_XS_R(ix, b);
                                break;
                            case 0x29: // SRA (IX + s), C
                                SRA_XS_R(ix, c);
                                break;
                            case 0x2A: // SRA (IX + s), D
                                SRA_XS_R(ix, d);
                                break;
                            case 0x2B: // SRA (IX + s), E
                                SRA_XS_R(ix, e);
                                break;
                            case 0x2C: // SRA (IX + s), H
                                SRA_XS_R(ix, h);
                                break;
                            case 0x2D: // SRA (IX + s), L
                                SRA_XS_R(ix, l);
                                break;
                            case 0x2E: // SRA (IX + s)
                                SRA_XS(ix);
                                break;
                            case 0x2F: // SRA (IX + s), A
                                SRA_XS_R(ix, a);
                                break;
                            case 0x30: // SLL (IX + s), B
                                SLL_XS_R(ix, b);
                                break;
                            case 0x31: // SLL (IX + s), C
                                SLL_XS_R(ix, c);
                                break;
                            case 0x32: // SLL (IX + s), D
                                SLL_XS_R(ix, d);
                                break;
                            case 0x33: // SLL (IX + s), E
                                SLL_XS_R(ix, e);
                                break;
                            case 0x34: // SLL (IX + s), H
                                SLL_XS_R(ix, h);
                                break;
                            case 0x35: // SLL (IX + s), L
                                SLL_XS_R(ix, l);
                                break;
                            case 0x36: // SLL (IX + s)
                                SLL_XS(ix);
                                break;
                            case 0x37: // SLL (IX + s), A
                                SLL_XS_R(ix, a);
                                break;
                            case 0x38: // SRL (IX + s), B
                                SRL_XS_R(ix, b);
                                break;
                            case 0x39: // SRL (IX + s), C
                                SRL_XS_R(ix, c);
                                break;
                            case 0x3A: // SRL (IX + s), D
                                SRL_XS_R(ix, d);
                                break;
                            case 0x3B: // SRL (IX + s), E
                                SRL_XS_R(ix, e);
                                break;
                            case 0x3C: // SRL (IX + s), H
                                SRL_XS_R(ix, h);
                                break;
                            case 0x3D: // SRL (IX + s), L
                                SRL_XS_R(ix, l);
                                break;
                            case 0x3E: // SRL (IX + s)
                                SRL_XS(ix);
                                break;
                            case 0x3F: // SRL (IX + s), A
                                SRL_XS_R(ix, a);
                                break;
                            case 0x40: // BIT 0, (IX + s)
                            case 0x41:
                            case 0x42:
                            case 0x43:
                            case 0x44:
                            case 0x45:
                            case 0x46:
                            case 0x47:
                                BIT_XS(0, ix);
                                break;
                            case 0x48: // BIT 1, (IX + s)
                            case 0x49:
                            case 0x4A:
                            case 0x4B:
                            case 0x4C:
                            case 0x4D:
                            case 0x4E:
                            case 0x4F:
                                BIT_XS(1, ix);
                                break;
                            case 0x50: // BIT 2, (IX + s)
                            case 0x51:
                            case 0x52:
                            case 0x53:
                            case 0x54:
                            case 0x55:
                            case 0x56:
                            case 0x57:
                                BIT_XS(2, ix);
                                break;
                            case 0x58: // BIT 3, (IX + s)
                            case 0x59:
                            case 0x5A:
                            case 0x5B:
                            case 0x5C:
                            case 0x5D:
                            case 0x5E:
                            case 0x5F:
                                BIT_XS(3, ix);
                                break;
                            case 0x60: // BIT 4, (IX + s)
                            case 0x61:
                            case 0x62:
                            case 0x63:
                            case 0x64:
                            case 0x65:
                            case 0x66:
                            case 0x67:
                                BIT_XS(4, ix);
                                break;
                            case 0x68: // BIT 5, (IX + s)
                            case 0x69:
                            case 0x6A:
                            case 0x6B:
                            case 0x6C:
                            case 0x6D:
                            case 0x6E:
                            case 0x6F:
                                BIT_XS(5, ix);
                                break;
                            case 0x70: // BIT 6, (IX + s)
                            case 0x71:
                            case 0x72:
                            case 0x73:
                            case 0x74:
                            case 0x75:
                            case 0x76:
                            case 0x77:
                                BIT_XS(6, ix);
                                break;
                            case 0x78: // BIT 7, (IX + s)
                            case 0x79:
                            case 0x7A:
                            case 0x7B:
                            case 0x7C:
                            case 0x7D:
                            case 0x7E:
                            case 0x7F:
                                BIT_XS(7, ix);
                                break;
                            case 0x80: // RES 0, (IX + s), B
                                RES_XS_R(0, ix, b);
                                break;
                            case 0x81: // RES 0, (IX + s), C
                                RES_XS_R(0, ix, c);
                                break;
                            case 0x82: // RES 0, (IX + s), D
                                RES_XS_R(0, ix, d);
                                break;
                            case 0x83: // RES 0, (IX + s), E
                                RES_XS_R(0, ix, e);
                                break;
                            case 0x84: // RES 0, (IX + s), H
                                RES_XS_R(0, ix, h);
                                break;
                            case 0x85: // RES 0, (IX + s), L
                                RES_XS_R(0, ix, l);
                                break;
                            case 0x86: // RES 0, (IX + s)
                                RES_XS(0, ix);
                                break;
                            case 0x87: // RES 0, (IX + s), A
                                RES_XS_R(0, ix, a);
                                break;
                            case 0x88: // RES 1, (IX + s), B
                                RES_XS_R(1, ix, b);
                                break;
                            case 0x89: // RES 1, (IX + s), C
                                RES_XS_R(1, ix, c);
                                break;
                            case 0x8A: // RES 1, (IX + s), D
                                RES_XS_R(1, ix, d);
                                break;
                            case 0x8B: // RES 1, (IX + s), E
                                RES_XS_R(1, ix, e);
                                break;
                            case 0x8C: // RES 1, (IX + s), H
                                RES_XS_R(1, ix, h);
                                break;
                            case 0x8D: // RES 1, (IX + s), L
                                RES_XS_R(1, ix, l);
                                break;
                            case 0x8E: // RES 1, (IX + s)
                                RES_XS(1, ix);
                                break;
                            case 0x8F: // RES 1, (IX + s), A
                                RES_XS_R(1, ix, a);
                                break;
                            case 0x90: // RES 2, (IX + s), B
                                RES_XS_R(2, ix, b);
                                break;
                            case 0x91: // RES 2, (IX + s), C
                                RES_XS_R(2, ix, c);
                                break;
                            case 0x92: // RES 2, (IX + s), D
                                RES_XS_R(2, ix, d);
                                break;
                            case 0x93: // RES 2, (IX + s), E
                                RES_XS_R(2, ix, e);
                                break;
                            case 0x94: // RES 2, (IX + s), H
                                RES_XS_R(2, ix, h);
                                break;
                            case 0x95: // RES 2, (IX + s), L
                                RES_XS_R(2, ix, l);
                                break;
                            case 0x96: // RES 2, (IX + s)
                                RES_XS(2, ix);
                                break;
                            case 0x97: // RES 2, (IX + s), A
                                RES_XS_R(2, ix, a);
                                break;
                            case 0x98: // RES 3, (IX + s), B
                                RES_XS_R(3, ix, b);
                                break;
                            case 0x99: // RES 3, (IX + s), C
                                RES_XS_R(3, ix, c);
                                break;
                            case 0x9A: // RES 3, (IX + s), D
                                RES_XS_R(3, ix, d);
                                break;
                            case 0x9B: // RES 3, (IX + s), E
                                RES_XS_R(3, ix, e);
                                break;
                            case 0x9C: // RES 3, (IX + s), H
                                RES_XS_R(3, ix, h);
                                break;
                            case 0x9D: // RES 3, (IX + s), L
                                RES_XS_R(3, ix, l);
                                break;
                            case 0x9E: // RES 3, (IX + s)
                                RES_XS(3, ix);
                                break;
                            case 0x9F: // RES 3, (IX + s), A
                                RES_XS_R(3, ix, a);
                                break;
                            case 0xA0: // RES 4, (IX + s), B
                                RES_XS_R(4, ix, b);
                                break;
                            case 0xA1: // RES 4, (IX + s), C
                                RES_XS_R(4, ix, c);
                                break;
                            case 0xA2: // RES 4, (IX + s), D
                                RES_XS_R(4, ix, d);
                                break;
                            case 0xA3: // RES 4, (IX + s), E
                                RES_XS_R(4, ix, e);
                                break;
                            case 0xA4: // RES 4, (IX + s), H
                                RES_XS_R(4, ix, h);
                                break;
                            case 0xA5: // RES 4, (IX + s), L
                                RES_XS_R(4, ix, l);
                                break;
                            case 0xA6: // RES 4, (IX + s)
                                RES_XS(4, ix);
                                break;
                            case 0xA7: // RES 4, (IX + s), A
                                RES_XS_R(4, ix, a);
                                break;
                            case 0xA8: // RES 5, (IX + s), B
                                RES_XS_R(5, ix, b);
                                break;
                            case 0xA9: // RES 5, (IX + s), C
                                RES_XS_R(5, ix, c);
                                break;
                            case 0xAA: // RES 5, (IX + s), D
                                RES_XS_R(5, ix, d);
                                break;
                            case 0xAB: // RES 5, (IX + s), E
                                RES_XS_R(5, ix, e);
                                break;
                            case 0xAC: // RES 5, (IX + s), H
                                RES_XS_R(5, ix, h);
                                break;
                            case 0xAD: // RES 5, (IX + s), L
                                RES_XS_R(5, ix, l);
                                break;
                            case 0xAE: // RES 5, (IX + s)
                                RES_XS(5, ix);
                                break;
                            case 0xAF: // RES 5, (IX + s), A
                                RES_XS_R(5, ix, a);
                                break;
                            case 0xB0: // RES 6, (IX + s), B
                                RES_XS_R(6, ix, b);
                                break;
                            case 0xB1: // RES 6, (IX + s), C
                                RES_XS_R(6, ix, c);
                                break;
                            case 0xB2: // RES 6, (IX + s), D
                                RES_XS_R(6, ix, d);
                                break;
                            case 0xB3: // RES 6, (IX + s), E
                                RES_XS_R(6, ix, e);
                                break;
                            case 0xB4: // RES 6, (IX + s), H
                                RES_XS_R(6, ix, h);
                                break;
                            case 0xB5: // RES 6, (IX + s), L
                                RES_XS_R(6, ix, l);
                                break;
                            case 0xB6: // RES 6, (IX + s)
                                RES_XS(6, ix);
                                break;
                            case 0xB7: // RES 6, (IX + s), A
                                RES_XS_R(6, ix, a);
                                break;
                            case 0xB8: // RES 7, (IX + s), B
                                RES_XS_R(7, ix, b);
                                break;
                            case 0xB9: // RES 7, (IX + s), C
                                RES_XS_R(7, ix, c);
                                break;
                            case 0xBA: // RES 7, (IX + s), D
                                RES_XS_R(7, ix, d);
                                break;
                            case 0xBB: // RES 7, (IX + s), E
                                RES_XS_R(7, ix, e);
                                break;
                            case 0xBC: // RES 7, (IX + s), H
                                RES_XS_R(7, ix, h);
                                break;
                            case 0xBD: // RES 7, (IX + s), L
                                RES_XS_R(7, ix, l);
                                break;
                            case 0xBE: // RES 7, (IX + s)
                                RES_XS(7, ix);
                                break;
                            case 0xBF: // RES 7, (IX + s), A
                                RES_XS_R(7, ix, a);
                                break;
                            case 0xC0: // SET 0, (IX + s), B
                                SET_XS_R(0, ix, b);
                                break;
                            case 0xC1: // SET 0, (IX + s), C
                                SET_XS_R(0, ix, c);
                                break;
                            case 0xC2: // SET 0, (IX + s), D
                                SET_XS_R(0, ix, d);
                                break;
                            case 0xC3: // SET 0, (IX + s), E
                                SET_XS_R(0, ix, e);
                                break;
                            case 0xC4: // SET 0, (IX + s), H
                                SET_XS_R(0, ix, h);
                                break;
                            case 0xC5: // SET 0, (IX + s), L
                                SET_XS_R(0, ix, l);
                                break;
                            case 0xC6: // SET 0, (IX + s)
                                SET_XS(0, ix);
                                break;
                            case 0xC7: // SET 0, (IX + s), A
                                SET_XS_R(0, ix, a);
                                break;
                            case 0xC8: // SET 1, (IX + s), B
                                SET_XS_R(1, ix, b);
                                break;
                            case 0xC9: // SET 1, (IX + s), C
                                SET_XS_R(1, ix, c);
                                break;
                            case 0xCA: // SET 1, (IX + s), D
                                SET_XS_R(1, ix, d);
                                break;
                            case 0xCB: // SET 1, (IX + s), E
                                SET_XS_R(1, ix, e);
                                break;
                            case 0xCC: // SET 1, (IX + s), H
                                SET_XS_R(1, ix, h);
                                break;
                            case 0xCD: // SET 1, (IX + s), L
                                SET_XS_R(1, ix, l);
                                break;
                            case 0xCE: // SET 1, (IX + s)
                                SET_XS(1, ix);
                                break;
                            case 0xCF: // SET 1, (IX + s), A
                                SET_XS_R(1, ix, a);
                                break;
                            case 0xD0: // SET 2, (IX + s), B
                                SET_XS_R(2, ix, b);
                                break;
                            case 0xD1: // SET 2, (IX + s), C
                                SET_XS_R(2, ix, c);
                                break;
                            case 0xD2: // SET 2, (IX + s), D
                                SET_XS_R(2, ix, d);
                                break;
                            case 0xD3: // SET 2, (IX + s), E
                                SET_XS_R(2, ix, e);
                                break;
                            case 0xD4: // SET 2, (IX + s), H
                                SET_XS_R(2, ix, h);
                                break;
                            case 0xD5: // SET 2, (IX + s), L
                                SET_XS_R(2, ix, l);
                                break;
                            case 0xD6: // SET 2, (IX + s)
                                SET_XS(2, ix);
                                break;
                            case 0xD7: // SET 2, (IX + s), A
                                SET_XS_R(2, ix, a);
                                break;
                            case 0xD8: // SET 3, (IX + s), B
                                SET_XS_R(3, ix, b);
                                break;
                            case 0xD9: // SET 3, (IX + s), C
                                SET_XS_R(3, ix, c);
                                break;
                            case 0xDA: // SET 3, (IX + s), D
                                SET_XS_R(3, ix, d);
                                break;
                            case 0xDB: // SET 3, (IX + s), E
                                SET_XS_R(3, ix, e);
                                break;
                            case 0xDC: // SET 3, (IX + s), H
                                SET_XS_R(3, ix, h);
                                break;
                            case 0xDD: // SET 3, (IX + s), L
                                SET_XS_R(3, ix, l);
                                break;
                            case 0xDE: // SET 3, (IX + s)
                                SET_XS(3, ix);
                                break;
                            case 0xDF: // SET 3, (IX + s), A
                                SET_XS_R(3, ix, a);
                                break;
                            case 0xE0: // SET 4, (IX + s), B
                                SET_XS_R(4, ix, b);
                                break;
                            case 0xE1: // SET 4, (IX + s), C
                                SET_XS_R(4, ix, c);
                                break;
                            case 0xE2: // SET 4, (IX + s), D
                                SET_XS_R(4, ix, d);
                                break;
                            case 0xE3: // SET 4, (IX + s), E
                                SET_XS_R(4, ix, e);
                                break;
                            case 0xE4: // SET 4, (IX + s), H
                                SET_XS_R(4, ix, h);
                                break;
                            case 0xE5: // SET 4, (IX + s), L
                                SET_XS_R(4, ix, l);
                                break;
                            case 0xE6: // SET 4, (IX + s)
                                SET_XS(4, ix);
                                break;
                            case 0xE7: // SET 4, (IX + s), A
                                SET_XS_R(4, ix, a);
                                break;
                            case 0xE8: // SET 5, (IX + s), B
                                SET_XS_R(5, ix, b);
                                break;
                            case 0xE9: // SET 5, (IX + s), C
                                SET_XS_R(5, ix, c);
                                break;
                            case 0xEA: // SET 5, (IX + s), D
                                SET_XS_R(5, ix, d);
                                break;
                            case 0xEB: // SET 5, (IX + s), E
                                SET_XS_R(5, ix, e);
                                break;
                            case 0xEC: // SET 5, (IX + s), H
                                SET_XS_R(5, ix, h);
                                break;
                            case 0xED: // SET 5, (IX + s), L
                                SET_XS_R(5, ix, l);
                                break;
                            case 0xEE: // SET 5, (IX + s)
                                SET_XS(5, ix);
                                break;
                            case 0xEF: // SET 5, (IX + s), A
                                SET_XS_R(5, ix, a);
                                break;
                            case 0xF0: // SET 6, (IX + s), B
                                SET_XS_R(6, ix, b);
                                break;
                            case 0xF1: // SET 6, (IX + s), C
                                SET_XS_R(6, ix, c);
                                break;
                            case 0xF2: // SET 6, (IX + s), D
                                SET_XS_R(6, ix, d);
                                break;
                            case 0xF3: // SET 6, (IX + s), E
                                SET_XS_R(6, ix, e);
                                break;
                            case 0xF4: // SET 6, (IX + s), H
                                SET_XS_R(6, ix, h);
                                break;
                            case 0xF5: // SET 6, (IX + s), L
                                SET_XS_R(6, ix, l);
                                break;
                            case 0xF6: // SET 6, (IX + s)
                                SET_XS(6, ix);
                                break;
                            case 0xF7: // SET 6, (IX + s), A
                                SET_XS_R(6, ix, a);
                                break;
                            case 0xF8: // SET 7, (IX + s), B
                                SET_XS_R(7, ix, b);
                                break;
                            case 0xF9: // SET 7, (IX + s), C
                                SET_XS_R(7, ix, c);
                                break;
                            case 0xFA: // SET 7, (IX + s), D
                                SET_XS_R(7, ix, d);
                                break;
                            case 0xFB: // SET 7, (IX + s), E
                                SET_XS_R(7, ix, e);
                                break;
                            case 0xFC: // SET 7, (IX + s), H
                                SET_XS_R(7, ix, h);
                                break;
                            case 0xFD: // SET 7, (IX + s), L
                                SET_XS_R(7, ix, l);
                                break;
                            case 0xFE: // SET 7, (IX + s)
                                SET_XS(7, ix);
                                break;
                            case 0xFF: // SET 7, (IX + s), A
                                SET_XS_R(7, ix, a);
                                break;
                        }
                        break;
                    case 0xCC: // CALL Z, NN
                        CALL_CND_NN(f & ZF);
                        break;
                    case 0xCD: // CALL NN
                        CALL_NN;
                        break;
                    case 0xCE: // ADC A, N
                        ADC_XR(pc++);
                        break;
                    case 0xCF: // RST 8
                        RST(0x08);
                        break;
                    case 0xD0: // RET NC
                        RET_CND(!(f & CF));
                        break;
                    case 0xD1: // POP DE
                        POP(de);
                        break;
                    case 0xD2: // JP NC, NN
                        JP_CND_NN(!(f & CF));
                        break;
                    case 0xD3: // OUT (N), A
                        OUT_N_A;
                        break;
                    case 0xD4: // CALL NC, NN
                        CALL_CND_NN(!(f & CF));
                        break;
                    case 0xD5: // PUSH DE
                        PUSH(de);
                        break;
                    case 0xD6: // SUB A, N
                        SUB_XR(pc++);
                        break;
                    case 0xD7: // RST 10
                        RST(0x10);
                        break;
                    case 0xD8: // RET C
                        RET_CND(f & CF);
                        break;
                    case 0xD9: // EXX
                        bc ^= alt.bc;
                        alt.bc ^= bc;
                        bc ^= alt.bc;
                        de ^= alt.de;
                        alt.de ^= de;
                        de ^= alt.de;
                        hl ^= alt.hl;
                        alt.hl ^= hl;
                        hl ^= alt.hl;
                        time(4);
                        break;
                    case 0xDA: // JP C, NN
                        JP_CND_NN(f & CF);
                        break;
                    case 0xDB: // IN A, (N)
                        IN_A_N;
                        break;
                    case 0xDC: // CALL C, NN
                        CALL_CND_NN(f & CF);
                        break;
                    case 0xDD: // --------------- DD DD combination ---------------
                        irl--;
                        pc--;
                        break;
                    case 0xDE: // SBC A, N
                        SBC_XR(pc++);
                        break;
                    case 0xDF: // RST 18
                        RST(0x18);
                        break;
                    case 0xE0: // RET PO
                        RET_CND(!(f & PF));
                        break;
                    case 0xE1: // POP IX
                        POP(ix);
                        break;
                    case 0xE2: // JP PO, NN
                        JP_CND_NN(!(f & PF));
                        break;
                    case 0xE3: // EX (SP), IX
                        EX_SP_RR(ix);
                        break;
                    case 0xE4: // CALL PO, NN
                        CALL_CND_NN(!(f & PF));
                        break;
                    case 0xE5: // PUSH IX
                        PUSH(ix);
                        break;
                    case 0xE6: // AND A, N
                        AND_XR(pc++);
                        break;
                    case 0xE7: // RST 20
                        RST(0x20);
                        break;
                    case 0xE8: // RET PE
                        RET_CND(f & PF);
                        break;
                    case 0xE9: // JP IX
                        JP_RR(ix);
                        break;
                    case 0xEA: // JP PE, NN
                        JP_CND_NN(f & PF);
                        break;
                    case 0xEB: // EX DE, HL
                        EX_RR_RR(de, hl);
                        break;
                    case 0xEC: // CALL PE, NN
                        CALL_CND_NN(f & PF);
                        break;
                    case 0xED: // --------------- DD ED ---------------
                        irl--;
                        pc--;
                        break;
                    case 0xEE: // XOR A, N
                        XOR_XR(pc++);
                        break;
                    case 0xEF: // RST 28
                        RST(0x28);
                        break;
                    case 0xF0: // RET P
                        RET_CND(!(f & SF))
                        break;
                    case 0xF1: // POP AF
                        POP(af);
                        break;
                    case 0xF2: // JP P, NN
                        JP_CND_NN(!(f & SF));
                        break;
                    case 0xF3: // DI
                        iff1 = iff2 = 0x00;
                        time(4);
                        break;
                    case 0xF4: // CALL P, NN
                        CALL_CND_NN(!(f & SF));
                        break;
                    case 0xF5: // PUSH AF
                        PUSH(af);
                        break;
                    case 0xF6: // OR A, N
                        OR_XR(pc++);
                        break;
                    case 0xF7: // RST 30
                        RST(0x30);
                        break;
                    case 0xF8: // RET M
                        RET_CND(f & SF);
                        break;
                    case 0xF9: // LD SP, IX
                        LD_RR_RR(sp, ix);
                        break;
                    case 0xFA: // JP M, NN
                        JP_CND_NN(f & SF);
                        break;
                    case 0xFB: // EI
                        iff1 = iff2 = 1; // IFF1, IFF2 = true
                        time(4);
                        break;
                    case 0xFC: // CALL M, NN
                        CALL_CND_NN(f & SF);
                        break;
                    case 0xFD: // --------------- DD FD --------------
                        irl--;
                        pc--;
                        break;
                    case 0xFE: // CP N
                        CP_XR(pc++);
                        break;
                    case 0xFF: // RST 38
                        RST(0x38);
                        break;
		        }
		        break;
            // No prefix.
            case 0xDE: // SBC A, N
                SBC_XR(pc++);
                break;
            case 0xDF: // RST 18
                RST(0x18);
                break;
            case 0xE0: // RET PO
                RET_CND(!(f & PF));
                break;
            case 0xE1: // POP HL
                POP(hl);
                break;
            case 0xE2: // JP PO, NN
                JP_CND_NN(!(f & PF));
                break;
            case 0xE3: // EX (SP), HL
                EX_SP_RR(hl);
                break;
            case 0xE4: // CALL PO, NN
                CALL_CND_NN(!(f & PF));
                break;
            case 0xE5: // PUSH HL    
                PUSH(hl);
                break;
            case 0xE6: // AND A, N
                AND_XR(pc++);
                break;
            case 0xE7: // RST 20
                RST(0x20);
                break;
            case 0xE8: // RET PE
                RET_CND(f & PF);
                break;
            case 0xE9: // JP HL
                JP_RR(hl);
                break;
            case 0xEA: // JP PE, NN
                JP_CND_NN(f & PF);
                break;
            case 0xEB: // EX DE, HL
                EX_RR_RR(de, hl);
                break;
            case 0xEC: // CALL PE, NN
                CALL_CND_NN(f & PF);
                break;
            case 0xED: // --------------- ED prefix ---------------
                irl++;
                time(4);
                switch(memory->read_byte_ex(pc++)){
                    case 0x40: // IN B, (C)
                        IN_R_RR(b, bc);
                        break;
                    case 0x41: // OUT (C), B
                        OUT_RR_R(bc, b);
                        break;
                    case 0x42: // SBC HL, BC
                        SBC16(hl, bc);
                        break;
                    case 0x43: // LD (NN), BC
                        LD_MM_RR(bc);
                        break;
                    case 0x44: // NEG A
                    case 0x4C: // NEG A
                    case 0x54: // NEG A
                    case 0x5C: // NEG A
                    case 0x64: // NEG A
                    case 0x6C: // NEG A
                    case 0x74: // NEG A
                    case 0x7C: // NEG A
                        NEG_A;
                        break;
                    case 0x45: // RETN
                    case 0x4D: // RETI the same as RETN.
                    case 0x55: // RETN
                    case 0x5D: // RETN
                    case 0x65: // RETN
                    case 0x6D: // RETN
                    case 0x75: // RETN
                    case 0x7D: // RETN
                        iff1 = iff2;
                        RET;
                        break;
                    case 0x46: // IM 0
                    case 0x4E: // IM 0/1 undefined mode ???
                    case 0x66: // IM 0
                    case 0x6E: // IM 0
                    case 0x56: // IM 1 Mode 0 and mode 1 is the same.
                    case 0x76: // IM 1
                        im = 1;
                        time(4);
                        break;
                    case 0x47: // LD I, A
                        irh = a;
                        time(5);
                        break;
                    case 0x48: // IN C, (C)
                        IN_R_RR(c, bc);
                        break;
                    case 0x49: // OUT (C), C
                        OUT_RR_R(bc, c);
                        break;
                    case 0x4A: // ADC HL, BC
                        ADC16(hl, bc);
                        break;
                    case 0x4B: // LD BC, (NN)
                        LD_RR_MM(bc);
                        break;
                    case 0x4F: // LD R, A
                        irl = a & 0x7F;
                        r8bit = a & 0x80;
                        time(5);
                        break;
                    case 0x50: // IN D, (C)
                        IN_R_RR(d, bc);
                        break;
                    case 0x51: // OUT (C), D
                        OUT_RR_R(bc, d);
                        break;
                    case 0x52: // SBC HL, DE
                        SBC16(hl, de);
                        break;
                    case 0x53: // LD (NN), DE
                        LD_MM_RR(de);
                        break;
                    case 0x57: // LD A, I
                        a = irh;
                        f = (flag_sz53p[a] & (~PF)) | (f & CF);
                        // P/V contains contents of IFF2.
                        // If an s32errupt occurs during execution of this instruction, the Parity flag contains a 0.
                        if (iff2 && clk > LD_IR_PF_CLK)
                            f |= PF;
                        time(5);
                        break;
                    case 0x58: // IN E, (C)
                        IN_R_RR(e, bc);
                        break;
                    case 0x59: // OUT (C), E
                        OUT_RR_R(bc, e);
                        break;
                    case 0x5A: // ADC HL, DE 
                        ADC16(hl, de);
                        break;
                    case 0x5B: // LD DE, (NN)
                        LD_RR_MM(de);
                        break;
                    case 0x5E: // IM 2
                        im = 2;
                        time(4);
                        break;
                    case 0x5F: // LD A, R
                        a = (irl & 0x7F) | (r8bit & 0x80);
                        f = (flag_sz53p[a] & (~PF)) | (f & CF);
                        // P/V contains contents of IFF2.
                        // If an s32errupt occurs during execution of this instruction, the Parity flag contains a 0.
                        if (iff2 && clk > LD_IR_PF_CLK)
                            f |= PF;
                        time(5);
                        break;
                    case 0x60: // IN H, (C)
                        IN_R_RR(h, bc);
                        break;
                    case 0x61: // OUT (C), H
                        OUT_RR_R(bc, h);
                        break;
                    case 0x62: // SBC HL, HL
                        SBC16(hl, hl);
                        break;
                    case 0x63: // LD (NN), HL
                        LD_MM_RR(hl);
                        break;
                    case 0x67: // RRD
                        RRD;
                        break;
                    case 0x68: // IN L, (C)
                        IN_R_RR(l, bc);
                        break;
                    case 0x69: // OUT (C), L
                        OUT_RR_R(bc, l);
                        break;
                    case 0x6A: // ADC HL, HL
                        ADC16(hl, hl);
                        break;
                    case 0x6B: // LD HL, (NN)
                        LD_RR_MM(hl);
                        break;
                    case 0x6F: // RLD
                        RLD;
                        break;
                    case 0x70: // IN (C)
                        {
                            u8 byte;
                            IN_R_RR(byte, bc);
                        }
                        break;
                    case 0x71: // OUT (C), 0
                        OUT_RR_R(bc, 0);
                        break;
                    case 0x72: // SBC HL, SP
                        SBC16(hl, sp);
                        break;
                    case 0x73: // LD (NN), SP
                        LD_MM_RR(sp);
                        break;
                    case 0x78: // IN A, (C)
                        IN_R_RR(a, bc);
                        break;
                    case 0x79: // OUT (C), A
                        OUT_RR_R(bc, a);
                        break;
                    case 0x7A: // ADC HL, SP
                        ADC16(hl, sp);
                        break;
                    case 0x7B: // LD SP, (NN)
                        LD_RR_MM(sp);
                        break;
                    case 0x7E: // IM 2
                        im = 2;
                        time(4);
                        break;
                    case 0xA0: // LDI
                        LDI;
                        break;
                    case 0xA1: // CPI 
                        CPI;
                        break;
                    case 0xA2: // INI
                        INI;
                        break;
                    case 0xA3: // OUTI
                        OUTI;
                        break;
                    // 0xA4-0xA7: NOP
                    case 0xA8: // LDD
                        LDD;
                        break;
                    case 0xA9: // CPD
                        CPD;
                        break;
                    case 0xAA: // IND
                        IND;
                        break;
                    case 0xAB: // OUTD
                        OUTD;
                        break;
                    // 0xAC-0xAF: NOP
                    case 0xB0: // LDIR
                        LDIR;
                        break;
                    case 0xB1: // CPIR
                        CPIR;
                        break;
                    case 0xB2: // INIR
                        INIR;
                        break;
                    case 0xB3: // OUTIR
                        OUTIR;
                        break;
                    // 0xB4-0xB7: NOP
                    case 0xB8: // LDDR
                        LDDR;
                        break;
                    case 0xB9: // CPDR
                        CPDR;
                        break;
                    case 0xBA: // INDR
                        INDR;
                        break;
                    case 0xBB: // OUTDR
                        OUTDR;
                        break;
                    default:
                        time(4);
                        break;
                }
                break;
            // No prefix
            case 0xEE: // XOR A, N
                XOR_XR(pc++);
                break;
            case 0xEF: // RST 28
                RST(0x28);
                break;
            case 0xF0: // RET P
                RET_CND(!(f & SF));
                break;
            case 0xF1: // POP AF
                POP(af);
                break;
            case 0xF2: // JP P, NN
                JP_CND_NN(!(f & SF));
                break;
            case 0xF3: // DI
                iff1 = iff2 = 0x00;
                time(4);
                break;
            case 0xF4: // CALL P, NN
                CALL_CND_NN(!(f & SF));
                break;
            case 0xF5: // PUSH AF
                PUSH(af);
                break;
            case 0xF6: // OR A, N
                OR_XR(pc++);
                break;
            case 0xF7: // RST 30
                RST(0x30);
                break;
            case 0xF8: // RET M
                RET_CND(f & SF);
                break;
            case 0xF9: // LD SP, HL
                LD_RR_RR(sp, hl);
                break;
            case 0xFA: // JP M, NN
                JP_CND_NN(f & SF);
                break;
            case 0xFB: // EI
                iff1 = iff2 = 1; // IFF1, IFF2 = true
                time(4);
                break;
            case 0xFC: // CALL M, NN
                CALL_CND_NN(f & SF);
                break;
            case 0xFD: // --------------- FD prefix --------------
                irl++;
                time(4);
                switch(memory->read_byte_ex(pc++)){
                    case 0x00: // NOP
                        time(4);
                        break;
                    case 0x01: // LD BC, NN
                        LD_RR_NN(bc);
                        break;
                    case 0x02: // LD (BC), A
                        LD_XR_A(bc);
                        break;
                    case 0x03: // INC BC
                        INC_RR(bc);
                        break;
                    case 0x04: // INC B
                        INC(b);
                        break;
                    case 0x05: // DEC B
                        DEC(b);
                        break;
                    case 0x06: // LD B, N
                        LD_R_XR(b, pc++);
                        break;
                    case 0x07: // RLCA
                        a = (a << 1) | a >> 7;
                        f = (f & (SF | ZF | PF)) | (a & (F5 | F3 | CF));
                        time(4);
                        break;
                    case 0x08: // EX AF, AF'
                        af ^= alt.af;
                        alt.af ^= af;
                        af ^= alt.af;
                        time(4);
                        break;
                    case 0x09: // ADD IY, BC
                        ADD16(iy, bc);
                        break;
                    case 0x0A: // LD A, (BC)
                        LD_A_XR(bc);
                        break;
                    case 0x0B: // DEC BC
                        DEC_RR(bc);
                        break; 
                    case 0x0C: // INC C
                        INC(c);
                        break;
                    case 0x0D: // DEC C
                        DEC(c);
                        break;
                    case 0x0E: // LD C, N
                        LD_R_XR(c, pc++);
                        break;
                    case 0x0F: // RRCA
                        f = (f & (SF | ZF | PF)) | (a & CF);
                        a = (a >> 1) | (a << 7);
                        f |= (a & (F5 | F3));
                        time(4);
                        break;
                    case 0x10: // DJNZ N
                        if (--b){
                            pc += (s8)memory->read_byte(pc) + 1;
                            memptr = pc;
                            time(13);
                        }else{
                            pc++;
                            time(8);
                        }
                        break;
                    case 0x11: // LD DE, NN
                        LD_RR_NN(de);
                        break;
                    case 0x12: // LD (DE), A
                        LD_XR_A(de);
                        break;
                    case 0x13: // INC DE
                        INC_RR(de);
                        break;
                    case 0x14: // INC D
                        INC(d);
                        break;
                    case 0x15: // DEC D
                        DEC(d);
                        break;
                    case 0x16: // LD D, N
                        LD_R_XR(d, pc++);
                        break;
                    case 0x17: // RLA
                        RLA;
                        break;
                    case 0x18: // JR N
                        JR_N;
                        break;
                    case 0x19: // ADD IY, DE
                        ADD16(iy, de);
                        break;
                    case 0x1A: // LD A, (DE)
                        LD_A_XR(de);
                        break;
                    case 0x1B: // DEC DE
                        DEC_RR(de);
                        break;
                    case 0x1C: // INC E
                        INC(e);
                        break;
                    case 0x1D: // DEC E
                        DEC(e);
                        break; 
                    case 0x1E: // LD E, N
                        LD_R_XR(e, pc++);
                        break;
                    case 0x1F: // RRA
                        RRA;
                        break;
                    case 0x20: // JR NZ, N
                        JR_CND_N(!(f & ZF));
                        break;
                    case 0x21: // LD IY, NN
                        LD_RR_NN(iy);
                        break;
                    case 0x22: // LD (NN), IY
                        LD_MM_RR(iy);
                        break;
                    case 0x23: // INC IY
                        INC_RR(iy);
                        break;
                    case 0x24: // INC IYH
                        INC(iyh);
                        break;
                    case 0x25: // DEC IYH
                        DEC(iyh);
                        break;
                    case 0x26: // LD IYH, N
                        LD_R_XR(iyh, pc++);
                        break;
                    case 0x27: // DAA
                        DAA;
                        break;
                    case 0x28: // JR Z, N
                        JR_CND_N(f & ZF);
                        break;
                    case 0x29: // ADD IY, IY
                        ADD16(iy, iy);
                        break;
                    case 0x2A: // LD IY, (NN)
                        LD_RR_MM(iy);
                        break;
                    case 0x2B: // DEC IY
                        DEC_RR(iy);
                        break;
                    case 0x2C: // INC IYL
                        INC(iyl);
                        break;
                    case 0x2D: // DEC IYL
                        DEC(iyl);
                        break;
                    case 0x2E: // LD IYL, N
                        LD_R_XR(iyl, pc++);
                        break;
                    case 0x2F: // CPL
                        a ^= 0xFF;
                        f = (f & (SF | ZF | PF | CF)) | HF | NF | (a & (F3 | F5));
                        time(4);
                        break;
                    case 0x30: // JR NC, N
                        JR_CND_N(!(f & CF))
                        break;
                    case 0x31: // LD SP, NN
                        LD_RR_NN(sp);
                        break;
                    case 0x32: // LD (NN), A
                        LD_MM_A;
                        break;
                    case 0x33: // INC SP
                        INC_RR(sp);
                        break;
                    case 0x34: // INC (IY + s)
                        INC_XS(iy);
                        break;
                    case 0x35: // DEC (IY + s)
                        DEC_XS(iy);
                        break;
                    case 0x36: // LD (IY + s), N
                        LD_XS_N(iy)
                        break;
                    case 0x37: // SCF
                        f = (f & (SF | ZF | PF)) | CF | (a & (F3 | F5));
                        time(4);
                        break;
                    case 0x38: // JR C, N
                        JR_CND_N(f & CF);
                        break;
                    case 0x39: // ADD IY, SP
                        ADD16(iy, sp);
                        break;
                    case 0x3A: // LD A, (NN)
                        LD_A_MM;
                        break;
                    case 0x3B: // DEC SP
                        DEC_RR(sp);
                        break;
                    case 0x3C: // INC A
                        INC(a);
                        break;
                    case 0x3D: // DEC A
                        DEC(a);
                        break;
                    case 0x3E: // LD A, N
                        LD_R_XR(a, pc++);
                        break;
                    case 0x3F: // CCF
                        f = ((f & ~(NF | HF)) | ((f << 4) & HF) | (a & (F3 | F5))) ^ CF;
                        time(4);
                        break;
                    case 0x40: // LD B, B
                        time(4);
                        break;
                    case 0x41: // LD B, C
                        LD_R_R(b, c);
                        break;
                    case 0x42: // LD B, D
                        LD_R_R(b, d);
                        break;
                    case 0x43: // LD B, E
                        LD_R_R(b, e);
                        break;
                    case 0x44: // LD B, IYH
                        LD_R_R(b, iyh);
                        break;
                    case 0x45: // LD B, IYL
                        LD_R_R(b, iyl);
                        break;
                    case 0x46: // LD B, (IY + s)
                        LD_R_XS(b, iy);
                        break;
                    case 0x47: // LD B, A
                        LD_R_R(b, a);
                        break;
                    case 0x48: // LD C, B
                        LD_R_R(c, b);
                        break;
                    case 0x49: // LD C, C
                        time(4);
                        break;
                    case 0x4A: // LD C, D
                        LD_R_R(c, d);
                        break;
                    case 0x4B: // LD C, E
                        LD_R_R(c, e);
                        break;
                    case 0x4C: // LD C, H
                        LD_R_R(c, iyh);
                        break;
                    case 0x4D: // LD C, IYL
                        LD_R_R(c, iyl);
                        break;
                    case 0x4E: // LD C, (IY + s)
                        LD_R_XS(c, iy);
                        break;
                    case 0x4F: // LD C, A
                        LD_R_R(c, a);
                        break;
                    case 0x50: // LD D, B
                        LD_R_R(d, b);
                        break;
                    case 0x51: // LD D, C
                        LD_R_R(d, c);
                        break;
                    case 0x52: // LD D, D
                        time(4);
                        break;
                    case 0x53: // LD D, E
                        LD_R_R(d, e);
                        break;
                    case 0x54: // LD D, IYH
                        LD_R_R(d, iyh);
                        break;
                    case 0x55: // LD D, IYL
                        LD_R_R(d, iyl);
                        break;
                    case 0x56: // LD D, (IY + s)
                        LD_R_XS(d, iy);
                        break;
                    case 0x57: // LD D, A
                        LD_R_R(d, a);
                        break;
                    case 0x58: // LD E, B
                        LD_R_R(e, b);
                        break;
                    case 0x59: // LD E, C
                        LD_R_R(e, c);
                        break;
                    case 0x5A: // LD E, D
                        LD_R_R(e, d);
                        break;
                    case 0x5B: // LD E, E
                        time(4);
                        break;
                    case 0x5C: // LD E, IYH
                        LD_R_R(e, iyh);
                        break;
                    case 0x5D: // LD E, IYL
                        LD_R_R(e, iyl);
                        break;
                    case 0x5E: // LD E, (IY + s)
                        LD_R_XS(e, iy);
                        break;
                    case 0x5F: // LD E, A
                        LD_R_R(e, a);
                        break;
                    case 0x60: // LD IYH, B
                        LD_R_R(iyh, b);
                        break;
                    case 0x61: // LD IYH, C
                        LD_R_R(iyh, c);
                        break;
                    case 0x62: // LD IYH, D
                        LD_R_R(iyh, d);
                        break;
                    case 0x63: // LD IYH, E
                        LD_R_R(iyh, e);
                        break;
                    case 0x64: // LD IYH, IYH
                        time(4);
                        break;
                    case 0x65: // LD IYH, IYL
                        LD_R_R(iyh, iyl);
                        break;
                    case 0x66: // LD H, (IY + s)
                        LD_R_XS(h, iy);
                        break;
                    case 0x67: // LD IYH, A
                        LD_R_R(iyh, a);
                        break;
                    case 0x68: // LD IYL, B
                        LD_R_R(iyl, b);
                        break;
                    case 0x69: // LD IYL, C
                        LD_R_R(iyl, c);
                        break;
                    case 0x6A: // LD IYL, D
                        LD_R_R(iyl, d);
                        break;
                    case 0x6B: // LD IYL, E
                        LD_R_R(iyl, e);
                        break;
                    case 0x6C: // LD IYL, IYH
                        LD_R_R(iyl, iyh);
                        break;
                    case 0x6D: // LD IYL, IYL
                        time(4);
                        break;
                    case 0x6E: // LD L, (IY + s);
                        LD_R_XS(l, iy);
                        break;
                    case 0x6F: // LD IYL, A
                        LD_R_R(iyl, a);
                        break;
                    case 0x70: // LD (IY + s), B
                        LD_XS_R(iy, b);
                        break;
                    case 0x71: // LD (IY + s), C
                        LD_XS_R(iy, c);
                        break;
                    case 0x72: // LD (IY + s), D
                        LD_XS_R(iy, d);
                        break;
                    case 0x73: // LD (IY + s), E
                        LD_XS_R(iy, e);
                        break;
                    case 0x74: // LD (IY + s), H
                        LD_XS_R(iy, h);
                        break;
                    case 0x75: // LD (IY + s), L
                        LD_XS_R(iy, l);
                        break;
                    case 0x76: // HALT
                        pc--; // Next HALT without prefix.
                        time(4);
                        break;
                    case 0x77: // LD (IY + s), A
                        LD_XS_R(iy, a);
                        break;
                    case 0x78: // LD A, B
                        LD_R_R(a, b);
                        break;
                    case 0x79: // LD A, C
                        LD_R_R(a, c);
                        break;
                    case 0x7A: // LD A, D
                        LD_R_R(a, d);
                        break;
                    case 0x7B: // LD A, E
                        LD_R_R(a, e);
                        break;
                    case 0x7C: // LD A, IYH
                        LD_R_R(a, iyh);
                        break;
                    case 0x7D: // LD A, IYL
                        LD_R_R(a, iyl);
                        break;
                    case 0x7E: // LD A, (IY + s)
                        LD_R_XS(a, iy);
                        break;
                    case 0x7F: // LD A, A
                        time(4);
                        break;
                    case 0x80: // ADD A, B
                        ADD(b);
                        break;
                    case 0x81: // ADD A, C
                        ADD(c);
                        break;
                    case 0x82: // ADD A, D
                        ADD(d);
                        break;
                    case 0x83: // ADD A, E
                        ADD(e);
                        break;
                    case 0x84: // ADD A, IYH
                        ADD(iyh);
                        break;
                    case 0x85: // ADD A, IYL
                        ADD(iyl);
                        break;
                    case 0x86: // ADD A, (IY + s)
                        ADD_XS(iy);
                        break;
                    case 0x87: // ADD A, A
                        ADD(a);
                        break;
                    case 0x88: // ADC A, B
                        ADC(b);
                        break;
                    case 0x89: // ADC A, C
                        ADC(c);
                        break;
                    case 0x8A: // ADC A, D
                        ADC(d);
                        break;
                    case 0x8B: // ADC A, E
                        ADC(e);
                        break;
                    case 0x8C: // ADC A, IYH
                        ADC(iyh);
                        break;
                    case 0x8D: // ADC A, IYL
                        ADC(iyl);
                        break;
                    case 0x8E: // ADC A, (IY + s)
                        ADC_XS(iy);
                        break;
                    case 0x8F: // ADC A, A
                        ADC(a);
                        break;
                    case 0x90: // SUB A, B
                        SUB(b);
                        break;
                    case 0x91: // SUB A, C
                        SUB(c);
                        break;
                    case 0x92: // SUB A, D
                        SUB(d);
                        break;
                    case 0x93: // SUB A, E
                        SUB(e);
                        break;
                    case 0x94: // SUB A, IYH
                        SUB(iyh);
                        break;
                    case 0x95: // SUB A, IYL
                        SUB(iyl);
                        break;
                    case 0x96: // SUB A, (IY + s)
                        SUB_XS(iy);
                        break;
                    case 0x97: // SUB A, A
                        SUB(a);
                        break;
                    case 0x98: // SBC A, B
                        SBC(b);
                        break;
                    case 0x99: // SBC A, C
                        SBC(c);
                        break;
                    case 0x9A: // SBC A, D
                        SBC(d);
                        break;
                    case 0x9B: // SBC A, E
                        SBC(e);
                        break;
                    case 0x9C: // SBC A, IYH
                        SBC(iyh);
                        break;
                    case 0x9D: // SBC A, IYL
                        SBC(iyl);
                        break;
                    case 0x9E: // SBC A, (IY + s)
                        SBC_XS(iy);
                        break;
                    case 0x9F: // SBC A, A
                        SBC(a);
                        break;
                    case 0xA0: // AND B
                        AND(b);
                        break;
                    case 0xA1: // AND C
                        AND(c);
                        break;
                    case 0xA2: // AND D
                        AND(d);
                        break;
                    case 0xA3: // AND E
                        AND(e);
                        break;
                    case 0xA4: // AND IYH
                        AND(iyh);
                        break;
                    case 0xA5: // AND IYL
                        AND(iyl);
                        break;
                    case 0xA6: // AND (IY + s)
                        AND_XS(iy);
                        break;
                    case 0xA7: // AND A
                        AND(a);
                        break;
                    case 0xA8: // XOR B
                        XOR(b);
                        break;
                    case 0xA9: // XOR C
                        XOR(c);
                        break;
                    case 0xAA: // XOR D
                        XOR(d);
                        break;
                    case 0xAB: // XOR E
                        XOR(e);
                        break;
                    case 0xAC: // XOR IYH
                        XOR(iyh);
                        break;
                    case 0xAD: // XOR IYL
                        XOR(iyl);
                        break;
                    case 0xAE: // XOR (IY + s)
                        XOR_XS(iy);
                        break;
                    case 0xAF: // XOR A
                        XOR(a);
                        break;
                    case 0xB0: // OR B
                        OR(b);
                        break;
                    case 0xB1: // OR C
                        OR(c);
                        break;
                    case 0xB2: // OR D
                        OR(d);
                        break;
                    case 0xB3: // OR E
                        OR(e);
                        break;
                    case 0xB4: // OR IYH
                        OR(iyh);
                        break;
                    case 0xB5: // OR IYL
                        OR(iyl);
                        break;
                    case 0xB6: // OR (IY + s)
                        OR_XS(iy);
                        break;
                    case 0xB7: // OR A
                        OR(a);
                        break;
                    case 0xB8: // CP B
                        CP(b);
                        break;
                    case 0xB9: // CP C
                        CP(c);
                        break;
                    case 0xBA: // CP D
                        CP(d);
                        break;
                    case 0xBB: // CP E
                        CP(e);
                        break;
                    case 0xBC: // CP IYH
                        CP(iyh);
                        break;
                    case 0xBD: // CP IYL
                        CP(iyl);
                        break;
                    case 0xBE: // CP (IY + s)
                        CP_XS(iy);
                        break;
                    case 0xBF: // CP A
                        CP(a);
                        break;
                    case 0xC0: // RET NZ
                        RET_CND(!(f & ZF));
                        break;
                    case 0xC1: // POP BC
                        POP(bc);
                        break;
                    case 0xC2: // JP NZ, NN
                        JP_CND_NN(!(f & ZF));
                        break;
                    case 0xC3: // JP NN
                        JP_NN;
                        break;
                    case 0xC4: // CALL NZ, NN
                        CALL_CND_NN(!(f & ZF));
                        break;
                    case 0xC5: // PUSH BC
                        PUSH(bc);
                        break;
                    case 0xC6: // ADD A, N
                        ADD_XR(pc++);
                        break;
                    case 0xC7: // RST 0x00
                        RST(0x00);
                        break;
                    case 0xC8: // RET Z
                        RET_CND(f & ZF);
                        break;
                    case 0xC9: // RET
                        RET;
                        break;
                    case 0xCA: // JP Z, NN
                        JP_CND_NN(f & ZF);
                        break;
                    case 0xCB: // --------------- FDCB --------------
                        pc++;
                        switch(memory->read_byte_ex(pc++)){
                            case 0x00: // RLC (IY + s), B
                                RLC_XS_R(iy, b);
                                break;
                            case 0x01: // RLC (IY + s), C
                                RLC_XS_R(iy, c);
                                break;
                            case 0x02: // RLC (IY + s), D
                                RLC_XS_R(iy, d);
                                break;
                            case 0x03: // RLC (IY + s), E
                                RLC_XS_R(iy, e);
                                break;
                            case 0x04: // RLC (IY + s), H
                                RLC_XS_R(iy, h);
                                break;
                            case 0x05: // RLC (IY + s), L
                                RLC_XS_R(iy, l);
                                break;
                            case 0x06: // RLC (IY + s)
                                RLC_XS(iy);
                                break;
                            case 0x07: // RLC (IY + s), A
                                RLC_XS_R(iy, a);
                                break;
                            case 0x08: // RRC (IY + s), B
                                RRC_XS_R(iy, b);
                                break;
                            case 0x09: // RRC (IY + s), C
                                RRC_XS_R(iy, c);
                                break;
                            case 0x0A: // RRC (IY + s), D
                                RRC_XS_R(iy, d);
                                break;
                            case 0x0B: // RRC (IY + s), E
                                RRC_XS_R(iy, e);
                                break;
                            case 0x0C: // RRC (IY + s), H
                                RRC_XS_R(iy, h);
                                break;
                            case 0x0D: // RRC (IY + s), L
                                RRC_XS_R(iy, l);
                                break;
                            case 0x0E: // RRC (IY + s)
                                RRC_XS(iy);
                                break;
                            case 0x0F: // RRC (IY + s), A
                                RRC_XS_R(iy, a);
                                break;
                            case 0x10: // RL (IY + s), B
                                RL_XS_R(iy, b);
                                break;
                            case 0x11: // RL (IY + s), C
                                RL_XS_R(iy, c);
                                break;
                            case 0x12: // RL (IY + s), D
                                RL_XS_R(iy, d);
                                break;
                            case 0x13: // RL (IY + s), E
                                RL_XS_R(iy, e);
                                break;
                            case 0x14: // RL (IY + s), H
                                RL_XS_R(iy, h);
                                break;
                            case 0x15: // RL (IY + s), L
                                RL_XS_R(iy, l);
                                break;
                            case 0x16: // RL (IY + s)
                                RL_XS(iy);
                                break;
                            case 0x17: // RL (IY + s), A
                                RL_XS_R(iy, a);
                                break;
                            case 0x18: // RR (IY + s), B
                                RR_XS_R(iy, b);
                                break;
                            case 0x19: // RR (IY + s), C
                                RR_XS_R(iy, c);
                                break;
                            case 0x1A: // RR (IY + s), D
                                RR_XS_R(iy, d);
                                break;
                            case 0x1B: // RR (IY + s), E
                                RR_XS_R(iy, e);
                                break;
                            case 0x1C: // RR (IY + s), H
                                RR_XS_R(iy, h);
                                break;
                            case 0x1D: // RR (IY + s), L
                                RR_XS_R(iy, l);
                                break;
                            case 0x1E: // RR (IY + s)
                                RR_XS(iy);
                                break;
                            case 0x1F: // RR (IY + s), A
                                RR_XS_R(iy, a);
                                break;
                            case 0x20: // SLA (IY + s), B
                                SLA_XS_R(iy, b);
                                break;
                            case 0x21: // SLA (IY + s), C
                                SLA_XS_R(iy, c);
                                break;
                            case 0x22: // SLA (IY + s), D
                                SLA_XS_R(iy, d);
                                break;
                            case 0x23: // SLA (IY + s), E
                                SLA_XS_R(iy, e);
                                break;
                            case 0x24: // SLA (IY + s), H
                                SLA_XS_R(iy, h);
                                break;
                            case 0x25: // SLA (IY + s), L
                                SLA_XS_R(iy, l);
                                break;
                            case 0x26: // SLA (IY + s)
                                SLA_XS(iy);
                                break;
                            case 0x27: // SLA (IY + s), A
                                SLA_XS_R(iy, a);
                                break;
                            case 0x28: // SRA (IY + s), B
                                SRA_XS_R(iy, b);
                                break;
                            case 0x29: // SRA (IY + s), C
                                SRA_XS_R(iy, c);
                                break;
                            case 0x2A: // SRA (IY + s), D
                                SRA_XS_R(iy, d);
                                break;
                            case 0x2B: // SRA (IY + s), E
                                SRA_XS_R(iy, e);
                                break;
                            case 0x2C: // SRA (IY + s), H
                                SRA_XS_R(iy, h);
                                break;
                            case 0x2D: // SRA (IY + s), L
                                SRA_XS_R(iy, l);
                                break;
                            case 0x2E: // SRA (IY + s)
                                SRA_XS(iy);
                                break;
                            case 0x2F: // SRA (IY + s), A
                                SRA_XS_R(iy, a);
                                break;
                            case 0x30: // SLL (IY + s), B
                                SLL_XS_R(iy, b);
                                break;
                            case 0x31: // SLL (IY + s), C
                                SLL_XS_R(iy, c);
                                break;
                            case 0x32: // SLL (IY + s), D
                                SLL_XS_R(iy, d);
                                break;
                            case 0x33: // SLL (IY + s), E
                                SLL_XS_R(iy, e);
                                break;
                            case 0x34: // SLL (IY + s), H
                                SLL_XS_R(iy, h);
                                break;
                            case 0x35: // SLL (IY + s), L
                                SLL_XS_R(iy, l);
                                break;
                            case 0x36: // SLL (IY + s)
                                SLL_XS(iy);
                                break;
                            case 0x37: // SLL (IY + s), A
                                SLL_XS_R(iy, a);
                                break;
                            case 0x38: // SRL (IY + s), B
                                SRL_XS_R(iy, b);
                                break;
                            case 0x39: // SRL (IY + s), C
                                SRL_XS_R(iy, c);
                                break;
                            case 0x3A: // SRL (IY + s), D
                                SRL_XS_R(iy, d);
                                break;
                            case 0x3B: // SRL (IY + s), E
                                SRL_XS_R(iy, e);
                                break;
                            case 0x3C: // SRL (IY + s), H
                                SRL_XS_R(iy, h);
                                break;
                            case 0x3D: // SRL (IY + s), L
                                SRL_XS_R(iy, l);
                                break;
                            case 0x3E: // SRL (IY + s)
                                SRL_XS(iy);
                                break;
                            case 0x3F: // SRL (IY + s), A
                                SRL_XS_R(iy, a);
                                break;
                            case 0x40: // BIT 0, (IY + s)
                            case 0x41:
                            case 0x42:
                            case 0x43:
                            case 0x44:
                            case 0x45:
                            case 0x46:
                            case 0x47:
                                BIT_XS(0, iy);
                                break;
                            case 0x48: // BIT 1, (IY + s)
                            case 0x49:
                            case 0x4A:
                            case 0x4B:
                            case 0x4C:
                            case 0x4D:
                            case 0x4E:
                            case 0x4F:
                                BIT_XS(1, iy);
                                break;
                            case 0x50: // BIT 2, (IY + s)
                            case 0x51:
                            case 0x52:
                            case 0x53:
                            case 0x54:
                            case 0x55:
                            case 0x56:
                            case 0x57:
                                BIT_XS(2, iy);
                                break;
                            case 0x58: // BIT 3, (IY + s)
                            case 0x59:
                            case 0x5A:
                            case 0x5B:
                            case 0x5C:
                            case 0x5D:
                            case 0x5E:
                            case 0x5F:
                                BIT_XS(3, iy);
                                break;
                            case 0x60: // BIT 4, (IY + s)
                            case 0x61:
                            case 0x62:
                            case 0x63:
                            case 0x64:
                            case 0x65:
                            case 0x66:
                            case 0x67:
                                BIT_XS(4, iy);
                                break;
                            case 0x68: // BIT 5, (IY + s)
                            case 0x69:
                            case 0x6A:
                            case 0x6B:
                            case 0x6C:
                            case 0x6D:
                            case 0x6E:
                            case 0x6F:
                                BIT_XS(5, iy);
                                break;
                            case 0x70: // BIT 6, (IY + s)
                            case 0x71:
                            case 0x72:
                            case 0x73:
                            case 0x74:
                            case 0x75:
                            case 0x76:
                            case 0x77:
                                BIT_XS(6, iy);
                                break;
                            case 0x78: // BIT 7, (IY + s)
                            case 0x79:
                            case 0x7A:
                            case 0x7B:
                            case 0x7C:
                            case 0x7D:
                            case 0x7E:
                            case 0x7F:
                                BIT_XS(7, iy);
                                break;
                            case 0x80: // RES 0, (IY + s), B
                                RES_XS_R(0, iy, b);
                                break;
                            case 0x81: // RES 0, (IY + s), C
                                RES_XS_R(0, iy, c);
                                break;
                            case 0x82: // RES 0, (IY + s), D
                                RES_XS_R(0, iy, d);
                                break;
                            case 0x83: // RES 0, (IY + s), E
                                RES_XS_R(0, iy, e);
                                break;
                            case 0x84: // RES 0, (IY + s), H
                                RES_XS_R(0, iy, h);
                                break;
                            case 0x85: // RES 0, (IY + s), L
                                RES_XS_R(0, iy, l);
                                break;
                            case 0x86: // RES 0, (IY + s)
                                RES_XS(0, iy);
                                break;
                            case 0x87: // RES 0, (IY + s), A
                                RES_XS_R(0, iy, a);
                                break;
                            case 0x88: // RES 1, (IY + s), B
                                RES_XS_R(1, iy, b);
                                break;
                            case 0x89: // RES 1, (IY + s), C
                                RES_XS_R(1, iy, c);
                                break;
                            case 0x8A: // RES 1, (IY + s), D
                                RES_XS_R(1, iy, d);
                                break;
                            case 0x8B: // RES 1, (IY + s), E
                                RES_XS_R(1, iy, e);
                                break;
                            case 0x8C: // RES 1, (IY + s), H
                                RES_XS_R(1, iy, h);
                                break;
                            case 0x8D: // RES 1, (IY + s), L
                                RES_XS_R(1, iy, l);
                                break;
                            case 0x8E: // RES 1, (IY + s)
                                RES_XS(1, iy);
                                break;
                            case 0x8F: // RES 1, (IY + s), A
                                RES_XS_R(1, iy, a);
                                break;
                            case 0x90: // RES 2, (IY + s), B
                                RES_XS_R(2, iy, b);
                                break;
                            case 0x91: // RES 2, (IY + s), C
                                RES_XS_R(2, iy, c);
                                break;
                            case 0x92: // RES 2, (IY + s), D
                                RES_XS_R(2, iy, d);
                                break;
                            case 0x93: // RES 2, (IY + s), E
                                RES_XS_R(2, iy, e);
                                break;
                            case 0x94: // RES 2, (IY + s), H
                                RES_XS_R(2, iy, h);
                                break;
                            case 0x95: // RES 2, (IY + s), L
                                RES_XS_R(2, iy, l);
                                break;
                            case 0x96: // RES 2, (IY + s)
                                RES_XS(2, iy);
                                break;
                            case 0x97: // RES 2, (IY + s), A
                                RES_XS_R(2, iy, a);
                                break;
                            case 0x98: // RES 3, (IY + s), B
                                RES_XS_R(3, iy, b);
                                break;
                            case 0x99: // RES 3, (IY + s), C
                                RES_XS_R(3, iy, c);
                                break;
                            case 0x9A: // RES 3, (IY + s), D
                                RES_XS_R(3, iy, d);
                                break;
                            case 0x9B: // RES 3, (IY + s), E
                                RES_XS_R(3, iy, e);
                                break;
                            case 0x9C: // RES 3, (IY + s), H
                                RES_XS_R(3, iy, h);
                                break;
                            case 0x9D: // RES 3, (IY + s), L
                                RES_XS_R(3, iy, l);
                                break;
                            case 0x9E: // RES 3, (IY + s)
                                RES_XS(3, iy);
                                break;
                            case 0x9F: // RES 3, (IY + s), A
                                RES_XS_R(3, iy, a);
                                break;
                            case 0xA0: // RES 4, (IY + s), B
                                RES_XS_R(4, iy, b);
                                break;
                            case 0xA1: // RES 4, (IY + s), C
                                RES_XS_R(4, iy, c);
                                break;
                            case 0xA2: // RES 4, (IY + s), D
                                RES_XS_R(4, iy, d);
                                break;
                            case 0xA3: // RES 4, (IY + s), E
                                RES_XS_R(4, iy, e);
                                break;
                            case 0xA4: // RES 4, (IY + s), H
                                RES_XS_R(4, iy, h);
                                break;
                            case 0xA5: // RES 4, (IY + s), L
                                RES_XS_R(4, iy, l);
                                break;
                            case 0xA6: // RES 4, (IY + s)
                                RES_XS(4, iy);
                                break;
                            case 0xA7: // RES 4, (IY + s), A
                                RES_XS_R(4, iy, a);
                                break;
                            case 0xA8: // RES 5, (IY + s), B
                                RES_XS_R(5, iy, b);
                                break;
                            case 0xA9: // RES 5, (IY + s), C
                                RES_XS_R(5, iy, c);
                                break;
                            case 0xAA: // RES 5, (IY + s), D
                                RES_XS_R(5, iy, d);
                                break;
                            case 0xAB: // RES 5, (IY + s), E
                                RES_XS_R(5, iy, e);
                                break;
                            case 0xAC: // RES 5, (IY + s), H
                                RES_XS_R(5, iy, h);
                                break;
                            case 0xAD: // RES 5, (IY + s), L
                                RES_XS_R(5, iy, l);
                                break;
                            case 0xAE: // RES 5, (IY + s)
                                RES_XS(5, iy);
                                break;
                            case 0xAF: // RES 5, (IY + s), A
                                RES_XS_R(5, iy, a);
                                break;
                            case 0xB0: // RES 6, (IY + s), B
                                RES_XS_R(6, iy, b);
                                break;
                            case 0xB1: // RES 6, (IY + s), C
                                RES_XS_R(6, iy, c);
                                break;
                            case 0xB2: // RES 6, (IY + s), D
                                RES_XS_R(6, iy, d);
                                break;
                            case 0xB3: // RES 6, (IY + s), E
                                RES_XS_R(6, iy, e);
                                break;
                            case 0xB4: // RES 6, (IY + s), H
                                RES_XS_R(6, iy, h);
                                break;
                            case 0xB5: // RES 6, (IY + s), L
                                RES_XS_R(6, iy, l);
                                break;
                            case 0xB6: // RES 6, (IY + s)
                                RES_XS(6, iy);
                                break;
                            case 0xB7: // RES 6, (IY + s), A
                                RES_XS_R(6, iy, a);
                                break;
                            case 0xB8: // RES 7, (IY + s), B
                                RES_XS_R(7, iy, b);
                                break;
                            case 0xB9: // RES 7, (IY + s), C
                                RES_XS_R(7, iy, c);
                                break;
                            case 0xBA: // RES 7, (IY + s), D
                                RES_XS_R(7, iy, d);
                                break;
                            case 0xBB: // RES 7, (IY + s), E
                                RES_XS_R(7, iy, e);
                                break;
                            case 0xBC: // RES 7, (IY + s), H
                                RES_XS_R(7, iy, h);
                                break;
                            case 0xBD: // RES 7, (IY + s), L
                                RES_XS_R(7, iy, l);
                                break;
                            case 0xBE: // RES 7, (IY + s)
                                RES_XS(7, iy);
                                break;
                            case 0xBF: // RES 7, (IY + s), A
                                RES_XS_R(7, iy, a);
                                break;
                            case 0xC0: // SET 0, (IY + s), B
                                SET_XS_R(0, iy, b);
                                break;
                            case 0xC1: // SET 0, (IY + s), C
                                SET_XS_R(0, iy, c);
                                break;
                            case 0xC2: // SET 0, (IY + s), D
                                SET_XS_R(0, iy, d);
                                break;
                            case 0xC3: // SET 0, (IY + s), E
                                SET_XS_R(0, iy, e);
                                break;
                            case 0xC4: // SET 0, (IY + s), H
                                SET_XS_R(0, iy, h);
                                break;
                            case 0xC5: // SET 0, (IY + s), L
                                SET_XS_R(0, iy, l);
                                break;
                            case 0xC6: // SET 0, (IY + s)
                                SET_XS(0, iy);
                                break;
                            case 0xC7: // SET 0, (IY + s), A
                                SET_XS_R(0, iy, a);
                                break;
                            case 0xC8: // SET 1, (IY + s), B
                                SET_XS_R(1, iy, b);
                                break;
                            case 0xC9: // SET 1, (IY + s), C
                                SET_XS_R(1, iy, c);
                                break;
                            case 0xCA: // SET 1, (IY + s), D
                                SET_XS_R(1, iy, d);
                                break;
                            case 0xCB: // SET 1, (IY + s), E
                                SET_XS_R(1, iy, e);
                                break;
                            case 0xCC: // SET 1, (IY + s), H
                                SET_XS_R(1, iy, h);
                                break;
                            case 0xCD: // SET 1, (IY + s), L
                                SET_XS_R(1, iy, l);
                                break;
                            case 0xCE: // SET 1, (IY + s)
                                SET_XS(1, iy);
                                break;
                            case 0xCF: // SET 1, (IY + s), A
                                SET_XS_R(1, iy, a);
                                break;
                            case 0xD0: // SET 2, (IY + s), B
                                SET_XS_R(2, iy, b);
                                break;
                            case 0xD1: // SET 2, (IY + s), C
                                SET_XS_R(2, iy, c);
                                break;
                            case 0xD2: // SET 2, (IY + s), D
                                SET_XS_R(2, iy, d);
                                break;
                            case 0xD3: // SET 2, (IY + s), E
                                SET_XS_R(2, iy, e);
                                break;
                            case 0xD4: // SET 2, (IY + s), H
                                SET_XS_R(2, iy, h);
                                break;
                            case 0xD5: // SET 2, (IY + s), L
                                SET_XS_R(2, iy, l);
                                break;
                            case 0xD6: // SET 2, (IY + s)
                                SET_XS(2, iy);
                                break;
                            case 0xD7: // SET 2, (IY + s), A
                                SET_XS_R(2, iy, a);
                                break;
                            case 0xD8: // SET 3, (IY + s), B
                                SET_XS_R(3, iy, b);
                                break;
                            case 0xD9: // SET 3, (IY + s), C
                                SET_XS_R(3, iy, c);
                                break;
                            case 0xDA: // SET 3, (IY + s), D
                                SET_XS_R(3, iy, d);
                                break;
                            case 0xDB: // SET 3, (IY + s), E
                                SET_XS_R(3, iy, e);
                                break;
                            case 0xDC: // SET 3, (IY + s), H
                                SET_XS_R(3, iy, h);
                                break;
                            case 0xDD: // SET 3, (IY + s), L
                                SET_XS_R(3, iy, l);
                                break;
                            case 0xDE: // SET 3, (IY + s)
                                SET_XS(3, iy);
                                break;
                            case 0xDF: // SET 3, (IY + s), A
                                SET_XS_R(3, iy, a);
                                break;
                            case 0xE0: // SET 4, (IY + s), B
                                SET_XS_R(4, iy, b);
                                break;
                            case 0xE1: // SET 4, (IY + s), C
                                SET_XS_R(4, iy, c);
                                break;
                            case 0xE2: // SET 4, (IY + s), D
                                SET_XS_R(4, iy, d);
                                break;
                            case 0xE3: // SET 4, (IY + s), E
                                SET_XS_R(4, iy, e);
                                break;
                            case 0xE4: // SET 4, (IY + s), H
                                SET_XS_R(4, iy, h);
                                break;
                            case 0xE5: // SET 4, (IY + s), L
                                SET_XS_R(4, iy, l);
                                break;
                            case 0xE6: // SET 4, (IY + s)
                                SET_XS(4, iy);
                                break;
                            case 0xE7: // SET 4, (IY + s), A
                                SET_XS_R(4, iy, a);
                                break;
                            case 0xE8: // SET 5, (IY + s), B
                                SET_XS_R(5, iy, b);
                                break;
                            case 0xE9: // SET 5, (IY + s), C
                                SET_XS_R(5, iy, c);
                                break;
                            case 0xEA: // SET 5, (IY + s), D
                                SET_XS_R(5, iy, d);
                                break;
                            case 0xEB: // SET 5, (IY + s), E
                                SET_XS_R(5, iy, e);
                                break;
                            case 0xEC: // SET 5, (IY + s), H
                                SET_XS_R(5, iy, h);
                                break;
                            case 0xED: // SET 5, (IY + s), L
                                SET_XS_R(5, iy, l);
                                break;
                            case 0xEE: // SET 5, (IY + s)
                                SET_XS(5, iy);
                                break;
                            case 0xEF: // SET 5, (IY + s), A
                                SET_XS_R(5, iy, a);
                                break;
                            case 0xF0: // SET 6, (IY + s), B
                                SET_XS_R(6, iy, b);
                                break;
                            case 0xF1: // SET 6, (IY + s), C
                                SET_XS_R(6, iy, c);
                                break;
                            case 0xF2: // SET 6, (IY + s), D
                                SET_XS_R(6, iy, d);
                                break;
                            case 0xF3: // SET 6, (IY + s), E
                                SET_XS_R(6, iy, e);
                                break;
                            case 0xF4: // SET 6, (IY + s), H
                                SET_XS_R(6, iy, h);
                                break;
                            case 0xF5: // SET 6, (IY + s), L
                                SET_XS_R(6, iy, l);
                                break;
                            case 0xF6: // SET 6, (IY + s)
                                SET_XS(6, iy);
                                break;
                            case 0xF7: // SET 6, (IY + s), A
                                SET_XS_R(6, iy, a);
                                break;
                            case 0xF8: // SET 7, (IY + s), B
                                SET_XS_R(7, iy, b);
                                break;
                            case 0xF9: // SET 7, (IY + s), C
                                SET_XS_R(7, iy, c);
                                break;
                            case 0xFA: // SET 7, (IY + s), D
                                SET_XS_R(7, iy, d);
                                break;
                            case 0xFB: // SET 7, (IY + s), E
                                SET_XS_R(7, iy, e);
                                break;
                            case 0xFC: // SET 7, (IY + s), H
                                SET_XS_R(7, iy, h);
                                break;
                            case 0xFD: // SET 7, (IY + s), L
                                SET_XS_R(7, iy, l);
                                break;
                            case 0xFE: // SET 7, (IY + s)
                                SET_XS(7, iy);
                                break;
                            case 0xFF: // SET 7, (IY + s), A
                                SET_XS_R(7, iy, a);
                                break;
                        }
                        break;
                    case 0xCC: // CALL Z, NN
                        CALL_CND_NN(f & ZF);
                        break;
                    case 0xCD: // CALL NN
                        CALL_NN;
                        break;
                    case 0xCE: // ADC A, N
                        ADC_XR(pc++);
                        break;
                    case 0xCF: // RST 8
                        RST(0x08);
                        break;
                    case 0xD0: // RET NC
                        RET_CND(!(f & CF));
                        break;
                    case 0xD1: // POP DE
                        POP(de);
                        break;
                    case 0xD2: // JP NC, NN
                        JP_CND_NN(!(f & CF));
                        break;
                    case 0xD3: // OUT (N), A
                        OUT_N_A;
                        break;
                    case 0xD4: // CALL NC, NN
                        CALL_CND_NN(!(f & CF));
                        break;
                    case 0xD5: // PUSH DE
                        PUSH(de);
                        break;
                    case 0xD6: // SUB A, N
                        SUB_XR(pc++);
                        break;
                    case 0xD7: // RST 10
                        RST(0x10);
                        break;
                    case 0xD8: // RET C
                        RET_CND(f & CF);
                        break;
                    case 0xD9: // EXX
                        bc ^= alt.bc;
                        alt.bc ^= bc;
                        bc ^= alt.bc;
                        de ^= alt.de;
                        alt.de ^= de;
                        de ^= alt.de;
                        hl ^= alt.hl;
                        alt.hl ^= hl;
                        hl ^= alt.hl;
                        time(4);
                        break;
                    case 0xDA: // JP C, NN
                        JP_CND_NN(f & CF);
                        break;
                    case 0xDB: // IN A, (N)
                        IN_A_N;
                        break;
                    case 0xDC: // CALL C, NN
                        CALL_CND_NN(f & CF);
                        break;
                    case 0xDD: // --------------- FDDD ---------------
                        irl--;
                        pc--;
                        break;
                    case 0xDE: // SBC A, N
                        SBC_XR(pc++);
                        break;
                    case 0xDF: // RST 18
                        RST(0x18);
                        break;
                    case 0xE0: // RET PO
                        RET_CND(!(f & PF));
                        break;
                    case 0xE1: // POP IY
                        POP(iy);
                        break;
                    case 0xE2: // JP PO, NN
                        JP_CND_NN(!(f & PF));
                        break;
                    case 0xE3: // EX (SP), IY
                        EX_SP_RR(iy);
                        break;
                    case 0xE4: // CALL PO, NN
                        CALL_CND_NN(!(f & PF));
                        break;
                    case 0xE5: // PUSH IY
                        PUSH(iy);
                        break;
                    case 0xE6: // AND A, N
                        AND_XR(pc++);
                        break;
                    case 0xE7: // RST 20
                        RST(0x20);
                        break;
                    case 0xE8: // RET PE
                        RET_CND(f & PF);
                        break;
                    case 0xE9: // JP IY
                        JP_RR(iy);
                        break;
                    case 0xEA: // JP PE, NN
                        JP_CND_NN(f & PF);
                        break;
                    case 0xEB: // EX DE, HL
                        EX_RR_RR(de, hl);
                        break;
                    case 0xEC: // CALL PE, NN
                        CALL_CND_NN(f & PF);
                        break;
                    case 0xED: // --------------- FDED ---------------
                        irl--;
                        pc--;
                        break;
                    case 0xEE: // XOR A, N
                        XOR_XR(pc++);
                        break;
                    case 0xEF: // RST 28
                        RST(0x28);
                        break;
                    case 0xF0: // RET P
                        RET_CND(!(f & SF))
                        break;
                    case 0xF1: // POP AF
                        POP(af);
                        break;
                    case 0xF2: // JP P, NN
                        JP_CND_NN(!(f & SF));
                        break;
                    case 0xF3: // DI
                        iff1 = iff2 = 0x00;
                        time(4);
                        break;
                    case 0xF4: // CALL P, NN
                        CALL_CND_NN(!(f & SF));
                        break;
                    case 0xF5: // PUSH AF
                        PUSH(af);
                        break;
                    case 0xF6: // OR A, N
                        OR_XR(pc++);
                        break;
                    case 0xF7: // RST 30
                        RST(0x30);
                        break;
                    case 0xF8: // RET M
                        RET_CND(f & SF);
                        break;
                    case 0xF9: // LD SP, IY
                        LD_RR_RR(sp, iy);
                        break;
                    case 0xFA: // JP M, NN
                        JP_CND_NN(f & SF);
                        break;
                    case 0xFB: // EI
                        iff1 = iff2 = 1; // IFF1, IFF2 = true
                        time(4);
                        break;
                    case 0xFC: // CALL M, NN
                        CALL_CND_NN(f & SF);
                        break;
                    case 0xFD: // --------------- FDFD ---------------
                        irl--;
                        pc--;
                        break;
                    case 0xFE: // CP N
                        CP_XR(pc++);
                        break;
                    case 0xFF: // RST 38
                        RST(0x38);
                        break;
                }
		        break;
            // Continue not prefixed opcodes.
            case 0xFE: // CP N
                CP_XR(pc++);
                break;
            case 0xFF: // RST 38
                RST(0x38);
                break;
        }
    }
}
