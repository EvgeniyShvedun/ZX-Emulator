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
    int steps, bytes;
    clk -= last_clk;
    last_clk += clk;

    #ifdef DEBUG
        printf("Command %02x, status: %02x, head: %d, reg_track: %d, reg_sector: %d, reg_data: %d, dirc: %d, motor_clk: %d, clk: %d\n", reg_command, reg_status, drive[reg_select & SYS_DRIVE].track, reg_track, reg_sector, reg_data, drive[reg_select & SYS_DRIVE].dirc, drive[reg_select & SYS_DRIVE].motor_clk, command_clk);
    #endif
    if (drive[reg_select & SYS_DRIVE].motor_clk > MOTOR_ON_CLK)
        return;
    drive[reg_select & SYS_DRIVE].motor_clk += clk;
    if (!(reg_command & 0x80)){
        if (drive[reg_select & SYS_DRIVE].motor_clk % DISK_TURN_CLK < INDEX_LABEL_CLK) // Type I
            reg_status |= ST_INDEX;
        else
            reg_status &= ~ST_INDEX;
    }
    if (!(reg_status & ST_BUSY))
        return;
    command_clk += clk;

    //////////////////////////////
    if ((reg_command & 0x80) && delay_clk && (reg_command >> 0x04) != 0xD){
        if (command_clk < delay_clk)
            return;
        command_clk -= delay_clk;
        delay_clk = 0;
    }

    switch ((reg_command >> 4) & 0x0F){
        case 0x00: // Restore
             /*
             if (drive[reg_select & SYS_DRIVE].track <= command_clk / step_time[reg_command & CMD_RATE]){
                drive[reg_select & SYS_DRIVE].track = 0x00;
                reg_track = 0;
                reg_sector = 1;
                reg_status |= ST_TRACK0;
                reg_status &= ~ST_BUSY;
            }
            break;
            */
        case 0x01: // Seek
            if (reg_track == reg_data || (drive[reg_select & SYS_DRIVE].dirc == -1 && !drive[reg_select & SYS_DRIVE].track)){
                if (!drive[reg_select & SYS_DRIVE].track){
                    reg_status |= ST_TRACK0;
                    reg_track = 0x00;
                }
                if (reg_command & CMD_VERIFY)
                    if (reg_track != drive[reg_select & SYS_DRIVE].track)
                        reg_status |= ST_SEEK_ERR;
                reg_status &= ~ST_BUSY;
                break;
            }
            steps = command_clk / step_time[reg_command & CMD_RATE];
            if (!steps)
                break;
            if (drive[reg_select & SYS_DRIVE].dirc > 0){
                if (steps > reg_data - reg_track)
                    steps = reg_data - reg_track;
                drive[reg_select & SYS_DRIVE].track += steps;
                if (drive[reg_select & SYS_DRIVE].track > LAST_TRACK)
                    drive[reg_select & SYS_DRIVE].track = LAST_TRACK;
                if (reg_command & CMD_MODIFY)
                    reg_track += steps;
            }else{
                steps = min(reg_track - reg_data, min(steps, drive[reg_select & SYS_DRIVE].track));
                drive[reg_select & SYS_DRIVE].track -= steps;
                if (reg_command & CMD_MODIFY)
                    reg_track -= steps;
            }
            command_clk -= steps * step_time[reg_command & CMD_RATE];
            break;
        case 0x02: // Step
        case 0x03: // Step modify
        case 0x04: // Step forward
        case 0x05: // Step forward modify
        case 0x06: // Step backward
        case 0x07: // Step backward modify
            if (command_clk < step_time[reg_command & CMD_RATE])
                break;
            if (drive[reg_select & SYS_DRIVE].dirc > 0){
                if (reg_command & CMD_MODIFY)
                    reg_track++;
                if (drive[reg_select & SYS_DRIVE].track < LAST_TRACK)
                    drive[reg_select & SYS_DRIVE].track++;
            }else{
                if (reg_command & CMD_MODIFY)
                    reg_track--;
                if (drive[reg_select & SYS_DRIVE].track > 0)
                    drive[reg_select & SYS_DRIVE].track--;
            }
            if (!drive[reg_select & SYS_DRIVE].track){
                reg_track = 0x00;
                reg_status |= ST_TRACK0;
            }
            if (reg_command & CMD_VERIFY && reg_track != drive[reg_select & SYS_DRIVE].track)
                reg_status |= ST_SEEK_ERR;
            reg_status &= ~ST_BUSY;
            break;
        case 0x08: // Read sector
        case 0x09: // Read sectors
            if (command_clk < DRQ_CLK)
                break;
            if (reg_command & CMD_MULTISEC){
                bytes = min(command_clk / DRQ_CLK, 0x100 * (LAST_SECTOR - (reg_sector - 1)) - data_idx);
                reg_sector += data_idx >> 8;
                data_idx += bytes;
                data_idx %= 0x101;
            }else{
                bytes = min(command_clk / DRQ_CLK, 0x100 - data_idx);
                data_idx += bytes;
            }
            command_clk -= bytes * DRQ_CLK;
            if (bytes > 1 || reg_status & ST_DRQ)
                reg_status |= ST_DATA_ERR;
            if (drive[reg_select & SYS_DRIVE].p_data && bytes){
                reg_data = drive[reg_select & SYS_DRIVE].p_data[0x2000 * drive[reg_select & SYS_DRIVE].track + 0x1000 * (reg_select & SYS_HEAD ? 0 : 1) + (min(reg_sector - 1, LAST_SECTOR - 1) << 8) + data_idx - 1];
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
            /*
            if (!(reg_command & 0x0F)){
                reg_status = 0x00;
            else{
                if (reg_command & 0x04){
                    if (drive[reg_select & SYS_DRIVE].index_label_clk % DISK_TURN_CLK < INDEX_LABEL_CLK)
                        reg_status = 0x00;
                }else{
                    if (reg_status & 0x08)
                        reg_status &= ST_BUSY;
                }
            }*/
            reg_status = 0x00;
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
        *p_byte &= ~(SYS_INTRQ | SYS_DRQ);
        if ((reg_command & 0x80)){
            if ((reg_command >> 4) != 0x0D){
                if (reg_status & ST_DRQ) // Data transfer command.
                    *p_byte |= SYS_DRQ;
            }
        }//else{
            if (!(reg_status & ST_BUSY))
                *p_byte |= SYS_INTRQ;
        //}
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
        printf("WD write %x -> %x\n", (port & 0xFF), byte);
    #endif
    update(clk);
    if (port & 0x80){
        reg_select = byte;
        if (!(byte & SYS_RESET))
            reset();
        return true;
    }
    switch((port >> 5) & 0x03){
        case 0x00: // 1F
            if ((byte & 0xF0) == 0xF0 && byte & 0x08) // A non-existent bit combination is treated as a "Forced Interrupt".
                byte &= 0xD0; 
            if ((byte & 0xF0) != 0xD0 && reg_status & ST_BUSY)
                break; // Not ready.
            command_clk = 0; 
            delay_clk = 0;
            reg_command = byte;
            reg_status = ST_BUSY;
            switch((byte >> 4) & 0x0F){
                case 0x00: // Restore.
                    reg_track = 0xFF;
                    reg_data = 0x00;
                    drive[reg_select & SYS_DRIVE].dirc = -1;
                    drive[reg_select & SYS_DRIVE].motor_clk %= DISK_TURN_CLK;
                    break;
                case 0x01: // Seek
                    drive[reg_select & SYS_DRIVE].dirc = reg_data > reg_track ? 1 : -1;
                    drive[reg_select & SYS_DRIVE].motor_clk %= DISK_TURN_CLK;
                    break;
                case 0x02: // Step
                case 0x03: // Step modify
                    break;
                case 0x04: // Step forward
                case 0x05: // Step forward modify
                    drive[reg_select & SYS_DRIVE].dirc = 1;
                    drive[reg_select & SYS_DRIVE].motor_clk %= DISK_TURN_CLK;
                    break;
                case 0x06: // Step backward
                case 0x07: // Step backward modify
                    drive[reg_select & SYS_DRIVE].dirc = -1;
                    drive[reg_select & SYS_DRIVE].motor_clk %= DISK_TURN_CLK;
                    break;
                // Type 2
                case 0x08: // Read sector
                case 0x09: // Read sectors
                case 0x0A: // Write sector
                case 0x0B: // Write sectors
                    if (reg_track != drive[reg_select & SYS_DRIVE].track || reg_sector < 1 || reg_sector > LAST_SECTOR){
                        reg_status |= ST_RNF;
                        reg_status &= ~ST_BUSY;
                        break;
                    }
                    data_idx = 0;
                    break;
                // Type 3
                case 0x0C: // Read address
                    data_idx = 0;
                    break;
                case 0x0D: // Force interrupt
                    if (!(reg_command & 0x0F))

                    break;
                case 0x0E: // Read track
                case 0x0F: // Write track
                    data_idx = 0;
                    reg_status = ST_BUSY;
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
    reg_command = 0x03;
    reg_select = 0x00;
    reg_track = 0xFF;
    reg_data = 0x00;
    command_clk = 0;
    drive[reg_select & SYS_DRIVE].dirc = -1;
    drive[reg_select & SYS_DRIVE].motor_clk = 0;
    reg_status = ST_BUSY;

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
