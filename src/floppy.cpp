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
    fdd[0].data = fdd[1].data = fdd[2].data = fdd[3].data = NULL;
    reset();
}

FDC::~FDC(){
    DELETE_ARRAY(fdd[0].data);
    DELETE_ARRAY(fdd[1].data);
    DELETE_ARRAY(fdd[2].data);
    DELETE_ARRAY(fdd[3].data);
}

void FDC::update(s32 clk){
    clk -= last_clk;
    last_clk += clk;
    #ifdef DEBUG
        if (drive)
            printf("Command %02x, status: %02x, sys: %02x, fdd.track: %02x, reg_trk: %02x, reg_sec: %02x, reg_dat: %02x, step_dir: %02x, idx: %02x, time: %d, clk: %d\n", reg_command, reg_status, reg_system, drive->track, reg_track, reg_sector, reg_data, step_dir, data_idx, time, clk);
    #endif
    reg_status &= ~ST_NOT_READY;
    if (!drive->data){
        reg_status |= ST_NOT_READY;
        reg_status &= ~ST_BUSY;
        return;
    }
    if (time - cmd_time > IDLE_WIDTH){
        hld = drive->hlt = false;
    }else{
        time += clk;
        if (reg_command < 0x80 || (reg_command & 0xF0) == 0xD0){ // Type I or Force interrupt.
            reg_status &= ~(ST_HEAD_LOADED | ST_TRACK0 | ST_INDEX);
            if (hld && drive->hlt)
                reg_status |= ST_HEAD_LOADED;
            if (!drive->track)
                reg_status |= ST_TRACK0;
            if (time % DISK_TURN_PERIOD < INDEX_PULSE_WIDTH)
                reg_status |= ST_INDEX;
        }
        if (reg_status & ST_BUSY){
            switch (reg_command >> 4){
                case 0x00: // Restore
                case 0x01: // Seek
                case 0x02: // Step
                case 0x03: // Step modify
                case 0x04: // Step forward
                case 0x05: // Step forward modify
                case 0x06: // Step backward
                case 0x07: // Step backward modify
                    if (step_cnt){
                        int steps = MIN(step_cnt, (time - cmd_time) / step_rate[reg_command & CM_STEP_RATE]);
                        if (!steps)
                            break;
                        if (reg_command & CM_TRACK_MODIFY)
                            reg_track += steps*step_dir;
                        if (drive->track + steps*step_dir <= 0){
                            step_cnt = steps = drive->track;
                            reg_track = 0;
                        }
                        drive->track = MIN(drive->track + steps*step_dir, 79);
                        cmd_time += steps*step_rate[reg_command & CM_STEP_RATE];
                        step_cnt -= steps;
                        if (step_cnt)
                            break;
                    }
                    if (reg_command & CM_VERIFY){
                        if (!hld){
                            if (time - cmd_time < DELAY_15MS)
                                break;
                            cmd_time += DELAY_15MS;
                            hld = true;
                        }
                        if (!drive->hlt){
                            if (time - cmd_time < HLT_TIME)
                                break;
                            drive->hlt = true;
                            cmd_time += HLT_TIME;
                        }
                        if (reg_track != drive->track){
                            if (time - cmd_time < DISK_TURN_PERIOD*5)
                                break;
                            cmd_time += DISK_TURN_PERIOD*5;
                            reg_status |= ST_SEEK_ERROR;
                        }
                    }
                    reg_status &= ~ST_BUSY;
                    break;
                case 0x08: // Read sector
                case 0x09: // Read sectors
                case 0x0A: // Write sector
                case 0x0B: // Write sectors
                case 0x0C: // Read address
                    if (reg_command & CM_DELAY){
                        if (time - cmd_time < DELAY_15MS)
                            break;
                        cmd_time += DELAY_15MS;
                        reg_command &= ~CM_DELAY;
                    }
                    if (!drive->hlt){
                        if (time - cmd_time < HLT_TIME)
                            break;
                        cmd_time += HLT_TIME;
                        drive->hlt = true;
                    }
                    /*
                    if (!data_idx && (reg_track != drive->track || reg_sector < 1 || reg_sector > 0x11)){
                        if (time - cmd_time < DISK_TURN_PERIOD*5)
                            break;
                        cmd_time += DISK_TURN_PERIOD*5;
                        reg_status |= ST_RNF;
                        reg_status &= ~ST_BUSY;
                        break;
                    }*/
                    if (reg_status & ST_DRQ){
                        if (time - cmd_time < DRQ_WIDTH)
                            break;
                        //printf("LOST DATA time: %d, cmd_time: %d, elapsed: %d, DRQ PERIOD: %d, DRQ_WIDTH: %d\n", time, cmd_time, time - cmd_time, DRQ_PERIOD, DRQ_WIDTH);
                        cmd_time += DRQ_WIDTH;
                        reg_status |= ST_LOST_DATA;
                        reg_status &= ~(ST_DRQ | ST_BUSY);
                        break;
                    }
                    if (time - cmd_time < DRQ_PERIOD)
                        break;
                    //printf("Set DRQ time: %d, cmd_time: %d, elapsed: %d\n", time, cmd_time, time - cmd_time);
                    cmd_time += DRQ_PERIOD;
                    reg_status |= ST_DRQ;
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
        switch ((port >> 5) & 0x03){ // Other ports by A8-A6.
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
                if (reg_status & ST_DRQ){
                    switch (reg_command >> 4){
                        case 0x08: // Read sector
                        case 0x09: // Read sectors
                        case 0x0E: // Read track
                            reg_data = drive->data[drive->track * 0x2000 + (reg_system & SS_HEAD ? 0 : 0x1000) + (reg_sector - 1) * 0x100 + data_idx];
                            if (++data_idx >= 0x100){
                                if (!(reg_command & CM_MULTISEC) || reg_sector >= 0x11){
                                    reg_status &= ~ST_BUSY;
                                }else{
                                    reg_sector++;
                                    data_idx = 0;
                                }
                            }
                            reg_status &= ~ST_DRQ;
                            break;
                        case 0x0C: // Read address
                            //printf("Read time: %d, cmd_time: %d, elapsed: %d\n", time, cmd_time, time - cmd_time);
                            switch (data_idx){
                                case 0x00:
                                    reg_data = drive->track;
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
                            }
                            if (++data_idx >= 6){
                                reg_sector = drive->track;
                                reg_status &= ~ST_BUSY;
                            }
                            reg_status &= ~ST_DRQ;
                            break;
                    }
                }
                *byte = reg_data;
                break;
        }
    }
#ifdef DEBUG
    printf("RD %02x -> %02x\n", port & 0xFF, *byte);
#endif
}

void FDC::write(u16 port, u8 byte, s32 clk){
#ifdef DEBUG
    printf("WR %x -> %x\n", (port & 0xFF), byte);
#endif
    update(clk);
    if (port & 0x80){
        reg_system = byte;
        drive = &fdd[reg_system & SS_DRIVE];
        if (!(reg_system & SS_RESET))
            reset();
    }else{
        switch ((port >> 5) & 0x3){
            case 0x00: // 0x1F
                if ((byte & 0xF0) == 0xF0 && byte & 0x08)
                    break;
                //if ((byte & 0xF0) == 0xF0 && byte & 0x08) // The unknown bits is threated as "Forced Interrupt" command.
                //    byte &= 0xD0;
                if (reg_status & ST_BUSY && (byte & 0xF0) != 0xD0)
                    break;
                time = cmd_time = 0;
                reg_command = byte;
#ifdef DEBUG
                printf("COMMAND: %02x\n", byte);
#endif
                switch (byte >> 4){
                    case 0x00: // Restore.
                        reg_track = 0xFF;
                        reg_data = 0x00;
                        step_cnt = ABS(reg_data - reg_track);
                        step_dir = reg_data > reg_track ? 1 : -1;
                        hld = byte & CM_HEAD_LOAD;
                        reg_status &= ~ST_SEEK_ERROR;
                        reg_status |= ST_BUSY;
                        break;
                    case 0x01: // Seek
                        step_cnt = ABS(reg_data - reg_track);
                        step_dir = reg_data > reg_track ? 1 : -1;
                        hld = byte & CM_HEAD_LOAD;
                        reg_status &= ~ST_SEEK_ERROR;
                        reg_status |= ST_BUSY;
                        break;
                    case 0x02: // Step
                    case 0x03: // Step modify
                        step_cnt = 1;
                        hld = byte & CM_HEAD_LOAD;
                        reg_status &= ~ST_SEEK_ERROR;
                        reg_status |= ST_BUSY;
                        break;
                    case 0x04: // Step forward
                    case 0x05: // Step forward modify
                        step_cnt = 1;
                        step_dir = 1;
                        hld = byte & CM_HEAD_LOAD;
                        reg_status &= ~ST_SEEK_ERROR;
                        reg_status |= ST_BUSY;
                        break;
                    case 0x06: // Step backward
                    case 0x07: // Step backward modify
                        step_dir = -1;
                        step_cnt = 1;
                        hld = byte & CM_HEAD_LOAD;
                        reg_status &= ~ST_SEEK_ERROR;
                        reg_status |= ST_BUSY;
                        break;
                    case 0x08: // Read sector
                    case 0x09: // Read sectors
                    case 0x0C: // Read address
                    case 0x0A: // Write sector
                    case 0x0B: // Write sectors
                    case 0x0E: // Read track
                    case 0x0F: // Write track
                        hld = true;
                        data_idx = 0;
                        reg_status &= ~(ST_WRITE_PROTECT | ST_WRITE_FAULT | ST_RNF | ST_LOST_DATA | ST_DRQ);
                        reg_status |= ST_BUSY;
                        break;
                    case 0x0D: // Force interrupt
                        reg_status &= ~(ST_SEEK_ERROR | ST_BUSY);
                        break;
                }
                break;
            case 0x01: // 0x3F
                if (reg_status & ST_BUSY)
                    break;
                reg_track = byte;
                break;
            case 0x02: // 0x5F
                if (reg_status & ST_BUSY)
                    break;
                reg_sector = byte;
                break;
            case 0x03: // 0x7F
                reg_data = byte;
                if (reg_status & ST_DRQ){
                    switch (reg_command >> 4){
                        case 0x0A: // Write sector
                        case 0x0B: // Write sectors
                        case 0x0F: // Write track
                            drive->data[drive->track * 0x2000 + (reg_system & SS_HEAD ? 0 : 0x1000) + (reg_sector - 1) * 0x100 + data_idx] = reg_data;
                            if (++data_idx >= 0x100){
                                if (!(reg_command & CM_MULTISEC) || reg_sector >= 0x11){
                                    cmd_time = time;
                                    reg_status &= ~ST_BUSY;
                                }else{
                                    reg_sector++;
                                    data_idx = 0;
                                }
                            }
                            reg_status &= ~ST_DRQ;
                            break;
                    }
                }
                break;
        }
    }
}

void FDC::frame(s32 clk){
    update(clk);
    last_clk -= clk;
}

void FDC::reset(){
    reg_system = SS_RESET | SS_HALT;
    reg_sector = 1;
    reg_track = 0xFF;
    reg_data = 0x00;
    reg_command = 0x03;
    step_cnt = 0xFF;
    step_dir = -1;;
    hld = false;
    reg_status &= ~(ST_HEAD_LOADED | ST_CRC_ERROR);
    reg_status |= ST_BUSY;
}

void FDC::load_trd(int drive_id, const char *path, bool write_protect){
    drive = &fdd[drive_id & 0x03];
    DELETE_ARRAY(drive->data);
    FILE *file = fopen(path, "rb");
    fseek(file, 0, SEEK_END);
    drive->size = ftell(file);
    drive->data = new unsigned char[drive->size];
    fseek(file, 0, SEEK_SET);
    if (fread(drive->data, 1, drive->size, file) != drive->size)
        throw std::runtime_error("Read TRD");
    fclose(file);
    drive->wprt = write_protect;
}

void FDC::save_trd(int drive_id, const char *path){
    drive = &fdd[drive_id & 0x03];
    if (!drive->data)
        throw std::runtime_error("Disk image is not exist");
    FILE *file = fopen(path, "wb");
    if (fwrite(drive->data, 1, drive->size, file) != drive->size)
        throw std::runtime_error("Write TRD");
    fclose(file);
}

void FDC::load_scl(int drive_id, const char *path, bool write_protect){
    drive = &fdd[drive_id & 0x03];
    DELETE_ARRAY(drive->data);
    drive->size = 655360;
    drive->data = new u8[drive->size];
    memset(drive->data, 0x00, drive->size);
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
    u8 *dst = drive->data;
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
    memcpy(drive->data + 0x100 * 0x10, src, sectors * 0x100);
    DELETE_ARRAY(data);
    dst = &drive->data[0x100 * 8];
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
    drive->wprt = write_protect;
}
