#include "base.h"
#include <algorithm>
#include "wd1793.h"

using namespace std;

//#define DEBUG

WD1793::WD1793(Board *p_board) : p_board(p_board) {
    for (int i = 0; i < 4; i++){
        drive[i].p_data = NULL;
        drive[i].index_label_clk = 0;
        drive[i].motor_on_clk = 0;
        drive[i].head_position = 0;
        drive[i].step_dir = -1;
        drive[i].head_select = 0;
        drive[i].head_loaded = false;
        drive[i].write_protect = true;
    }
    last_clk = 0;
    reset();
}

WD1793::~WD1793(){
    for (int i = 0; i < 4; i++)
        DELETE_ARRAY(drive[i].p_data);
}

void WD1793::update(int clk){
    int steps, step_clk, bytes;
    clk -= last_clk;
    last_clk += clk;
    if (p_drive->motor_on_clk >= MOTOR_ON_CLK)
        return;
    p_drive->index_label_clk += clk;
    if (!(reg_command & 0x80)){ // Type I
        if (p_drive->index_label_clk % DISK_TURN_CLK < INDEX_LABEL_CLK)
            reg_status |= ST_INDEX;
        else
            reg_status &= ~ST_INDEX;
    }
    if (!(reg_status & ST_BUSY)){
        p_drive->motor_on_clk += clk;
        return;
    }
    command_clk += clk;
    #ifdef DEBUG
        printf("Command %02x, status: %02x, head: %02x, reg_track: %02x, reg_sector: %02x, reg_data: %02x, clk: %d\n", reg_command, reg_status, p_drive->head_position, reg_track, reg_sector, reg_data, command_clk);
    #endif

    switch ((reg_command >> 4) & 0x0F){
        case 0x00: // Restore
            step_clk = step_time[reg_command & CMD_RATE] * Z80_FREQ / 1000;
            steps = min(command_clk / step_clk, p_drive->head_position);
            command_clk -= steps; 
            p_drive->head_position -= steps;
            if (p_drive->head_position > 0)
                break;
            reg_track = 0;
            reg_sector = 1;
            reg_status |= ST_TRACK0;
            reg_status &= ~ST_BUSY;
            break;
        case 0x01: // Seek
            //printf("SEEK reg_track: %02x, reg_data: %02x, reg_status: %02x\n", reg_track, reg_data, reg_status);
            step_clk = step_time[reg_command & CMD_RATE] * Z80_FREQ / 1000;
            if (p_drive->step_dir > 0){
                steps = min(command_clk / step_clk, reg_data - reg_track);
                p_drive->head_position += min(steps, LAST_TRACK - p_drive->head_position);
                if (reg_command & CMD_MODIFY)
                    reg_track += steps & 0xFF;
            }else{
                steps = min(command_clk / step_clk, reg_track - reg_data);
                p_drive->head_position -= min(steps, p_drive->head_position);
                if (reg_command & CMD_MODIFY)
                    reg_track -= steps & 0xFF;
            }
            command_clk -= steps * step_clk;
            if (!p_drive->head_position)
                reg_status |= ST_TRACK0;
            if (reg_track == reg_data){
                if (reg_command & CMD_VERIFY)
                    if (reg_track != p_drive->head_position)
                        reg_status |= ST_SEEK_ERR;
                reg_status &= ~ST_BUSY;
            }
            break;
        case 0x02: // Step
        case 0x03: // Step modify
        case 0x04: // Step forward
        case 0x05: // Step forward modify
        case 0x06: // Step backward
        case 0x07: // Step backward modify
            step_clk = step_time[reg_command & CMD_RATE] * Z80_FREQ / 1000;
            if (command_clk < step_clk)
                break;
            if (p_drive->step_dir > 0){
                if (p_drive->head_position < LAST_TRACK)
                    p_drive->head_position++;
                if (reg_command & CMD_MODIFY)
                    reg_track++;
            }else{
                if (p_drive->head_position > 0)
                    if (!--p_drive->head_position)
                        reg_status |= ST_TRACK0;
                if (reg_command & CMD_MODIFY)
                    reg_track--;
            }
            if (reg_command & CMD_VERIFY)
                if (reg_track != p_drive->head_position)
                    reg_status |= ST_SEEK_ERR;
            reg_status &= ~ST_BUSY;
            break;
        case 0x08: // Read sector
        case 0x09: // Read sectors
            if (command_clk < DRQ_CLK)
                break;
            if (reg_command & CMD_MULTI_SECTOR){
                bytes = min(command_clk / DRQ_CLK, 0x100 * (LAST_SECTOR - (reg_sector - 1)) - data_idx);
                reg_sector += data_idx >> 8;
                data_idx += bytes;
                data_idx %= 0x101;
            }else{
                bytes = min(command_clk / DRQ_CLK, 0x100 - data_idx);
                data_idx += bytes;
            }
            command_clk -= bytes * DRQ_CLK;
            if (bytes > 1 || reg_status & ST_DRQ){
                //printf("LOAD DATA bytes: %d, status: %x\n", bytes, reg_status);
                reg_status |= ST_DATA_ERR;
            }
            if (p_drive->p_data and bytes){
                reg_data = p_drive->p_data[0x2000 * p_drive->head_position + 0x1000 * p_drive->head_select + (min(reg_sector - 1, LAST_SECTOR - 1) << 8) + data_idx - 1];
                //printf("Read sector idx: %d, data: %02x", data_idx, reg_data);
                reg_status |= ST_DRQ;
            }else
                reg_status &= ~(ST_BUSY | ST_DRQ);
            break;
        case 0x0A: // Write sector
        case 0x0B: // Write sectors
            break;
        case 0x0C: // Read address
            if (command_clk < DRQ_CLK)
                break;
            bytes = min(command_clk / DRQ_CLK, 6 - data_idx);
            if (bytes > 1 || reg_status & ST_DRQ)
                reg_status |= ST_DATA_ERR;
            if (bytes){
                data_idx += bytes;
                command_clk -= bytes * DRQ_CLK;
                switch(data_idx - 1){
                    case 0x00:
                        reg_data = p_drive->head_position;
                        break;
                    case 0x01:
                        reg_data = p_drive->head_select;
                        break;
                    case 0x02: // Sector
                        reg_data = command_clk % (LAST_SECTOR + 1);
                        break;
                    case 0x03: // Sector size 1 = 0x100 bytes
                        reg_data = 1;
                        break;
                    case 0x04: // CRC
                        reg_data = 0x00;
                        break;
                    case 0x05:
                        reg_data = 0x00;
                        break;
                }
                reg_status |= ST_DRQ;
            }else{
                reg_sector = p_drive->head_position;
                reg_status &= ~(ST_BUSY | ST_DRQ);
            }
            break;
        case 0x0D: // Force interrupt
            reg_status &= ~(ST_BUSY | ST_DRQ);
            break;
        case 0x0E: // Read track
        case 0x0F: // Write track
            reg_status &= ~(ST_BUSY | ST_DRQ);
            break;
    }
}

bool WD1793::io_rd(unsigned short port, unsigned char *p_byte, int clk){
    if (!p_board->trdos_active())
        return false;
    update(clk);
    if (!(port & 0x80)){ // System port decode only by A8.
        switch ((port >> 5) & 0x03){ // All other by A8-A6.
            case 0x00:
                *p_byte = reg_status;
                break;
            case 0x01:
                *p_byte = reg_track;
                break;
            case 0x02:
                *p_byte = reg_sector;
                break;
            case 0x03:
                *p_byte = reg_data;
                reg_status &= ~ST_DRQ;
                break;
        }
    }else{
        //*p_byte &= ~0b11;
        *p_byte &= ~(SYS_INTRQ | SYS_DRQ);
        if ((reg_command & 0x80) && (reg_command >> 8) != 0x0D) // DRQ must be separated value
            if (reg_status & ST_DRQ)
                *p_byte |= SYS_DRQ;
        if (!(reg_status & ST_BUSY))
            *p_byte |= SYS_INTRQ;
    }
    #ifdef DEBUG
       printf("WD read %02x -> %02x\n", port & 0xFF, *p_byte);
    #endif
    return true;
}

bool WD1793::io_wr(unsigned short port, unsigned char byte, int clk){
    if (!p_board->trdos_active())
        return false;
    #ifdef DEBUG
        printf("WD write %02x -> %02x\n", port & 0xFF, byte);
    #endif
    update(clk);
    if (!(port & 0x80)){
        switch ((port >> 5) & 0x03){
            case 0x00: // 0x1F Command.
                if (byte >> 4 != 0x0D and reg_status & ST_BUSY)
                    break;
                if (byte >> 4 == 0x0F and byte & 0x08)
                    byte = 0xD0;
                reg_command = byte;
                command_clk = 0;
                //printf("START COMMAND: %02x\n", byte);
                p_drive->motor_on_clk = 0;
                switch (byte >> 4){
                    // Type 1
                    case 0x00: // Restore.
                        p_drive->step_dir = -1;
                        reg_status = ST_BUSY | ST_HEAD_LOAD;
                        break;
                    case 0x01: // Seek
                        p_drive->step_dir = reg_track < reg_data ? 1 : -1;
                        reg_status = ST_BUSY | ST_HEAD_LOAD;
                        //printf("Start seek status: %02x, busy: %02x, head_load: %02x\n", reg_status, ST_BUSY, ST_HEAD_LOAD);
                        break;
                    case 0x02: // Step
                        reg_status = ST_BUSY | ST_HEAD_LOAD;
                        break;
                    case 0x03: // Step modify
                        reg_status = ST_BUSY | ST_HEAD_LOAD;
                        break;
                    case 0x04: // Step forward
                    case 0x05: // Step forward modify
                        p_drive->step_dir = 1;
                        reg_status = ST_BUSY | ST_HEAD_LOAD;
                        break;
                    case 0x06: // Step backward
                    case 0x07: // Step backward modify
                        p_drive->step_dir = -1;
                        reg_status = ST_BUSY | ST_HEAD_LOAD;
                        break;
                    // Type 2
                    case 0x08: // Read sector
                    case 0x09: // Read sectors
                    case 0x0A: // Write sector
                    case 0x0B: // Write sectors
                        if (reg_track != p_drive->head_position || reg_sector < 1 || reg_sector > LAST_SECTOR){
                            reg_status |= ST_RNF;
                            break;
                        }
                        data_idx = 0;
                        reg_status = ST_BUSY;
                        break;
                    // Type 3
                    case 0x0C: // Read address
                        data_idx = 0;
                        reg_status = ST_BUSY;
                        break;
                    case 0x0D: // Force interrupt
                        reg_status = ST_BUSY;
                        break;
                    case 0x0E: // Read track
                    case 0x0F: // Write track
                        data_idx = 0;
                        reg_status = ST_BUSY;
                        break;
                }
                break;
            case 0x01: // 0x3F
                reg_track = byte;
                break;
            case 0x02: // 0x5F
                reg_sector = byte;
                break;
            case 0x03: // 0x7F
                reg_data = byte;
                reg_status &= ~ST_DRQ;
                break;
        }
    }else{ // 0xFF
        if (!(byte & SYS_RESET)){
            reg_track = 0x00;
            reg_sector = 0x01;
            reg_status &= ~(ST_BUSY | ST_DRQ);
        }
        p_drive = &drive[byte & SYS_DRIVE];
        p_drive->head_select = byte & SYS_HEAD ? 0 : 1;
    }
    return true;
}

void WD1793::reset(){
    for (int i = 0; i < 4; i++){
        drive[i].index_label_clk = 0;
        drive[i].motor_on_clk = 0;
        drive[i].head_position = 0;
        drive[i].step_dir = -1;
        drive[i].head_select = 0;
        drive[i].head_loaded = false;
    }
    p_drive = &drive[0];
    last_clk = 0;
    command_clk = 0;
    data_idx = 0;
    reg_command = 0;
    reg_track = 0;
    reg_sector = 1;
    reg_data = 0;
    reg_status = 0;
}

void WD1793::frame(int clk){
    update(clk);
    last_clk -= clk;
}

bool WD1793::open_trd(int drive_idx, const char *p_file_path, bool write_protect){
    if (drive_idx  >= 4)
        return false;
    DELETE_ARRAY(drive[drive_idx].p_data);
    drive[drive_idx].p_data = new unsigned char[TRD_DISK_SIZE];
    drive[drive_idx].write_protect = write_protect;
    int handle = open(p_file_path, write_protect ? O_RDONLY : O_RDWR);
    if (handle > 0){
        if (read(handle, drive[drive_idx].p_data, TRD_DISK_SIZE) == TRD_DISK_SIZE)
            return true;
        close(handle);
    }
    DELETE_ARRAY(drive[drive_idx].p_data);
    return false;
}

unsigned int* WD1793::status_pic(){
    FDC_STATUS status = p_drive->motor_on_clk < MOTOR_ON_CLK ? FDC_STATUS::MOTOR_ON : FDC_STATUS::UNKNOWN;
    if (reg_status & ST_BUSY){
        switch(reg_command >> 5){
            case 0x04:
                status = FDC_STATUS::READ_DATA;
                break;
            case 0x05:
                status = FDC_STATUS::WRITE_DATA;
                break;
        }
    }
    return p_status_pic[status];
}
