#include <cstddef>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "base.h"

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


namespace Snapshot {
    unsigned int rle_decode(unsigned char **pages, int *offset, unsigned char *src, unsigned int src_size){
        unsigned int idx = 0;
        while (idx < src_size){
            if (idx + 4 < src_size && src[idx] == 0xED && src[idx+1] == 0xED){
                for (int cnt = 0; cnt < src[idx+2]; cnt++){
                    (*pages)[(*offset)++] = src[idx+3];
                    if (*offset >= PAGE_SIZE){
                        *offset = 0;
                        pages++;
                        if (!*pages)
                            return idx;
                    }
                }
                idx += 4;
            }else{
                (*pages)[(*offset)++] = src[idx++];
                if (*offset >= PAGE_SIZE){
                    *offset = 0;
                    pages++;
                    if (!*pages)
                        return idx;
                }
            }
        }
        return idx;
    }

    void save_z80(const char *path, Z80 *cpu, Memory *memory, IO *io){
        FILE *fp = fopen(path, "wb");
        struct Z80_Header header {
            .a = cpu->a, .f = cpu->f,
            .bc = cpu->bc, .hl = cpu->hl, .pc = 0, .sp = cpu->sp,
            .i = cpu->irh, .r = cpu->irl,
            .de = cpu->de,
            .alt_bc = cpu->alt.bc, .alt_de = cpu->alt.de, .alt_hl = cpu->alt.hl,
            .alt_a = cpu->alt.a, .alt_f = cpu->alt.f,
            .iy = cpu->iy, .ix = cpu->ix,
            .iff1 = cpu->iff1, .iff2 = cpu->iff2, .im = cpu->im,

            .ext_length = header_v300_length,
            .ext_pc = cpu->pc,
            .ram_paged = 0,
            .emu_opt = 0b1000111,
            .pfffd = 0x0F
        };
        header.flags = ((cpu->r8bit >> 7) & 0x01);
        header.mode = 4;
        header.p7ffd = memory->read_7FFD();
        header.clk_lo = cpu->clk & 0xFFFF;
        header.clk_hi = cpu->clk >> 16;
        struct Z80_Data data { .length = 0xFFFF };
        for (int i = 0; i < 0x10; i++){
            io->write(0xFFFD, i);
            header.ay_regs[i] = io->read(0xBFFD);
        }
        if (fwrite(&header, sizeof(Z80_Header), 1, fp) != 1)
            throw std::runtime_error("Write z80 snapshot header");
        for (unsigned char page = 0; page <= 7 ; page++){
            data.page = page + 3;
            if (fwrite(&data, sizeof(Z80_Data), 1, fp) != 1)
                throw std::runtime_error("Write data header");
            if (fwrite(memory->page(page), 0x4000, 1, fp) != 1)
                throw std::runtime_error("Write page data");
        }
        fclose(fp);
    }

    HW_Model load_z80(const char *path, Z80 *cpu, Memory *memory, IO *io){
        HW_Model hw = HW_SPECTRUM_128;
        Z80_Header *header;
        int page_mode = 1;
        int page_map[2][12] = { { -1, -1, -1, -1, 2, 0, -1, -1, 5, -1, -1, -1 }, { -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, -1 } };
        unsigned int idx, block_size, page;
        unsigned char *pages[4];
        int page_offset;
        size_t size;
        FILE *fp = fopen(path, "rb");
        fseek(fp, 0, SEEK_END);
        size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        unsigned char *data = new unsigned char[size];
        if (fread(data, 1, size, fp) != size)
            throw std::runtime_error("Read Z80 snapshot");
        fclose(fp);
        idx = header_v145_length;
        header = (Z80_Header*)data;
        if (header->flags == 0xFF)
            header->flags = 1;
        if (!header->pc){
            idx += header->ext_length + 2;
            if (header->ext_length == header_v201_length){
                if (header->mode < 3)
                    hw = HW_SPECTRUM_48;
            }else{
                if (header->mode < 4)
                    hw = HW_SPECTRUM_48;
            }
            if (header->mode == 9)
                hw = HW_PENTAGON_128;
            page_mode = (hw == HW_SPECTRUM_128 || hw == HW_PENTAGON_128) ? 1 : 0;
            while (idx + 4 < size){
                block_size = data[idx] | (data[idx + 1] << 8);
                page = data[idx + 2];
                idx += 3;
                if (page >= 12)
                    throw std::runtime_error("RAM-page index overflow");
                if (block_size == 0xFFFF){ // Not encoded.
                    block_size = 0x4000;
                    if (idx + block_size > size)
                        throw std::runtime_error("Wrong block size");
                    memcpy(memory->page(page_map[page_mode][page]), &data[idx], block_size);
                    idx += block_size;
                }else{
                    pages[0] = memory->page(page_map[page_mode][page]);
                    pages[1] = NULL;
                    page_offset = 0;
                    idx += rle_decode(pages, &page_offset, &data[idx], block_size);
                }
            }
            io->write(0x7FFD, page_mode == 1 ? header->p7ffd : 0x10);
            for (int i = 0; i < 0x10; i++){
                io->write(0xFFFD, i);
                io->write(0xBFFD, header->ay_regs[i]);
            }
            cpu->pc = header->ext_pc;
        }else{
            if (header->flags & 0b100000){ // RLE
                pages[0] = memory->page(5);
                pages[1] = memory->page(2);
                pages[2] = memory->page(0);
                pages[3] = NULL;
                page_offset = 0;
                rle_decode(pages, &page_offset, &data[idx], size - idx);
            }else{
                memcpy(memory->page(5), &data[idx], 0x4000);
                memcpy(memory->page(2), &data[idx + 0x4000], 0x4000);
                memcpy(memory->page(0), &data[idx + 0x8000], 0x4000);
            }
            io->write(0x7FFD, 0x10);
            cpu->pc = header->pc;
        }
        cpu->a = header->a;
        cpu->f = header->f;
        cpu->bc = header->bc;
        cpu->hl = header->hl;
        cpu->sp = header->sp;
        cpu->irh = header->i;
        cpu->irl = header->r & 0x7F;
        cpu->r8bit = (header->flags & 0x01) << 7;
        cpu->de = header->de;
        cpu->alt.bc = header->alt_bc;
        cpu->alt.de = header->alt_de;
        cpu->alt.hl = header->alt_hl;
        cpu->alt.a = header->alt_a;
        cpu->alt.f = header->alt_f;
        cpu->iy = header->iy;
        cpu->ix = header->ix;
        cpu->iff1 = header->iff1;
        cpu->iff2 = header->iff2;
        cpu->im = header->im & 0x03;
        io->write(0xFE, (header->flags >> 1) & 0x07);
        DELETE_ARRAY(data);
        return hw;
    }
}
