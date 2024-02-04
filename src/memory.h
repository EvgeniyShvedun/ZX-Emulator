#define PAGE_SIZE           0x4000
#define ROM_PAGES           3
#define RAM_PAGES           8

// Port 7FFD
#define P7FFD_PAGE          0b00000111
#define P7FFD_SCR7          0b00001000
#define P7FFD_ROM48         0b00010000
#define P7FFD_LOCK          0b00100000

#define ROM_TRDOS           0
#define ROM_128             1
#define ROM_48              2

class Memory : public Device {
    public:
        Memory();
        ~Memory();
        inline unsigned char read_byte(unsigned short ptr){ return p_page_rd[ptr >> 0x0E][ptr]; };
        inline unsigned char read_byte_ex(unsigned short ptr){ return p_page_ex[ptr >> 0x0E][ptr]; };
        inline void write_byte(unsigned short ptr, unsigned char byte, int clk=0){ p_page_wr[ptr >> 0x0E][ptr] = byte; };
        bool exec_trap(unsigned short pc);
        bool trdos_active(){ return p_page_ex[0] == p_rom[ROM_TRDOS]; };
        unsigned char* page(int n) { return (n < RAM_PAGES and n >= 0) ? p_ram[n] : NULL; };
        void load_rom(int id, const char *p_file_path);
        bool io_wr(unsigned short port, unsigned char byte, int clk);
        void reset();
        void set_rom(int id);
        unsigned char read_7FFD(){ return p7FFD; };
    protected:
        unsigned char *p_page_wr_null;
        unsigned char *p_rom[ROM_PAGES];
        unsigned char *p_trap[ROM_PAGES];
        unsigned char *p_ram[RAM_PAGES];
        unsigned char *p_page_rd[4];
        unsigned char *p_page_wr[4];
        unsigned char *p_page_ex[4];
        int rom_id;
        unsigned char p7FFD;
};
