#define TAP_FLAG_HEADER 0
#define TAP_TYPE_PROGRAM 0

#define TAPE_EAR_IN                 0b1000000

#define SILENCE_TIME                (int)(Z80_FREQ * 1.0)
#define TONE_PERIOD                 2168
#define TONE_PULSES                 (int)(3223 * 2.0)
#define ONE_PERIOD                  1710
#define ZERO_PERIOD                 855

class Tape : public Device {
    public:
        enum STATE { STOP, SILENCE, TONE, SYNC_1, SYNC_2, DATA, ONE, ZERO };
        Tape();
        ~Tape();
        bool load_tap(const char *p_path);
        void rewind(int time_shift);
        void play();
        void stop();
        void update(int clk);
        void frame(int clk);
        bool io_rd(unsigned short port, unsigned char *p_byte, int clk);
    private:
        FILE *p_file;
        STATE state;
        int last_clk;
        int time;
        int idx;
        int block_end;
        int bit;
        int pulse;
        const int state_wait[8] = {
            SILENCE_TIME,
            SILENCE_TIME,
            TONE_PERIOD,
            667,
            735,
            0,
            ONE_PERIOD,
            ZERO_PERIOD
        };
        unsigned char *p_data;
        int data_size;
        unsigned char pFE;
};
