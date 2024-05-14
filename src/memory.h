#define PAGE_SIZE           0x4000
#define RAM_PAGES           8
#define TRAP_TRDOS_BYTE     0x5B

// Port 7FFD bits
#define PAGE_MASK       0b00000111
#define ULA_PAGE5       0b00001000
#define BANK0_ROM48     0b00010000
#define PORT_LOCKED     0b00100000

class Memory : public Device {
    public:
        Memory();
        ~Memory();

        inline u8 read_byte(u16 ptr){
            return page_rd[ptr >> 0x0E][ptr];
        };
        inline u8 read_byte_ex(u16 ptr){
            return page_ex[ptr >> 0x0E][ptr];
        };
        inline void write_byte(u16 ptr, u8 byte, s32 clk){
            page_wr[ptr >> 0x0E][ptr] = byte;
        };

        void load_rom(ROM_Bank bank, const char *path);
        void set_main_rom(ROM_Bank bank);
        bool trap_trdos(u16 pc);
        bool is_trdos_active(){ return page_ex[0] == rom[ROM_Trdos]; };
        u8* page(int page_num) { return ram[page_num]; };
        u8 read_7FFD(){ return port_7FFD; };

        void write(u16 port, u8 byte, s32 clk);
        void reset();

    protected:
        ROM_Bank main_rom = ROM_Trdos;
        u8 *page_wr_null;
        u8 *rom[sizeof(ROM_Bank)];
        u8 *trap[sizeof(ROM_Bank)];
        u8 *ram[RAM_PAGES];
        u8 *page_rd[4];
        u8 *page_wr[4];
        u8 *page_ex[4];
        u8 port_7FFD;
};
