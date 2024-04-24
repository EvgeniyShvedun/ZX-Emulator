#define DEVICE_MAX 16

class Device {
    public:
        virtual ~Device(){};

        virtual bool io_rd(unsigned short port, unsigned char *byte, int clk){ return false; };
        virtual bool io_wr(unsigned short port, unsigned char byte, int clk){ return false; };

        virtual void reset(){};
        virtual void frame(int clk){};
};

class IO {
    public:
    virtual ~IO(){};
        virtual unsigned char read(unsigned short port, int clk = 0) { return 0xFF; };
        virtual void write(unsigned short port, unsigned char byte, int clk = 0) {};
};
