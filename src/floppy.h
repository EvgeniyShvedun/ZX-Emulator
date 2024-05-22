// Floppy disk controller FD179X

// Time units to cpu clock states conversion.
#define Msec(x) ((s32)((Z80_FREQ/1000)*(x)))
#define Usec(x) ((s32)((Z80_FREQ/1000000)*(x)))
#define Nsec(x) ((s32)((Z80_FREQ/1000000000)*(x)))

// "System" register w/mode.
#define SS_DRIVE                    0b00000011              // Drive select
#define SS_RESET                    0b00000100              // Zero to FDC reset.
#define SS_HALT                     0b00001000              // Zero to stop FDC.
#define SS_HEAD                     0b00010000              // Disk side select

// "System" register r/mode.
#define SS_DRQ                      0b01000000              // Data I/O ready.
#define SS_INTRQ                    0b10000000              // Command completed.

// Command bit fields.
#define CM_STEP_RATE                0b00000011
#define CM_VERIFY                   0b00000100              // Label info verify.
#define CM_HEAD_LOAD                0b00001000              // Load head on command start by turn on HLD state.
#define CM_TRACK_MODIFY             0b00010000              // Track register modify.
#define CM_DELAY                    0b00000100              // 15ms delay at start.
#define CM_MULTISEC                 0b00010000              // Read sectors to the end of track.

// Status register.
#define ST_BUSY                     0b00000001              // Command in progress.
#define ST_INDEX                    0b00000010              // Index label sensor.
#define ST_TRACK0                   0b00000100              // Head at zero track.
#define ST_CRC_ERROR                0b00001000              // Corupted data.
#define ST_SEEK_ERROR               0b00010000              // Seek data error.
#define ST_HEAD_LOADED              0b00100000              // The logical "and" of HLD/HLT.
#define ST_WRITE_PROTECT            0b01000000              // Write protected.
#define ST_NOT_READY                0b10000000              // Drive not ready.
#define ST_DRQ                      0b00000010              // Data i/o ready

#define ST_LOST_DATA                0b00000100              // Data timeout.
#define ST_RNF                      0b00010000              // Record not found.
#define ST_WRITE_FAULT              0b00100000              // FDD fault to write.
//#define ST_RECORD_TYPE                0b00100000          // Data type from address mark.

// Time in CPU clocks.
#define DISK_TURN_PERIOD            Msec(200)               // Disk revolution time.
#define INDEX_PULSE_WIDTH           Usec(10)                // Index sensor high level while.
#define DRQ_PERIOD                  Usec(35)                //
#define DRQ_WIDTH                   Usec(25)                // Time for servicing DRQ and avoid "lost data".
#define HLT_TIME                    Msec(50)                // "Head load timeing" HLT 30-100ms. HLD=1/HLT=1 Head is complete engaged.
#define DELAY_15MS                  Msec(15)                // Delay 15ms.
#define IDLE_WIDTH                  DISK_TURN_PERIOD*15     // idle state (non-busy) 15 index pulses have occured.

struct FDD {
    bool hlt = false;
    s16 track;                                              // Drive head track positioned on.
    bool wprt;                                              // Write protected sensor status.
    u8 *data;                                               // Disk image sectors data.
    size_t size;
};

class FDC : public Device {
public:
    FDC();
    ~FDC();

    void load_trd(int drive, const char *file_path, bool write_protect = false);
    void load_scl(int drive, const char *file_path, bool write_protect = false);
    void save_trd(int drive, const char *file_path);

    void read(u16 port, u8 *byte, s32 clk);
    void write(u16 port, u8 byte, s32 clk);

    void update(int clk);
    void frame(int clk);
    void reset();

private:
    FDD fdd[4];
    FDD *drive = &fdd[0];
    u8 reg_status;
    u8 reg_track;
    u8 reg_sector;
    u8 reg_data;
    u8 reg_command;
    u8 reg_system;
    bool hld;
    s16 step_dir;
    s16 step_cnt;
    s32 cmd_time;
    s32 data_idx;
    s32 last_clk;
    s32 time;
    s32 step_rate[4] = { Msec(3)/70, Msec(6), Msec(10), Msec(15) };
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
