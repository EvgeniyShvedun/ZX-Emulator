#include "base.h"

unsigned char IO::read(unsigned short port, int clk){
    unsigned char byte = 0xFF;
    for (int i = 0; i < device_idx; i++)
        if (p_devices[i]->io_rd(port, &byte, clk))
            break;
#ifdef DEBUG
    printf("PORT %04x in %02x\n", port, byte);
#endif
    return byte;
}

void IO::write(unsigned short port, unsigned char byte, int clk){
#ifdef DEBUG
    printf("PORT %04x out %02x\n", port, byte);
#endif
    for (int i = 0; i < device_idx; i++)
        if (p_devices[i]->io_wr(port, byte, clk))
            break;
}

void IO::frame(int clk){
    for (int i = 0; i < device_idx; i++)
        p_devices[i]->frame(clk);
}

void IO::reset(){
    for (int i = 0; i < device_idx; i++)
        p_devices[i]->reset();
}

void IO::add_device(Device *p_device){
    if (device_idx > DEVICE_MAX -1)
        throw;
    if (device_idx)
        p_devices[device_idx] = p_devices[0];
    p_devices[0] = p_device;
    device_idx++;
}
