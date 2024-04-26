// Floppy disk controller FD179X

// System reg/w
#define SYS_DRIVE                   0b00000011 // Drive select
#define SYS_RESET                   0b00000100 // Zero for FDC reset
#define SYS_HALT                    0b00001000 // Zero for FDC stop
#define SYS_HEAD                    0b00010000 // Disk side select
 
// System reg/w
#define SYS_DRQ                     0b01000000 // Data ready
#define SYS_INTRQ                   0b10000000 // Command completed

// Command reg
#define CMD_STEP_RATE               0b00000011 
#define CMD_VERIFY                  0b00000100 // Disk label verify
#define CMD_HEAD_LOAD               0b00001000 // Head load at start
#define CMD_MODIFY                  0b00010000 // Track register modify
#define CMD_DELAY                   0b00000100 // 15ms delay at start
#define CMD_MULTI_SECTOR            0b00010000

// Status reg
#define STS_BUSY                    0b00000001 // Command not complete
#define STS_INDEX                   0b00000010 // Index hole impulse
#define STS_TRACK0                  0b00000100 // Head set on the zero track
#define STS_CRC_ERROR               0b00001000 // CRC error
#define STS_SEEK_ERROR              0b00010000 // Record not found
#define STS_HEAD_LOAD               0b00100000 // Head was loaded
//#define STS_WRITE_PROTECT           0b01000000 // Write protected
#define STS_NOT_READY               0b10000000 // Drive not ready

#define STS_DRQ                     0b00000010 // Data is r/w ready
#define STS_DATA_LOST               0b00000100 // Data transfer timeout
#define STS_RNF                     0b00010000 // Record not found
#define STS_WRITE_PROTECT           0b01000000 // Write protected

// Timeing
#define DISK_CLK                    700000     // Media turn time
#define MOTOR_CLK                   DISK_CLK*15// Motor time while
#define INDEX_CLK                   7000       // Index hole sensor
#define DRQ_CLK                     80

struct FDD {
    int motor_clk = 0;          // Motor turn-on while time
    int track = 0;              // Head tarack
    char step_dir = -1;         // Step direction
    unsigned char *data = NULL;
    size_t size = 0;
    bool write_protect = true;
};

class FDC : public Device {
public:
    FDC();
    ~FDC();

    void load_trd(int drive_num, const char *path);
    void load_scl(int drive_num, const char *path);
    void save_trd(int drive_num, const char *path);

    bool io_wr(unsigned short port, unsigned char byte, int clk);
    bool io_rd(unsigned short port, unsigned char *p_byte, int clk);

    void update(int clk);
    void frame(int clk);
    void reset();

private:
    unsigned char reg_command;
    unsigned char reg_track;
    unsigned char reg_sector;
    unsigned char reg_data;
    unsigned char reg_status;
    unsigned char reg_system;
    int command_clk = 0;
    int last_clk;
    int delay_clk;
    int data_idx;
    int step_time[4] = {
        6 * Z80_FREQ / 1000, 12 * Z80_FREQ / 1000,
       20 * Z80_FREQ / 1000, 30 * Z80_FREQ / 1000 };
    FDD fdd[4];
};

#pragma pack(1)
struct SCL_Entry {
    char file_name[8];
    char file_type;
};

struct SCL_Header {
    char signature[8];
    unsigned char files;
};
#pragma pack()
