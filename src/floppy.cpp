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
#include "z80.h"
#include "floppy.h"
//#include <algorithm>

//#define DEBUG
using namespace std;

FDC::FDC(){
    reset();
}

FDC::~FDC(){
    for (int i = 0; i < 4; i++)
        DELETE_ARRAY(fdd[i].data);
}

void FDC::update(s32 clk){
    int steps, bytes;
    FDD &disk = fdd[reg_system & 0x03];
    clk -= last_clk;
    last_clk += clk;
    #ifdef DEBUG
        printf("Command %02x, status: %02x, head: %d, reg_track: %d, reg_sector: %d, reg_data: %d, step_dir: %d, motor: %d, com_clk: %d, clk: %d\n", reg_command, reg_status, disk.track, reg_track, reg_sector, reg_data, disk.step_dir, disk.motor_clk, command_clk, clk);
    #endif
    if (disk.motor_clk > MOTOR_CLK)
        return;
    disk.motor_clk += clk;
    // Type I. 
    if (disk.data && !(reg_command & 0x80)){
        if (disk.motor_clk % DISK_CLK < INDEX_CLK)
            reg_status |= STS_INDEX;
        else
            reg_status &= ~STS_INDEX;
    }
    if (!(reg_status & STS_BUSY))
        return;
    command_clk += clk;
    if (command_clk < delay_clk && (reg_command & 0x80) && (reg_command & 0xF0) != 0xD0)
        return;
    command_clk -= delay_clk;
    delay_clk = 0;
    switch ((reg_command >> 4) & 0x0F){
        case 0x00: // Restore
        case 0x01: // Seek
            if (reg_track == reg_data || (disk.step_dir == -1 && !disk.track)){
                if (!disk.track){
                    reg_status |= STS_TRACK0;
                    reg_track = 0x00;
                }
                if (reg_command & CMD_VERIFY){
                    reg_status |= STS_HEAD_LOAD;
                    if (reg_track != disk.track)
                        reg_status |= STS_SEEK_ERROR;
                }
                reg_status &= ~STS_BUSY;
                break;
            }
            steps = command_clk / step_time[reg_command & CMD_STEP_RATE];
            if (!steps)
                break;
            if (disk.step_dir > 0){
                if (steps > reg_data - reg_track)
                    steps = reg_data - reg_track;
                disk.track = min(disk.track + steps, 79);
                if (reg_command & CMD_MODIFY)
                    reg_track += steps;
            }else{
                steps = min(reg_track - reg_data, min(steps, disk.track));
                disk.track -= steps;
                if (reg_command & CMD_MODIFY)
                    reg_track -= steps;
            }
            command_clk -= steps * step_time[reg_command & CMD_STEP_RATE];
            break;
        case 0x02: // Step
        case 0x03: // Step modify
        case 0x04: // Step forward
        case 0x05: // Step forward modify
        case 0x06: // Step backward
        case 0x07: // Step backward modify
            if (command_clk < step_time[reg_command & CMD_STEP_RATE])
                break;
            if (disk.step_dir > 0){
                if (reg_command & CMD_MODIFY)
                    reg_track++;
                if (disk.track < 79)
                    disk.track++;
            }else{
                if (reg_command & CMD_MODIFY)
                    reg_track--;
                if (disk.track > 0)
                    disk.track--;
            }
            if (!disk.track){
                reg_track = 0x00;
                reg_status |= STS_TRACK0;
            }
            if (reg_command & CMD_VERIFY){
                reg_status |= STS_HEAD_LOAD;
                if (reg_track != disk.track)
                    reg_status |= STS_SEEK_ERROR;
            }
            reg_status &= ~STS_BUSY;
            break;
        case 0x08: // Read sector
        case 0x09: // Read sectors
            if (command_clk < DRQ_CLK)
                break;
            if (reg_command & CMD_MULTI_SECTOR){
                bytes = min(command_clk / DRQ_CLK, 0x100 * (0x10 - reg_sector - 1) - data_idx);
                reg_sector += data_idx >> 8;
                data_idx += bytes;
                data_idx %= 0x101;
            }else{
                bytes = min(command_clk / DRQ_CLK, 0x100 - data_idx);
                data_idx += bytes;
            }
            command_clk -= bytes * DRQ_CLK;
            if (bytes > 1 || reg_status & STS_DRQ)
                reg_status |= STS_DATA_LOST;
            if (disk.data && bytes){
                reg_data = disk.data[disk.track * 0x2000 + (reg_system & SYS_HEAD ? 0 : 0x1000) + (min(reg_sector - 1, 0x10 - 1) * 0x100) + data_idx - 1];
                reg_status |= STS_DRQ;
            }else
                reg_status &= ~(STS_BUSY | STS_DRQ);
            break;
        case 0x0A: // Write sector
        case 0x0B: // Write sectors
            reg_status &= ~STS_BUSY;
            break;
        case 0x0C: // Read address
            if (command_clk < DRQ_CLK)
                break;
            bytes = min(command_clk / DRQ_CLK, 6 - data_idx);
            if (bytes > 1 || reg_status & STS_DRQ)
                reg_status |= STS_DATA_LOST;
            if (bytes){
                data_idx += bytes;
                command_clk -= bytes * DRQ_CLK;
                switch(data_idx - 1){
                    case 0x00:
                        reg_data = disk.track;
                        break;
                    case 0x01:
                        reg_data = reg_system & SYS_HEAD ? 1 : 0;
                        break;
                    case 0x02: // sector
                        reg_data = command_clk % (0x10 + 0x01);
                        break;
                    case 0x03: // sector size
                        reg_data = 1;
                        break;
                    case 0x04: // crc
                        reg_data = 0x00;
                        break;
                    case 0x05:
                        reg_data = 0x00;
                        break;
                }
                reg_status |= STS_DRQ;
            }else{
                reg_sector = disk.track;
                reg_status &= ~(STS_BUSY | STS_DRQ);
            }
            break;
        case 0x0D: // Force interrupt
            /*
            if (!(reg_command & 0x0F)){
                reg_status = 0x00;
            else{
                if (reg_command & 0x04){
                    if (disk.index_label_clk % DISK_CLK < STS_INDEX_LABEL_CLK)
                        reg_status = 0x00;
                }else{
                    if (reg_status & 0x08)
                        reg_status &= STS_BUSY;
                }
            }*/
            reg_status = 0x00;
            break;
        case 0x0E: // Read track
        case 0x0F: // Write track
            reg_status &= ~(STS_BUSY | STS_DRQ);
            break;
    }
}

void FDC::read(u16 port, u8 *byte, s32 clk){
    update(clk);
    if (!(port & 0x80)){                // System port decode only by A8.
        switch ((port >> 5) & 0x03){    // All other by A8-A6.
            case 0x00:
                *byte = reg_status;
                break;
            case 0x01:
                *byte = reg_track;
                break;
            case 0x02:
                *byte = reg_sector;
                break;
            case 0x03:
                *byte = reg_data;
                reg_status &= ~STS_DRQ;
                break;
        }
    }else{
        *byte &= ~(SYS_INTRQ | SYS_DRQ);
        if ((reg_command & 0x80) && (reg_command & 0xF0) != 0xD0 && reg_status & STS_DRQ)
            *byte |= SYS_DRQ;
        if (!(reg_status & STS_BUSY))
            *byte |= SYS_INTRQ;
    }
    #ifdef DEBUG
       printf("WD read %02x -> %02x\n", port & 0xFF, *byte);
    #endif
}

void FDC::write(u16 port, u8 byte, s32 clk){
    #ifdef DEBUG
        printf("WD write %x -> %x\n", (port & 0xFF), byte);
    #endif
    update(clk);
    if (port & 0x80){
        reg_system = byte;
        if (!(byte & SYS_RESET))
            reset();
    }
    FDD &disk = fdd[reg_system & 0x03];
    switch((port >> 5) & 0x03){
        case 0x00: // 1F
            if ((byte & 0xF0) == 0xF0 && byte & 0x08) // The unknown bits is threated as "Forced Interrupt" command.
                byte &= 0xD0; 
            if ((byte & 0xF0) != 0xD0 && reg_status & STS_BUSY)
                break;
            command_clk = 0; 
            delay_clk = (byte & (0x80 | CMD_DELAY)) && ((byte & 0xF0) != 0xD0) ? 15 * Z80_FREQ / 1000 : 0;
            disk.motor_clk %= DISK_CLK;
            reg_command = byte;
            reg_status = STS_BUSY;
            switch((byte >> 4) & 0x0F){
                case 0x00: // Restore.
                    reg_track = 0xFF;
                    reg_data = 0x00;
                    disk.step_dir = -1;
                    break;
                case 0x01: // Seek
                    disk.step_dir = reg_data > reg_track ? 1 : -1;
                    break;
                case 0x02: // Step
                case 0x03: // Step modify
                    break;
                case 0x04: // Step forward
                case 0x05: // Step forward modify
                    disk.step_dir = 1;
                    break;
                case 0x06: // Step backward
                case 0x07: // Step backward modify
                    disk.step_dir = -1;
                    break;
                // Type II
                case 0x0A: // Write sector
                case 0x0B: // Write sectors
                    reg_status |= STS_WRITE_PROTECT; /////////
                    break;
                case 0x08: // Read sector
                case 0x09: // Read sectors
                    if (reg_track != disk.track || reg_sector > 0x10){
                        reg_status |= STS_RNF;
                        reg_status &= ~STS_BUSY;
                    }
                    data_idx = 0;
                    break;
                // Type III
                case 0x0C: // Read address
                case 0x0E: // Read track
                case 0x0F: // Write track
                    data_idx = 0;
                    break;
                case 0x0D: // Force interrupt
                    break;
            }
            break;
        case 0x01: // 0x3F
            if (!(reg_status & STS_BUSY))
                reg_track = byte;
            break;
        case 0x02: // 0x5F
            if (!(reg_status & STS_BUSY))
                reg_sector = byte;
            break;
        case 0x03: // 0x7F
            reg_data = byte;
            reg_status &= ~STS_DRQ;
            break;
    }
}

void FDC::frame(s32 clk){
    update(clk);
    last_clk -= clk;
}

void FDC::reset(){
    reg_command = 0x03;
    reg_system = 0x00;
    reg_track = 0xFF;
    reg_data = 0x00;
    last_clk = 0;
    command_clk = 0;
    delay_clk = 0;
    fdd[reg_system & SYS_DRIVE].step_dir = -1;
    fdd[reg_system & SYS_DRIVE].motor_clk = 0;
    reg_status = STS_BUSY;
}

void FDC::load_trd(int drive, const char *path){
    FDD &disk = fdd[drive & 0x03];
    DELETE_ARRAY(disk.data);
    FILE *file = fopen(path, "rb");
    fseek(file, 0, SEEK_END);
    disk.size = ftell(file);
    disk.data = new unsigned char[disk.size];
    fseek(file, 0, SEEK_SET);
    if (fread(disk.data, 1, disk.size, file) != disk.size)
        throw runtime_error("Read TRD");
    fclose(file);
}

void FDC::save_trd(int drive, const char *path){
    FDD &disk = fdd[drive & 0x03];
    if (!disk.data)
        throw runtime_error("Disk image is not exist");
    FILE *file = fopen(path, "wb");
    if (fwrite(disk.data, 1, disk.size, file) != disk.size)
        throw runtime_error("Write TRD");
    fclose(file);
}

void FDC::load_scl(int drive, const char *path){
    FDD &disk = fdd[drive & 0x03];
    DELETE_ARRAY(disk.data);
    disk.size = 655360;
    disk.data = new unsigned char[disk.size];
    memset(disk.data, 0x00, disk.size);
    FILE *file = fopen(path, "rb");
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    unsigned char *data = new unsigned char[size];
    fseek(file, 0, SEEK_SET);
    if (fread(data, 1, size, file) != size)
        throw runtime_error("Read SCL");
    fclose(file);
    SCL_Header *header = (SCL_Header*)data;
    if (memcmp(header->signature, "SINCLAIR", 8) || header->files > 128)
        throw runtime_error("SCL format");
    unsigned char *src = data + sizeof(SCL_Header);
    unsigned char *dst = disk.data;
    unsigned int del_files = 0, sectors = 0;
    for (int file = 0; file < header->files; file++){
        if (*src == 0x00) // Directory end.
            break;
        if (*src == 0x01) // Deleted file.
            del_files++;
        memcpy(dst, src, 0x0E);
        dst[0x0E] = sectors % 0x10;
        dst[0x0F] = sectors / 0x10 + 1;
        sectors += src[0x0D];
        src += 0x0E, dst += 0x10;
    }
    memcpy(disk.data + 0x100 * 0x10, src, sectors * 0x100);
    DELETE_ARRAY(data);
    dst = &disk.data[0x100 * 8];
    //Free space start at
    dst[0xE1] = sectors % 0x10;
    dst[0xE2] = sectors / 0x10 + 1;
    // FMT DD2S80T
    dst[0xE3] = 0x16;
    dst[0xE4] = header->files;
    // Free space amount
    dst[0xE5] = (2544 - sectors) % 0x100;
    dst[0xE6] = (2544 - sectors) / 0x100;
    // TRDOS ID
    dst[0xE7] = 0x10;
    dst[0xE8] = 0x00;
    dst[0xE9] = 0x00;
    // Just spaces
    memset(dst + 0xEA, ' ', 9);
    dst[0xF3] = 0x00;
    dst[0xF4] = del_files;
    // Label
    memcpy(dst + 0xF5, "Emulator", 8);
    dst[0xFD] = 0x00;
    dst[0xFE] = 0x00;
    dst[0xFF] = 0x00;
}
