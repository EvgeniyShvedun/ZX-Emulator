#define TRD_IMG_SIZE           655360
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
                                          //
// System read
#define SYS_DRQ                0b01000000 // Controller wait data i/o.
#define SYS_INTRQ              0b10000000 // Command complete.


// Command bits.
// Type I
#define CMD_RATE               0b00000011 // Clock sync rate select.
#define CMD_VERIFY             0b00000100 // Track register and track record is not match.
#define CMD_LOAD               0b00001000 // Type I - Load head at begin.
#define CMD_MODIFY             0b00010000 // Modify track register.

// Type III.
#define CMD_DELAY              0b00000100 // Type III commands 15ms delay.
#define CMD_MULTISEC           0b00010000 // Data transfer to end of track.
                                          //

// Status register.
#define ST_BUSY                0b00000001 // Command is not complete.
#define ST_CRC_ERR             0b00001000 // Bad CRC.
#define ST_WR_PRT              0b01000000 // Write protected disk.
// Type I
#define ST_INDEX               0b00000010 // Index label sensor.
#define ST_TRACK0              0b00000100 // Zero in track record.
#define ST_SEEK_ERR            0b00010000 // Seek is not match with address record.
#define ST_HEAD_LOADED         0b00100000 // Head loaded.
#define ST_NOT_READY           0b10000000 // Drive not ready.
// Type II-III
#define ST_DRQ                 0b00000010 // Controller wait data i/o.
#define ST_DATA_ERR            0b00000100 // Data i/o timeout.
#define ST_RNF                 0b00010000 // Record not found.

struct Drive {
    int motor_clk;
    char dirc;
    int track;
    char *p_data;
    bool writable;
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
    public:
        WD1793(Board *p_board);
        ~WD1793();
        void load_trd(int drive_idx, const char *p_path, bool writable = false);
        void load_scl(int drive_idx, const char *p_path);
        void update(int clk);

        bool io_wr(unsigned short addr, unsigned char byte, int clk);
        bool io_rd(unsigned short addr, unsigned char *p_byte, int clk);
        void frame(int clk);
        void reset();
    protected:
        int step_time[4] = {6 * Z80_FREQ / 1000, 12 * Z80_FREQ / 1000, 20 * Z80_FREQ / 1000, 30 * Z80_FREQ / 1000};
        unsigned char reg_command;
        unsigned char reg_track;
        unsigned char reg_sector;
        unsigned char reg_data;
        unsigned char reg_status;
        unsigned char reg_select;
        //char reg_dsr; // Internal
        int last_clk;
        int command_clk;
        int delay_clk;
        int data_idx;

        Drive drive[4];
        Board *p_board;
};
