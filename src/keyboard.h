/*
 
 Line | Port | Lower 5 bits
 ------------------------------
 0    | 7FFE | B, N, M,SH, SP
 1    | BFFE | H, J, K, L, EN
 2    | DFFE | Y, U, I, O, P
 3    | EFFE | 6, 7, 8, 9, 0
 4    | F7FE | 5, 4, 3, 2, 1
 5    | FBFE | T, R, E, W, Q
 6    | FDFE | G, F, D, S, A
 7    | FEFE | V, C, X, Z, CH

 */

class Keyboard : public Device {
    public:
        void set_btn_state(unsigned short port, unsigned char btn_mask, bool pressed);
        bool io_rd(unsigned short port, unsigned char *p_byte, int clk);
    private:
        unsigned char kbd_state[8] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };
};
