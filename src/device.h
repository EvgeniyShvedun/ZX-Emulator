#define DEVICE_MAX 16

class Device {
    public:
        virtual ~Device(){};

        virtual bool io_rd(unsigned short addr, unsigned char *byte, int clk){ return false; };
        virtual bool io_wr(unsigned short port, unsigned char byte, int clk){ return false; };

        virtual void reset(){};
        virtual void frame(int clk){};
};

class IO {
    public:
        void add_device(Device *p_device);

        unsigned char read(unsigned short port, int clk = 0);
        void write(unsigned short port, unsigned char byte, int clk = 0);

        void frame(int clk);
        void reset();

    private:
        Device *p_devices[DEVICE_MAX];
        int device_idx = 0;
};
