#include <cstddef>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "base.h"

using namespace std;

Snapshot::Snapshot(){
    p_file = NULL;
    p_data = NULL;
}

Snapshot::~Snapshot(){
    free();
}

void Snapshot::free(){
    if (p_file)
        fclose(p_file);
    p_file = NULL;
    DELETE_ARRAY(p_data);
}

int Snapshot::rle_decode(unsigned char **p_pages, unsigned int *p_page_offset, unsigned char *p_encoded, int encoded_size){
    int idx = 0;
    while (idx < encoded_size and *p_pages){
        if (idx + 4 < encoded_size and p_encoded[idx] == 0xED and p_encoded[idx+1] == 0xED){
            for (int i = 0; i < p_encoded[idx+2]; i++){
                (*p_pages)[(*p_page_offset)++] = p_encoded[idx+3];
                if (*p_page_offset >= PAGE_SIZE){
                    *p_page_offset = 0;
                    if (!*++p_pages)
                        break;
                }
            }
            idx += 4;
        }else{
            (*p_pages)[(*p_page_offset)++] = p_encoded[idx++];
            if (*p_page_offset >= PAGE_SIZE){
                *p_page_offset = 0;
                if (!*++p_pages)
                    break;
            }
        }
    }
    return idx;
}

void Snapshot::save_z80(const char *p_path, Z80 *p_cpu, Memory *p_memory, IO *p_io){
    try {
        p_file = fopen(p_path, "wb");
        struct Z80_Header header {
            .a = p_cpu->a, .f = p_cpu->f,
            .bc = p_cpu->bc, .hl = p_cpu->hl, .pc = 0, .sp = p_cpu->sp,
            .i = p_cpu->irh, .r = p_cpu->irl,
            .de = p_cpu->de,
            .alt_bc = p_cpu->alt.bc, .alt_de = p_cpu->alt.de, .alt_hl = p_cpu->alt.hl,
            .alt_a = p_cpu->alt.a, .alt_f = p_cpu->alt.f,
            .iy = p_cpu->iy, .ix = p_cpu->ix,
            .iff1 = p_cpu->iff1, .iff2 = p_cpu->iff2, .im = p_cpu->im,

            .ext_length = header_v300_length,
            .ext_pc = p_cpu->pc,
            .ram_paged = 0,
            .emu_opt = 0b1000111,
            .pfffd = 0x0F
        };
        header.flags = ((p_cpu->r8bit >> 7) & 0x01);
        header.mode = 4;
        header.p7ffd = p_memory->read_7FFD();
        header.clk_lo = p_cpu->clk & 0xFFFF;
        header.clk_hi = p_cpu->clk >> 16;
        struct Z80_Data data { .length = 0xFFFF };
        for (int i = 0; i < 0x10; i++){
            p_io->write(0xFFFD, i);
            header.ay_regs[i] = p_io->read(0xBFFD);
        }
        if (fwrite(&header, sizeof(Z80_Header), 1, p_file) != 1)
            throw runtime_error("Write z80 snapshot header");
        for (unsigned char page = 0; page <= 7 ; page++){
            data.page = page + 3;
            if (fwrite(&data, sizeof(Z80_Data), 1, p_file) != 1)
                throw runtime_error("Write data header");
            if (fwrite(p_memory->page(page), 0x4000, 1, p_file) != 1)
                throw runtime_error("Write page data");
        }
        fclose(p_file);
        p_file = NULL;
    }catch(exception &e){
        cerr << "EXCEPTION: " << e.what();
    }
}

void memory_dump(unsigned char *p_page, unsigned short ptr, int length, int line_split){
    for (int i = 0; i < length; i++){
        if (!(i % line_split))
            printf("\n%04x ", ptr+i);
        printf(" %02x", p_page[i]);
    }
}

HW Snapshot::load_z80(const char *p_path, Z80 *p_cpu, Memory *p_memory, IO *p_io){
    HW hw = HW::SPECTRUM_128;
    Z80_Header *p_header;
    int page_mode = 1;
    int page_map[2][12] = { { -1, -1, -1, -1, 2, 0, -1, -1, 5, -1, -1, -1 }, { -1, -1, -1, 0, 1, 2, 3, 4, 5, 6, 7, -1 } };
    int idx, block_size, page;
    unsigned char *p_pages[4];
    unsigned int page_offset;
    try {
        p_file = fopen(p_path, "rb");
        fseek(p_file, 0, SEEK_END);
        file_size = ftell(p_file);
        fseek(p_file, 0, SEEK_SET);
        p_data = new unsigned char[file_size];
        if (fread(p_data, 1, file_size, p_file) != (size_t)file_size)
            throw range_error("Read file data error");
        idx = header_v145_length;
        p_header = (Z80_Header*)p_data;
        if (p_header->flags == 0xFF)
            p_header->flags = 1;
        if (!p_header->pc){
            idx += p_header->ext_length + 2;
            if (p_header->ext_length == header_v201_length){
                if (p_header->mode < 3)
                    hw = HW::SPECTRUM_48;
            }else{
                if (p_header->mode < 4)
                    hw = HW::SPECTRUM_48;
            }
            if (p_header->mode == 9)
                hw = HW::PENTAGON_128;
            page_mode = (hw == HW::SPECTRUM_128 || hw == HW::PENTAGON_128) ? 1 : 0;
            while (idx + 4 < file_size){
                block_size = p_data[idx] | (p_data[idx + 1] << 8);
                page = p_data[idx + 2];
                idx += 3;
                if (page >= 12)
                    throw range_error("RAM page index overflow");
                if (block_size == 0xFFFF){ // Not encoded.
                    block_size = 0x4000;
                    if (idx + block_size > file_size)
                        throw range_error("Block size out of file data");
                    memcpy(p_memory->page(page_map[page_mode][page]), &p_data[idx], block_size);
                    idx += block_size;
                }else{
                    p_pages[0] = p_memory->page(page_map[page_mode][page]);
                    p_pages[1] = NULL;
                    page_offset = 0;
                    idx += rle_decode(p_pages, &page_offset, &p_data[idx], block_size);
                }
            }
            p_io->write(0x7FFD, page_mode == 1 ? p_header->p7ffd : 0x10);
            for (int i = 0; i < 0x10; i++){
                p_io->write(0xFFFD, i);
                p_io->write(0xBFFD, p_header->ay_regs[i]);
            }
            p_cpu->pc = p_header->ext_pc;
        }else{
            if (p_header->flags & 0b100000){ // RLE
                p_pages[0] = p_memory->page(5);
                p_pages[1] = p_memory->page(2);
                p_pages[2] = p_memory->page(0);
                p_pages[3] = NULL;
                page_offset = 0;
                rle_decode(p_pages, &page_offset, &p_data[idx], file_size - idx);
                //memory_dump(p_memory->page(5), 0x4000, 0x4000, 0x10);
                printf("\n");
                //memory_dump(p_memory->page(2), 0x8000, 0x4000, 0x10);
                printf("\n");
                //memory_dump(p_memory->page(0), 0xC000, 0x4000, 0x10);
                printf("\n");
            }else{
                memcpy(p_memory->page(5), &p_data[idx], 0x4000);
                memcpy(p_memory->page(2), &p_data[idx + 0x4000], 0x4000);
                memcpy(p_memory->page(0), &p_data[idx + 0x8000], 0x4000);
            }
            p_io->write(0x7FFD, 0x10);
            p_cpu->pc = p_header->pc;
        }
        p_cpu->a = p_header->a;
        p_cpu->f = p_header->f;
        p_cpu->bc = p_header->bc;
        p_cpu->hl = p_header->hl;
        p_cpu->sp = p_header->sp;
        p_cpu->irh = p_header->i;
        p_cpu->irl = p_header->r & 0x7F;
        p_cpu->r8bit = (p_header->flags & 0x01) << 7;
        p_cpu->de = p_header->de;
        p_cpu->alt.bc = p_header->alt_bc;
        p_cpu->alt.de = p_header->alt_de;
        p_cpu->alt.hl = p_header->alt_hl;
        p_cpu->alt.a = p_header->alt_a;
        p_cpu->alt.f = p_header->alt_f;
        p_cpu->iy = p_header->iy;
        p_cpu->ix = p_header->ix;
        p_cpu->iff1 = p_header->iff1;
        p_cpu->iff2 = p_header->iff2;
        p_cpu->im = p_header->im & 0x03;
        p_io->write(0xFE, (p_header->flags >> 1) & 0x07);
    }catch(exception &e){
        cerr << e.what();
        throw(e);
    }
    free();
    return hw;
}
