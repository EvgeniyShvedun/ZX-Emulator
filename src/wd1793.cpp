#include "base.h"
#include <algorithm>

//#define DEBUG

using namespace std;


WD1793::WD1793(Board *p_board){
    this->p_board = p_board;
    for (int i = 0; i < 4; i++)
        drive[i].p_data = NULL;
    reset();
}

WD1793::~WD1793(){
    for (int i = 0; i < 4; i++)
        if (drive[i].p_data)
            DELETE_ARRAY(drive[i].p_data);
}

void WD1793::update(int clk){
    int steps, step_clk, bytes;
    clk -= last_clk;
    last_clk += clk;

    #ifdef DEBUG
        printf("Command %02x, status: %02x, head: %02x, reg_track: %02x, reg_sector: %02x, reg_data: %02x, motor on: %d, label: %d, clk: %d\n", reg_command, reg_status, drive[reg_select & SYS_DRIVE].track, reg_track, reg_sector, reg_data, drive[reg_select & SYS_DRIVE].motor_on_clk, drive[reg_select & SYS_DRIVE].index_label_clk, command_clk);
    #endif
    if (drive[reg_select & SYS_DRIVE].motor_on_clk >= MOTOR_ON_CLK)
        return;
    drive[reg_select & SYS_DRIVE].index_label_clk += clk;
    if (!(reg_command & 0x80)){ // Type I
        if (drive[reg_select & SYS_DRIVE].index_label_clk % DISK_TURN_CLK < INDEX_LABEL_CLK)
            reg_status |= ST_INDEX;
        else
            reg_status &= ~ST_INDEX;
    }
    if (!(reg_status & ST_BUSY)){
        drive[reg_select & SYS_DRIVE].motor_on_clk += clk;
        return;
    }
    command_clk += clk;

    switch ((reg_command >> 4) & 0x0F){
        case 0x00: // Restore
            if (drive[reg_select & SYS_DRIVE].track <= command_clk / step_time[reg_command & CMD_RATE]){
                reg_track = 0;
                reg_sector = 1;
                reg_status |= ST_TRACK0;
                reg_status &= ~ST_BUSY;
            }
            break;
        case 0x01: // Seek
    #ifdef DEBUG
            printf("SEEK reg_track: %02x, reg_data: %02x, reg_status: %02x, dir: %x\n", reg_track, reg_data, reg_status, drive[reg_select & SYS_DRIVE].step_dir);
    #endif
            steps = command_clk / step_time[reg_command & CMD_RATE];
            if (drive[reg_select & SYS_DRIVE].step_dir > 0){
                if (steps > reg_data - reg_track)
                    steps = reg_data - reg_track;
                drive[reg_select & SYS_DRIVE].track += steps;
                if (drive[reg_select & SYS_DRIVE].track > LAST_TRACK)
                    drive[reg_select & SYS_DRIVE].track = LAST_TRACK;
                reg_track += steps;
            }else{
                if (steps > reg_track - reg_data)
                    steps = reg_track - reg_data;
                drive[reg_select & SYS_DRIVE].track -= steps;
                if (drive[reg_select & SYS_DRIVE].track < 0)
                    drive[reg_select & SYS_DRIVE].track = 0;
                reg_track -= steps;
                if (!drive[reg_select & SYS_DRIVE].track)
                    reg_status |= ST_TRACK0;
            }
            command_clk -= steps * step_time[reg_command & CMD_RATE];
            if (reg_track == reg_data){
                if (reg_command & CMD_VERIFY)
                    if (reg_track != drive[reg_select & SYS_DRIVE].track)
                        reg_status |= ST_SEEK_ERR;
                reg_status &= ~ST_BUSY;
            }
            break;////////////////////////////////////////////////////////
        case 0x02: // Step
        case 0x03: // Step modify
        case 0x04: // Step forward
        case 0x05: // Step forward modify
        case 0x06: // Step backward
        case 0x07: // Step backward modify
            step_clk = step_time[reg_command & CMD_RATE];
            if (command_clk < step_clk)
                break;
            if (drive[reg_select & SYS_DRIVE].step_dir > 0){
                if (drive[reg_select & SYS_DRIVE].track < LAST_TRACK)
                    drive[reg_select & SYS_DRIVE].track++;
                if (reg_command & CMD_MODIFY)
                    reg_track++;
            }else{
                if (drive[reg_select & SYS_DRIVE].track > 0)
                    if (!--drive[reg_select & SYS_DRIVE].track)
                        reg_status |= ST_TRACK0;
                if (reg_command & CMD_MODIFY)
                    reg_track--;
            }
            if (reg_command & CMD_VERIFY)
                if (reg_track != drive[reg_select & SYS_DRIVE].track)
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
            if (drive[reg_select & SYS_DRIVE].p_data and bytes){
                reg_data = drive[reg_select & SYS_DRIVE].p_data[0x2000 * drive[reg_select & SYS_DRIVE].track + 0x1000 * (reg_select & SYS_HEAD ? 0 : 1) + (min(reg_sector - 1, LAST_SECTOR - 1) << 8) + data_idx - 1];
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
                        reg_data = drive[reg_select & SYS_DRIVE].track;
                        break;
                    case 0x01:
                        reg_data = reg_select & SYS_HEAD ? 1 : 0;
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
                reg_sector = drive[reg_select & SYS_DRIVE].track;
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

bool WD1793::io_wr(unsigned short addr, unsigned char byte, int clk){
    if (!p_board->trdos_active())
        return false;
    update(clk);
    if (addr & 0x80){
        reg_select = byte;
        if (!(byte & SYS_RESET))
            reset();
        return true;
    }
    #ifdef DEBUG
        printf("ADDR %x -> %x\n", (addr & 0xFF), byte);
    #endif
    switch((addr >> 5) & 0x03){
        case 0x00: // 1F
            if ((byte & 0xF0) == 0xD0){
                reset(); // Hard reset.
                break;
            }
            if (reg_status & ST_BUSY)
                break; // Not ready.
            reg_command = byte;
            command_clk = 0; 
            drive[reg_select & SYS_DRIVE].motor_on_clk = 0;
            switch((byte >> 4) & 0x0F){
                case 0x00: // Restore.
                    drive[reg_select & SYS_DRIVE].step_dir = -1;
                    reg_status = ST_BUSY | ST_HEAD_LOAD;
                    break;
                case 0x01: // Seek
                    drive[reg_select & SYS_DRIVE].step_dir = reg_track < reg_data ? 1 : -1;
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
                    drive[reg_select & SYS_DRIVE].step_dir = 1;
                    reg_status = ST_BUSY | ST_HEAD_LOAD;
                    break;
                case 0x06: // Step backward
                case 0x07: // Step backward modify
                    drive[reg_select & SYS_DRIVE].step_dir = -1;
                    reg_status = ST_BUSY | ST_HEAD_LOAD;
                    break;
                // Type 2
                case 0x08: // Read sector
                case 0x09: // Read sectors
                case 0x0A: // Write sector
                case 0x0B: // Write sectors
                    if (reg_track != drive[reg_select & SYS_DRIVE].track || reg_sector < 1 || reg_sector > LAST_SECTOR){
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
    return true;
}

void WD1793::frame(int clk){
    update(clk);
    last_clk -= clk;
}

void WD1793::reset(){
    for (int i = 0; i < 4; i++){
        drive[i].step_dir = -1;
        drive[i].track = 0;
    }
    data_idx = 0;
    command_clk = 0;
    reg_command = 0;
    reg_track = 0;
    reg_sector = 1;
    reg_data = 0;
    reg_status = 0;
    reg_select = SYS_INTRQ;
}

void WD1793::load_trd(int drive_idx, const char *p_path, bool writable){
    drive_idx &= 0x03;
    DELETE_ARRAY(drive[drive_idx].p_data);
    drive[drive_idx].p_data = new char[TRD_IMG_SIZE];
    drive[drive_idx].writable = writable;
    int fd = open(p_path, O_RDWR);
    if (fd < 0)
        throw runtime_error(string("Can't open disk image: ") + p_path);
    if (read(fd, drive[drive_idx].p_data, TRD_IMG_SIZE) != TRD_IMG_SIZE)
        throw;
    close(fd);
}

void WD1793::load_scl(int drive, const char *p_path){
}
