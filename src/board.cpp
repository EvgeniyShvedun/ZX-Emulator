#include "base.h"

Board::Board(HW hw){
    add_device(&ula);
    set_hw(hw);
}

void Board::frame(){
    cpu.frame(&ula, this, frame_clk);
    cpu.interrupt(&ula);
    cpu.clk -= frame_clk;
    IO::frame(frame_clk);
}

void Board::reset(){
    cpu.reset();
    IO::reset();
}
