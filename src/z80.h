#define CF                  0x01            // Carry
#define NF                  0x02            // ADD(0) or SUB(1)
#define PF                  0x04            // Parity and Overflow
#define F3                  0x08            // Bit 3 of result
#define HF                  0x10            // Half carry used DAA
#define F5                  0x20            // Bit 5 of result
#define ZF                  0x40            // Zero
#define SF                  0x80            // Negative. Bit 7 of result.

#define LD_IR_PF_CLK        18              // Time while PF-flag was not change (Undocumented behievior: LD A, I and LD A, R).
#define Z80_FREQ            3579545

#define Reg16(reg16)\
    union {\
        u16 reg16;\
        struct {\
            u8 reg16##l;\
            u8 reg16##h;\
        };\
    };

#define Reg88(h8, l8)\
    union {\
        u16 h8##l8;\
        union {\
            struct {\
                u8 h8##l8##l;\
                u8 h8##l8##h;\
            };\
            struct {\
                u8 l8;\
                u8 h8;\
            };\
        };\
    };

struct Z80_State {
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
    Reg16(memptr);   // Internal ptr

    s32 clk;
    u8 im;           // Interrupt mode.
    u8 iff1;         // Int is enabled.
    u8 iff2;         // Save's iff1 while NMI.
    u8 r8bit;        // 8 bbt of the R.
};

struct Z80 : public Z80_State {
    public:
        Z80();
        void reset();
        void frame(ULA *memory, IO *io, s32 frame_clk);
        void interrupt(ULA *memory);
        void step_over(ULA *memory, IO *io, s32 frame_clk);
        void step_into(ULA *memory, IO *io, s32 frame_clk);
        void NMI();
    private:
        u8 flag_inc[0x100];
        u8 flag_dec[0x100];
        u8 flag_parity[0x100];
        u8 flag_adc[0x20000];
        u8 flag_sbc[0x20000];
        u8 flag_sz53p[0x100];
        u8 flag_cp[0x10000];
        u8 flag_cpb[0x10000];
};
