class IO {
    public:
        virtual ~IO() {};
        virtual void read(u16 port, u8* byte, s32 clk=0) {};
        virtual void write(u16 port, u8 byte, s32 clk=0) {};
};

class Device : public IO {
    public:
        virtual ~Device() {};
        virtual void reset() {};
        virtual void frame(s32 clk) {};
};
