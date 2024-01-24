
    #define header_v145_length 30
    #define header_v201_length 23
    #define header_v300_length 54

    #pragma pack(1)
    struct Z80_Header {
        unsigned char a;
        unsigned char f;
        unsigned short bc;
        unsigned short hl;
        unsigned short pc;
        unsigned short sp;
        unsigned char i;
        unsigned char r;
        unsigned char flags;
        unsigned short de;
        unsigned short alt_bc;
        unsigned short alt_de;
        unsigned short alt_hl;
        unsigned char alt_a;
        unsigned char alt_f;
        unsigned short iy;
        unsigned short ix;
        unsigned char iff1;
        unsigned char iff2;
        unsigned char im;
        // Version 2.01 extension.
        unsigned short ext_length;
        unsigned short ext_pc;
        unsigned char mode;
        unsigned char p7ffd;
        unsigned char ram_paged;
        unsigned char emu_opt;
        unsigned char pfffd;
        unsigned char ay_regs[0x10];
        // Version 3.00
        unsigned short clk_lo;
        unsigned char clk_hi;
        unsigned char skip[28];
    };
    struct Z80_Data {
        unsigned short length;
        unsigned char page;
    };
#pragma pack()

class Snapshot {
    public:
        Snapshot();
        ~Snapshot();
        HW load_z80(const char *p_path, Z80 *p_cpu, Memory *p_memory, IO *p_io);
        void save_z80(const char *p_path, Z80 *p_cpu, Memory *p_memory, IO *p_io);
        int rle_decode(unsigned char **p_pages, unsigned int *p_page_offset, unsigned char *p_encoded, int encoded_size);
    private:
        void free();
        FILE *p_file;
        int file_size;
        unsigned char *p_data;
};


