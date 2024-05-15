#include <cstddef>
#include <stdio.h>
#include <string.h>
#include <stdexcept>
#include "types.h"
#include "utils.h"
#include "config.h"
#include "device.h"
#include "memory.h"
#include "ula.h"
#include "z80.h"
#include "floppy.h"

//#define DEBUG

FDC::FDC(){
    drives[0].data = drives[1].data = drives[2].data = drives[3].data = NULL;
    reset();
}

FDC::~FDC(){
    DELETE_ARRAY(drives[0].data);
    DELETE_ARRAY(drives[1].data);
    DELETE_ARRAY(drives[2].data);
    DELETE_ARRAY(drives[3].data);
    
}

void FDC::update(s32 clk){
    FDD &drive = drives[reg_system & SS_DRIVE];
    clk -= last_clk;
    last_clk += clk;
    #ifdef DEBUG
        printf("Command %02x, status: %02x, fdd.track: %d, reg_trk: %d, reg_sec: %d, reg_dat: %d, step_dir: %d, time: %d, motor: %d, com_clk: %d, clk: %d\n", reg_command, reg_status, drive.track, reg_track, reg_sector, reg_data, drive.step_dir, time, (time - drive.motor) < MOTOR_ON_WHILE ? time - drive.motor : 0, time-command_time, clk);
    #endif
    if (time - drive.motor < MOTOR_ON_WHILE){
        time += clk;
        if (drive.data && !(reg_command & 0x80)){
            if ((time - drive.motor) % DISK_TURN_TIME < INDEX_PERIOD)
                reg_status |= ST_INDEX;
            else
                reg_status &= ~ST_INDEX;
        }
        if (reg_status & ST_BUSY){
            //if (command_clk < delay_clk && (reg_command & 0x80) && (reg_command & 0xF0) != 0xD0)
            //    return;
            //command_clk -= delay_clk;
            //delay_clk = 0;
            int steps;
            switch (reg_command >> 4){
                case 0x00: // Restore
                case 0x01: // Seek
                    if (reg_track != reg_data){
                        steps = std::abs(reg_track - reg_data);
                        if ((time - command_time) / step_rate[reg_command & CM_STEP_RATE] < steps)
                            break;
                        drive.track = MIN(MAX(drive.track + steps*drive.step_dir, 0), 79);
                        if (reg_command & CM_TRACK_MODIFY)
                            reg_track += steps * drive.step_dir;
                    }
                    if (!drive.track){
                        reg_track = 0;
                        reg_status |= ST_TRACK0;
                    }
                    if (reg_command & CM_VERIFY){
                        reg_status |= ST_HEAD_LOAD;
                        if (reg_track != drive.track)
                            reg_status |= ST_SEEK_ERROR;
                    }
                    reg_status &= ~ST_BUSY;
                    break;
                case 0x02: // Step
                case 0x03: // Step modify
                case 0x04: // Step forward
                case 0x05: // Step forward modify
                case 0x06: // Step backward
                case 0x07: // Step backward modify
                    if ((time - command_time) / step_rate[reg_command & CM_STEP_RATE] < 1)
                        break;
                    drive.track = MIN(MAX(drive.track + 1*drive.step_dir, 0), 79);
                    if (reg_command & CM_TRACK_MODIFY)
                        reg_track += 1* drive.step_dir;
                    if (!drive.track){
                        reg_track = 0; 
                        reg_status |= ST_TRACK0;
                    }
                    if (reg_command & CM_VERIFY){
                        reg_status |= ST_HEAD_LOAD;
                        if (reg_track != drive.track)
                            reg_status |= ST_SEEK_ERROR;
                    }
                    reg_status &= ~ST_BUSY;
                    break;
                case 0x08: // Read sector
                case 0x09: // Read sectors
                    if (time - command_time < DRQ_TIME)
                        break;
                    if (time - command_time < DRQ_TIME*2 && !(reg_status & ST_DRQ)){
                        if (data_idx <= 0x100){
                            reg_data = drive.data[drive.track * 0x2000 + (reg_system & SS_HEAD ? 0 : 0x1000) + (reg_sector - 1) * 0x100 + data_idx++];
                            reg_status |= ST_DRQ;
                        }else{
                            reg_status &= ~(ST_DRQ | ST_BUSY);
                        }
                    }else{
                        reg_status |= ST_DATA_LOST;
                        reg_status &= ~(ST_DRQ | ST_BUSY);
                    }
                    command_time = time;
                    break;
                case 0x0A: // Write sector
                case 0x0B: // Write sectors
                    break;
                case 0x0C: // Read address
                    if (time - command_time < DRQ_TIME)
                        break;
                    if (time - command_time < DRQ_TIME*2 && !(reg_status & ST_DRQ)){
                        switch (data_idx++){
                            case 0x00:
                                reg_data = drive.track;
                                break;
                            case 0x01:
                                reg_data = reg_system & SS_HEAD ? 1 : 0;
                                break;
                            case 0x02:
                                reg_data = time % 0x10 + 1;
                                break;
                            case 0x03: // Sector size = 256b
                                reg_data = 1;
                                break;
                            case 0x04: // CRC
                                reg_data = time % 0x100;
                                break;
                            case 0x05:
                                reg_data = time % 0x100;
                                break;
                            case 0x06: // Command complete.
                                reg_sector = drive.track;
                                reg_status &= ~(ST_BUSY | ST_DRQ);
                                break;
                        }
                    }else{
                        reg_status |= ST_DATA_LOST;
                        reg_status &= ~(ST_BUSY | ST_DRQ);
                    }
                    command_time = time;
                    break;
                case 0x0D: // Force interrupt
                    break;
                case 0x0E: // Read track
                case 0x0F: // Write track
                    reg_status &= ~(ST_BUSY | ST_DRQ);
                    break;
            }
        }
    }
}

void FDC::read(u16 port, u8 *byte, s32 clk){
    update(clk);
    if (port & 0x80){ // System port decodes using A8 only.
        *byte &= ~(SS_INTRQ | SS_DRQ);
        if ((reg_command & 0x80) && (reg_command & 0xF0) != 0xD0 && reg_status & ST_DRQ)
            *byte |= SS_DRQ;
        if (!(reg_status & ST_BUSY))
            *byte |= SS_INTRQ;
    }else{
        switch ((port >> 5) & 0x03){    // All other A8-A6.
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
                reg_status &= ~ST_DRQ;
                break;
        }
    }
#ifdef DEBUG
    printf("%02x -> %02x\n", port & 0xFF, *byte);
#endif
}

void FDC::write(u16 port, u8 byte, s32 clk){
    FDD &drive = drives[byte & SS_DRIVE];
#ifdef DEBUG
    printf("%x -> %x\n", (port & 0xFF), byte);
#endif
    update(clk);
    if (port & 0x80){
        reg_system = byte;
        if (!(reg_system & SS_RESET)){
            reg_command = 0;
            reg_status = ST_BUSY;
        }
    }
    switch((port >> 5) & 0x03){
        case 0x00: // 1F
            if ((byte & 0xF0) == 0xF0 && byte & 0x08) // The unknown bits is threated as "Forced Interrupt" command.
                byte &= 0xD0; 
            if ((byte & 0xF0) != 0xD0 && reg_status & ST_BUSY)
                break;
            //delay_clk = (byte & (0x80 | CM_DELAY)) && ((byte & 0xF0) != 0xD0) ? 15 * Z80_FREQ / 1000 : 0;
            // drive.motor_clk %= DISK_CLK;
            if (time - drive.motor > MOTOR_ON_WHILE)
                drive.motor = time = 0;
            else
                drive.motor %= DISK_TURN_TIME;
            command_time = time;
            reg_command = byte;
            reg_status = ST_BUSY;
            switch((byte >> 4) & 0x0F){
                case 0x00: // Restore.
                    reg_track = 0xFF;
                    reg_data = 0x00;
                    drive.step_dir = -1;
                    break;
                case 0x01: // Seek
                    drive.step_dir = reg_data > reg_track ? 1 : -1;
                    break;
                case 0x02: // Step
                case 0x03: // Step modify
                    break;
                case 0x04: // Step forward
                case 0x05: // Step forward modify
                    drive.step_dir = 1;
                    break;
                case 0x06: // Step backward
                case 0x07: // Step backward modify
                    drive.step_dir = -1;
                    break;
                // Type II
                case 0x0A: // Write sector
                case 0x0B: // Write sectors
                    reg_status |= ST_WRITE_PROTECT; /////////
                    break;
                case 0x08: // Read sector
                case 0x09: // Read sectors
                    if (reg_track != drive.track || reg_sector > 0x10){
                        reg_status |= ST_RNF;
                        reg_status &= ~ST_BUSY;
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
                    reg_status &= ~ST_BUSY;
                    break;
            }
            break;
        case 0x01: // 0x3F
            if (!(reg_status & ST_BUSY))
                reg_track = byte;
            break;
        case 0x02: // 0x5F
            if (!(reg_status & ST_BUSY))
                reg_sector = byte;
            break;
        case 0x03: // 0x7F
            reg_data = byte;
            reg_status &= ~ST_DRQ;
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
    command_time = 0;
    //delay_clk = 0;
    reg_status = ST_BUSY;
}

void FDC::load_trd(int drive_id, const char *path){
    FDD &drive = drives[drive_id & 0x03];
    DELETE_ARRAY(drive.data);
    FILE *file = fopen(path, "rb");
    fseek(file, 0, SEEK_END);
    drive.size = ftell(file);
    drive.data = new unsigned char[drive.size];
    fseek(file, 0, SEEK_SET);
    if (fread(drive.data, 1, drive.size, file) != drive.size)
        throw std::runtime_error("Read TRD");
    fclose(file);
}

void FDC::save_trd(int drive_id, const char *path){
    FDD &drive = drives[drive_id & 0x03];
    if (!drive.data)
        throw std::runtime_error("Disk image is not exist");
    FILE *file = fopen(path, "wb");
    if (fwrite(drive.data, 1, drive.size, file) != drive.size)
        throw std::runtime_error("Write TRD");
    fclose(file);
}

void FDC::load_scl(int drive_id, const char *path){
    FDD &drive = drives[drive_id & 0x03];
    DELETE_ARRAY(drive.data);
    drive.size = 655360;
    drive.data = new u8[drive.size];
    memset(drive.data, 0x00, drive.size);
    FILE *file = fopen(path, "rb");
    fseek(file, 0, SEEK_END);
    size_t size = ftell(file);
    u8 *data = new u8[size];
    fseek(file, 0, SEEK_SET);
    if (fread(data, 1, size, file) != size)
        std::runtime_error("Read SCL");
    fclose(file);
    SCL_Header *header = (SCL_Header*)data;
    if (memcmp(header->signature, "SINCLAIR", 8) || header->files > 128)
        throw std::runtime_error("SCL format");
    u8 *src = data + sizeof(SCL_Header);
    u8 *dst = drive.data;
    u8 del_files = 0, sectors = 0;
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
    memcpy(drive.data + 0x100 * 0x10, src, sectors * 0x100);
    DELETE_ARRAY(data);
    dst = &drive.data[0x100 * 8];
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
