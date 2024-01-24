#include "base.h"

using namespace std;

unsigned short Disasm::dis(unsigned short ptr, Z80 *p_cpu, Memory *p_memory, bool show_info){
    int len, index = 0, idx_reg = 0;
    Desc *p_desc;
    unsigned short start_ptr = ptr;
    unsigned char byte = p_memory->read_byte_ex(ptr++);
/*
c - condition
p - register pair
y - 8 bit operand
b - byte value
w - word value
l - relative address
z - src 8 bit ( z 2-0 bit)
i - reg pair1 (y 5 -3 POP)
f - flags (CALL NZ)
s - bit on bit mnemonic (y bits)
x - IX or IY
X - (IX + s),
Y - (IX + s)
*/
    switch(byte){
        case 0xCB:
            byte = p_memory->read_byte_ex(ptr++);
            p_desc = &cb_prefix[byte];
        case 0xDD:
        case 0xFD:
            idx_reg = byte == 0xDD ? IDX_REG::IX : IDX_REG::IY;
            byte = p_memory->read_byte_ex(ptr++);
            if (byte == 0xCB){
                index = (signed char)p_memory->read_byte(ptr++);
                byte = p_memory->read_byte_ex(ptr++);
                p_desc = &ddfdcb_prefix[byte];
            }else{
                if (ddfd_prefix[byte].mnem == zDB)
                    p_desc = &no_prefix[byte];
                else
                    p_desc = &ddfd_prefix[byte];
            }
            break;
        case 0xED:
            byte = p_memory->read_byte_ex(ptr++);
            p_desc = &ed_prefix[byte];
            break;
       default:
            p_desc = &no_prefix[byte];
            break;
    }
    len = sprintf(line, "%04X %c ", start_ptr, show_info and p_cpu->pc == start_ptr ? '>' : ' ');
    for (int i = 0; i < 5; i++)
        len += sprintf(line + len, " %02x", p_memory->read_byte_ex(start_ptr + i));
    len += sprintf(line + len, "       %s ", p_mnemonic[p_desc->mnem]);
    if (p_desc->p_op){
        for (int i = 0; p_desc->p_op[i]; i++){
            switch (p_desc->p_op[i]){
                case 'p': // Register pair.
                    len += sprintf(line + len, "%s", p_op16_1[((byte >> 4) & 0x03)]);
                    break;
                case 'i':
                    len += sprintf(line + len, "%s", p_op16_2[((byte >> 4) & 0x03)]);
                    break;
                case 'b': // LD B, #NN
                    len += sprintf(line + len, "#%02x", p_memory->read_byte_ex(ptr++));
                    break;
                case 'w': // LD BC, #NNNN
                    byte = p_memory->read_byte_ex(ptr++);
                    len += sprintf(line + len, "#%02x%02x", p_memory->read_byte_ex(ptr++), byte);
                    break;
               case 'l': // Relative ptr.  DJNZ, JR.
                    index = (signed char)p_memory->read_byte_ex(ptr++);
                    len += sprintf(line + len, "#%04x", ptr + index);
                    break;
               case 'y': // Operand 8 bit 5-3.
                    len += sprintf(line + len, "%s", p_op8[(byte >> 3) & 0x07]);
                    break;
               case 'z': // Operand 8 bit 2-0.
                    len += sprintf(line + len, "%s", p_op8[byte & 0x07]);
                    break;
               case 'f': // Condition flags, bit 5-3.
                    len += sprintf(line + len, "%s", p_flag[(byte >> 3) & 0x07]);
                    break;
               case 'c': // Condition flags, bit 5-3.
                    len += sprintf(line + len, "%s", p_flag[((byte >> 3) & 0x07) - 4]); //!!!!!!!!!!!!
                    break;
               case 't': // RST
                    len += sprintf(line + len, "#%02x", byte & (0x07 << 3));
                    break;
               case 's': // RES 1, B
                    len += sprintf(line + len, "%d", (byte >> 3) & 0x07);
                    break;
               case 'x': // IX / IY
                    len += sprintf(line + len, "%s", p_idx_reg[idx_reg]);
                    break;
               case 'X': // Read relative ptr.
                    index = (signed char)p_memory->read_byte_ex(ptr++);
                    break;
               case 'Y': // Out IX+S ptr.
                    len += sprintf(line + len, "(%s %c#%02x)", p_idx_reg[idx_reg], index >=0 ? '+' : '-', index & 0x7F);
                    break;
               default:
                    line[len++] = p_desc->p_op[i];
                    break;
            }
        }
    }
    line[len] = 0x00;
    printf("%s\n", line);
    return ptr;
}
