#include "base.h"

Board::Board(HW hw) {
    add_device(&ula);
    set_hw(hw);
}

void Board::frame(){
    cpu.frame(&ula, this, frame_clk);
    IO::frame(frame_clk);
}

void Board::reset(){
    cpu.reset();
    IO::reset();
}

/*
void Board::draw_device_status(){
    unsigned int *p_pic;
    int x = STATUS_PIC_X, y = STATUS_PIC_Y;
    for (int i = 0; i < device_idx; i++){
        p_pic = p_devices[i]->status_pic();
        if (p_pic){
            for (int h = 0; h < STATUS_PIC_HEIGHT; h++)
                for (int w = 0; w < STATUS_PIC_WIDTH; w++)
                    frame_buffer[(y + h) * SCREEN_WIDTH + x + w] = RGBA32_4444(p_pic[h * STATUS_PIC_WIDTH + w]);
            x += STATUS_PIC_WIDTH + 2;
        }
    }
}
*/
