#define TRD_DISK_SIZE          655360

#define INDEX_LABEL_CLK        7000
#define DISK_TURN_CLK          700000
#define LAST_TRACK             79
#define LAST_SECTOR            0x10
#define MOTOR_ON_CLK           DISK_TURN_CLK*15
#define DRQ_CLK                80

// System write
#define SYS_DRIVE              0b00000011 // Drive select.
#define SYS_RESET              0b00000100 // Zero for reset controller.
#define SYS_HALT               0b00001000 // Zero for halt controller.
#define SYS_HEAD               0b00010000 // Zero for first head select.
// System read
#define SYS_DRQ                0b01000000 // Controller wait data i/o.
#define SYS_INTRQ              0b10000000 // Command complete.
// Command bits.
#define CMD_RATE               0b00000011 // Clock sync rate select.
#define CMD_VERIFY             0b00000100 // Track register and track record is not match.
#define CMD_HEAD_LOAD          0b00001000 // Load head on disk, turn on motor.
#define CMD_MODIFY             0b00010000 // Modify track register.
#define CMD_MULTI_SECTOR       0b00010000 // Data transfer to end of track.
// Type I-III
#define ST_BUSY                0b00000001 // Command is not complete.
#define ST_CRC_ERR             0b00001000 // Bad CRC.
#define ST_WR_PRT              0b01000000 // Write protected disk.
// Type I
#define ST_INDEX               0b00000010 // Index label sensor.
#define ST_TRACK0              0b00000100 // Zero in track record.
#define ST_SEEK_ERR            0b00010000 // Seek is not match with address record.
#define ST_HEAD_LOAD           0b00100000 // Head loaded and motor is on.
#define ST_NOT_READY           0b10000000 // Drive not ready.
// Type II-III
#define ST_DRQ                 0b00000010 // Controller wait data i/o.
#define ST_DATA_ERR            0b00000100 // Data i/o timeout.
#define ST_RNF                 0b00010000 // Record not found.
                                          //

enum FDC_STATUS {
    UNKNOWN, MOTOR_ON, READ_DATA, WRITE_DATA
};

struct Drive {
    unsigned char *p_data;
    int index_label_clk;
    int motor_on_clk;
    int head_position;
    char step_dir;
    int head_select;
    bool head_loaded;
    bool write_protect;
};


/*
 uint16_t update_crc(uint16_t crc, uint8_t value)
    {
        for (int i = 8; i < 16; ++i) {
            crc = (crc << 1) ^ ((((crc ^ (value << i)) & 0x8000) ? 0x1021 : 0));
        }
        return crc;
    }
Example usage: calculate the CRC for a whole buffer:

    uint16_t calculate_crc(uint8_t* buffer, size_t length)
    {
        uint16_t crc = 0xffff; // start value
        for (size_t i = 0; i < length; ++i) {
            crc = update_crc(crc, buffer[i]);
        }
        return crc;
    }*/


class WD1793 : public Device {
        const int step_time[4] = { 6, 12, 20, 30 };
    public:
        WD1793(Board *p_board);
        ~WD1793();
        bool open_trd(int drive, const char *p_file_path, bool write_protect = true);
        void update(int clk);
        void reset();
        void frame(int clk);
        bool io_wr(unsigned short port, unsigned char byte, int clk);
        bool io_rd(unsigned short port, unsigned char *p_byte, int clk);
        unsigned int* status_pic();
    protected:
        unsigned char reg_command;
        unsigned char reg_track;
        unsigned char reg_sector;
        unsigned char reg_data;
        unsigned char reg_status;

        int last_clk;
        int command_clk;
        int data_idx;
        Drive drive[4];
        Drive *p_drive;
        Board *p_board;
#include "wd1793_status_pic.h"
        unsigned int *p_status_pic[4] = {
            NULL,
            status_pic_motor_on,
            status_pic_read_data, 
            status_pic_write_data
        };
};
