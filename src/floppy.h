// Floppy disk controller FD179X

// "System" register w/mode.
#define SS_DRIVE                   0b00000011 				// Drive select
#define SS_RESET                   0b00000100 				// Zero to FDC reset.
#define SS_HALT                    0b00001000 				// Zero to stop FDC.
#define SS_HEAD                    0b00010000 				// Disk side select
 
// "System" register r/mode.
#define SS_DRQ                     0b01000000 				// Data I/O ready.
#define SS_INTRQ                   0b10000000 				// Command completed.

// Command bit fields.
#define CM_STEP_RATE               0b00000011 
#define CM_VERIFY                  0b00000100 				// Label info verify.
#define CM_HEAD_LOAD               0b00001000 				// Load head on command start.
#define CM_TRACK_MODIFY            0b00010000 				// Track register modify.
#define CM_DELAY                   0b00000100 				// 15ms delay at start.
#define CM_MULTISEC                0b00010000

// Status register.
#define ST_BUSY                    0b00000001 				// Command in progress.
#define ST_INDEX                   0b00000010 				// Index label sensor.
#define ST_TRACK0                  0b00000100 				// Head at zero track.
#define ST_CRC_ERROR               0b00001000 				// Corupted data.
#define ST_SEEK_ERROR              0b00010000 				// Seek data error.
#define ST_HEAD_LOAD               0b00100000 				// Head loaded.
//#define S_WRITE_PROTECT           0b01000000 				// Write protected.
#define ST_NOT_READY               0b10000000 				// Drive not ready.
#define ST_DRQ                     0b00000010 				// Data i/o ready
#define ST_DATA_LOST               0b00000100 				// Data timeout.
#define ST_RNF                     0b00010000 				// Record not found.
#define ST_WRITE_PROTECT           0b01000000 				// Write protected.

// Time values in cpu clock states.
#define DISK_TURN_TIME            700000
#define MOTOR_ON_WHILE            DISK_TURN_TIME*15
#define INDEX_PERIOD              7000        			    // "Index label" sensor activation.
#define DRQ_TIME                  80                        // Data i/o ready request..

struct FDD {
    s32 motor;
    s32 track;
    s8 step_dir;
    u8 *data;
    size_t size;
    u8 *io_ptr;
    bool write_deny;
};

class FDC : public Device {
public:
    FDC();
    ~FDC();

    void load_trd(int drive, const char *file_path);
    void load_scl(int drive, const char *file_path);
    void save_trd(int drive, const char *file_path);

    void read(u16 port, u8 *byte, s32 clk);
    void write(u16 port, u8 byte, s32 clk);

    void update(int clk);
    void frame(int clk);
    void reset();

private:
    u8 reg_status;
    u8 reg_track;
    u8 reg_sector;
    u8 reg_data;
    u8 reg_command;
    u8 reg_system;
    s32 data_idx;
    FDD drives[4];
    s32 command_time;
    s32 time;
    s32 last_clk;
    float step_rate[4] = {
         (6.0f / Z80_FREQ / 1000.0f),
        (20.0f / Z80_FREQ / 1000.0f),
        (12.0f / Z80_FREQ / 1000.0f),
        (30.0f / Z80_FREQ / 1000.0)
    };
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
