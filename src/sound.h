#define AY_RATE                     (1774400 / 8.0)
#define TAPE_OUT	    			0b00001000
#define SPEAKER 	    			0b00010000
#define TAPE_IN                     0b01000000

class Sound : public Device {
/*
Register       Function                        Range
               Channel A fine pitch            8-bit (0-255)
               Channel A course pitch          4-bit (0-15)
               Channel B fine pitch            8-bit (0-255)
               Channel B course pitch          4-bit (0-15)
               Channel C fine pitch            8-bit (0-255)
               Channel C course pitch          4-bit (0-15)
               Noise pitch                     5-bit (0-31) 
               Mixer                           8-bit (see below)
               Channel A volume                4-bit (0-15, see below)
               Channel B volume                4-bit (0-15, see below)
               Channel C volume                4-bit (0-15, see below)
               Envelope fine duration          8-bit (0-255)
               Envelope course duration        8-bit (0-255)
               Envelope shape                  4-bit (0-15) 
               I/O port A                      8-bit (0-255)
               I/O port B                      8-bit (0-255)
*/
	enum {
    	A_PCH_L, A_PCH_H,
        B_PCH_L, B_PCH_H,
        C_PCH_L, C_PCH_H,
    	N_PCH,
        MIX,
        A_VOL, B_VOL, C_VOL,
        ENV_L, ENV_H, ENV_SHAPE,
    	PORT_A, PORT_B
	};
    public:
	    enum STEREO { LEFT, RIGHT };
	    enum CHANNEL { A, B, C };
		~Sound();
        void init(int sample_rate, int frame_clk);
        void setup_lpf(int rate);
        void set_mixer_levels(float side = 1.0, float center = 0.5, float opposite = 0.25);
        void set_mixer_mode(int mode) { mixer_mode = mode; };
		void set_ay_volume(float volume = 1.0);
		void set_speaker_volume(float volume = 0.5);
		void set_tape_volume(float volume = 0.3);
		void update(s32 clk);
        void queue();

        void read(u16 port, u8* byte, s32 clk);
        void write(u16 port, u8 byte, s32 clk);
		void frame(s32 frame_clk);
        void reset();

	protected:
        int sample_rate;
		s32 frame_idx;
		float increment, cpu_factor;
		unsigned char tone_a, tone_b, tone_c, noise, envelope;
		float tone_a_counter, tone_b_counter, tone_c_counter, noise_counter, envelope_counter;
		float tone_a_limit, tone_b_limit, tone_c_limit, noise_limit, envelope_limit;
		unsigned int noise_seed;
		int envelope_idx;
        /*
        The envelope counter on the AY-3-8910 has 16 steps. On the YM2149 it has twice the steps, happening twice as fast.
        C AtAlH
        0 0 x x  \___ 0-3
        0 1 x x  /___ 4
        1 0 0 0  \\\\ 8
        1 0 0 1  \___ 9
        1 0 1 0  \/\/ 10
        1 0 1 1  \``` 11
        1 1 0 0  //// 12
        1 1 0 1  /``` 13
        1 1 1 0  /\/\ 14
        1 1 1 1  /___ 15
        */
        const unsigned char envelope_shape[0x20 * 0x10] = {
            0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, /* \__ */
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, /* \__ */
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, /* \__ */
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, /* \__ */
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, /* /|_ */
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, /* /|_ */
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, /* /|_ */
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, /* /|_ */
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, /* \|\ */
            0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00,
            0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, /* \__ */
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, /* \/ */
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
            0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00, /* \-- */
            0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, /* /|/ */
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, /* /-- */
            0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, /* /\ */
            0x0F, 0x0E, 0x0D, 0x0C, 0x0B, 0x0A, 0x09, 0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01, 0x00,
            0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, /* /|__*/
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
		float ay_volume, speaker_volume, tape_volume;
	    float left_amp, right_amp;
        float speaker_amp, tape_in_amp, tape_out_amp;
        float lpf_alpha;
		int mixer_mode;
        float mixer[sizeof(AY_Mixer)][sizeof(STEREO)][sizeof(CHANNEL)];
        // Posted to comp.sys.sinclair in Dec 2001 by Matthew Westcott.
        const float ay_volume_table[0x10] = {
            0.000000, 0.013748, 0.020462, 0.029053, 0.042343, 0.061844, 0.084718, 0.136903,
            0.169130, 0.264667, 0.352712, 0.449942, 0.570382, 0.687281, 0.848172, 1.000000
        };
		unsigned char registers[0x10];
		unsigned char port_wFE, port_rFE, port_wFFFD;
        SDL_AudioSpec audio_spec;
        SDL_AudioDeviceID audio_device_id = 0;
        unsigned int frame_samples = 0;
        short *buffer = NULL;
};
