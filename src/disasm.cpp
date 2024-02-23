#include "base.h"


#include "disasm.h"

namespace Disasm {
        const char *p_opcode[] = {
            "ADC", "ADD", "AND", "BIT", "CALL", "CCF", "CP", "CPD",
            "CPDR", "CPI", "CPIR", "CPL", "DAA",  "DB",  "DEC", "DI",
            "DJNZ", "EI",  "EX",  "EXX", "HALT", "IM",  "IN", "INC",
            "IND", "INDR", "INI", "INIR", "JP",   "JR",  "LD", "LDD",
            "LDDR", "LDI", "LDIR", "NEG", "NOP",  "OR",  "OUTDR", "OTIR",
            "OUT", "OUTD", "OUTI", "POP", "PUSH", "RES", "RET", "RETI",
            "RETN", "RL",  "RLA", "RLC", "RLCA", "RLD", "RR", "RRA",
            "RRC", "RRCA", "RRD", "RST", "SBC",  "SCF", "SET", "SLA",
            "SLL", "SRA", "SRL", "SUB", "XOR"
        };
        const char *p_op16_1[4]    = { "BC", "DE", "HL", "SP" };
        const char *p_op16_2[4]    = { "BC", "DE", "HL", "AF" };
        const char *p_op8[8]       = { "B", "C", "D", "E", "H", "L", "(HL)", "A" };
        const char *p_flag[8]      = { "NZ", "Z", "NC", "C", "PO", "PE", "P", "M" };
        const char *p_idx_reg[2]   = {  "IX", "IY" };

        Table no_prefix[0x100] = {
            {zNOP, NULL},       {zLD, "p, w"},      {zLD, "(BC), A"},    {zINC, "p"},        {zINC, "y"},        {zDEC, "y"},        {zLD, "B, b"},      {zRLCA, NULL},  // 0x00
            {zEX, "AF, AF'"},   {zADD, "HL, p"},    {zLD, "A, (BC)"},    {zDEC, "p"},        {zINC, "y"},        {zDEC, "y"},        {zLD, "C, b"},      {zRRCA, NULL},  // 0x08
            {zDJNZ, "l"},       {zLD, "p, w"},      {zLD, "(DE), A"},    {zINC, "p"},        {zINC, "y"},        {zDEC, "y"},        {zLD, "D, b"},      {zRLA, NULL},   // 0x10
            {zJR, "c, l"},      {zADD, "HL, p"},    {zLD, "A, (DE)"},    {zDEC, "p"},        {zINC, "y"},        {zDEC, "y"},        {zLD, "E, b"},      {zRRA, NULL},   // 0x18
            {zJR, "c, l"},      {zLD, "p, w"},      {zLD, "(w), HL"},    {zINC, "p"},        {zINC, "y"},        {zDEC, "y"},        {zLD, "H, b"},      {zDAA, NULL},   // 0x20
            {zJR, "c, l"},      {zADD, "HL, p"},    {zLD, "HL, (w)"},    {zDEC, "p"},        {zINC, "y"},        {zDEC, "y"},        {zLD, "L, b"},      {zCPL, NULL},   // 0x08
            {zJR, "c, l"},      {zLD, "p, w"},      {zLD, "(w), A"},     {zINC, "p"},        {zINC, "y"},        {zDEC, "y"},        {zLD, "(HL), b"},   {zSCF, NULL},   // 0x30
            {zJR, "c, l"},      {zADD, "HL, p"},    {zLD, "A, (w)"},     {zDEC, "p"},        {zINC, "y"},        {zDEC, "y"},        {zLD, "A, b"},      {zCCF, NULL},   // 0x38
            {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},       {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},  // 0x40
            {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},       {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},  // 0x48
            {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},       {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},  // 0x50
            {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},       {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},  // 0x58
            {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},       {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},  // 0x60
            {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},       {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},  // 0x68
            {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},       {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},      {zHLT, NULL},       {zLD, "y, z"},  // 0x70
            {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},       {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},      {zLD, "y, z"},
            {zADD, "A, z"},     {zADD, "A, z"},     {zADD, "A, z"},      {zADD, "A, z"},     {zADD, "A, z"},     {zADD, "A, z"},     {zADD, "A, z"},     {zADD, "A, z"},
            {zADC, "A, z"},     {zADC, "A, z"},     {zADC, "A, z"},      {zADC, "A, z"},     {zADC, "A, z"},     {zADC, "A, z"},     {zADC, "A, z"},     {zADC, "A, z"},
            {zSUB, "z"},        {zSUB, "z"},        {zSUB, "z"},         {zSUB, "z"},        {zSUB, "z"},        {zSUB, "z"},        {zSUB, "z"},        {zSUB, "z"},
            {zSBC, "A, z"},     {zSBC, "A, z"},     {zSBC, "A, z"},      {zSBC, "A, z"},     {zSBC, "A, z"},     {zSBC, "A, z"},     {zSBC, "A, z"},     {zSBC, "A, z"},
            {zAND, "z"},        {zAND, "z"},        {zAND, "z"},         {zAND, "z"},        {zAND, "z"},        {zAND, "z"},        {zAND, "z"},        {zAND, "z"},
            {zXOR, "z"},        {zXOR, "z"},        {zXOR, "z"},         {zXOR, "z"},        {zXOR, "z"},        {zXOR, "z"},        {zXOR, "z"},        {zXOR, "z"},
            {zOR, "z"},         {zOR, "z"},         {zOR, "z"},          {zOR, "z"},         {zOR, "z"},         {zOR, "z"},         {zOR, "z"},         {zOR, "z"},
            {zCP, "z"},         {zCP, "z"},         {zCP, "z"},          {zCP, "z"},         {zCP, "z"},         {zCP, "z"},         {zCP, "z"},         {zCP, "z"},
            {zRET, "f"},        {zPOP, "i"},        {zJP, "f, w"},       {zJP, "w"},         {zCALL, "f, w"},    {zPUSH, "i"},       {zADD, "A, b"},     {zRST, "t"},
            {zRET, "f"},        {zRET, NULL},       {zJP, "f, w"},       {zDB, NULL},        {zCALL, "f, w"},    {zCALL, "w"},       {zADC, "A, b"},     {zRST, "t"},
            {zRET, "f"},        {zPOP, "i"},        {zJP, "f, w"},       {zOUT, "(b), A"},   {zCALL, "f, w"},    {zPUSH, "i"},       {zSUB, "b"},        {zRST, "t"},
            {zRET, "f"},        {zEXX, NULL},       {zJP, "f, w"},       {zIN, "A, (b)"},    {zCALL, "f, w"},    {zDB, NULL},        {zSBC, "A, b"},     {zRST, "t"},
            {zRET, "f"},        {zPOP, "i"},        {zJP, "f, w"},       {zEX, "(SP), HL"},  {zCALL, "f, w"},    {zPUSH, "i"},       {zAND, "b"},        {zRST, "t"},
            {zRET, "f"},        {zJP, "(HL)"},      {zJP, "f, w"},       {zEX, "DE, HL"},    {zCALL, "f, w"},    {zDB, NULL},        {zXOR, "b"},        {zRST, "t"},
            {zRET, "f"},        {zPOP, "i"},        {zJP, "f, w"},       {zDI, NULL},        {zCALL, "f, w"},    {zPUSH, "i"},       {zOR, "b"},         {zRST, "t"},
            {zRET, "f"},        {zLD, "SP, HL"},    {zJP, "f, w"},       {zEI, NULL},        {zCALL, "f, w"},    {zDB, NULL},        {zCP, "b"},         {zRST, "t"}
        };
        Table cb_prefix[0x100] = {
            {zRLC, "z"},     {zRLC, "z"},     {zRLC, "z"},     {zRLC, "z"}, 
            {zRLC, "z"},     {zRLC, "z"},     {zRLC, "z"},     {zRLC, "z"},
            {zRRC, "z"},     {zRRC, "z"},     {zRRC, "z"},     {zRRC, "z"},
            {zRRC, "z"},     {zRRC, "z"},     {zRRC, "z"},     {zRRC, "z"},
            {zRL, "z"},      {zRL, "z"},      {zRL, "z"},      {zRL, "z"},
            {zRL, "z"},      {zRL, "z"},      {zRL, "z"},      {zRL, "z"},
            {zRR, "z"},      {zRR, "z"},      {zRR, "z"},      {zRR, "z"},
            {zRR, "z"},      {zRR, "z"},      {zRR, "z"},      {zRR, "z"},
            {zSLA, "z"},     {zSLA, "z"},     {zSLA, "z"},     {zSLA, "z"},
            {zSLA, "z"},     {zSLA, "z"},     {zSLA, "z"},     {zSLA, "z"},
            {zSRA, "z"},     {zSRA, "z"},     {zSRA, "z"},     {zSRA, "z"},
            {zSRA, "z"},     {zSRA, "z"},     {zSRA, "z"},     {zSRA, "z"},
            {zSLL, "z"},     {zSLL, "z"},     {zSLL, "z"},     {zSLL, "z"},
            {zSLL, "z"},     {zSLL, "z"},     {zSLL, "z"},     {zSLL, "z"},
            {zSRL, "z"},     {zSRL, "z"},     {zSRL, "z"},     {zSRL, "z"},
            {zSRL, "z"},     {zSRL, "z"},     {zSRL, "z"},     {zSRL, "z"},
            {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},
            {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},
            {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},
            {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},
            {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},
            {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},
            {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},
            {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},
            {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},
            {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},
            {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},
            {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},
            {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},
            {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},
            {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},
            {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},  {zBIT, "s, z"},
            {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},
            {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},
            {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},
            {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},
            {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},
            {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},
            {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},
            {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},
            {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},
            {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},
            {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},
            {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},
            {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},
            {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},
            {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},
            {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},  {zRES, "s, z"},
            {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},
            {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},
            {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},
            {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},
            {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},
            {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},
            {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},
            {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},
            {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},
            {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},
            {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},
            {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},
            {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},
            {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},
            {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},
            {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"},  {zSET, "s, z"}
        };
        Table ddfd_prefix[0x100] = {
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zADD, "x, p"},  {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zADD, "x, p"},  {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zLD, "x, w"},   {zLD, "(w), x"}, {zINC, "x"},
            {zINC, "xh"},    {zDEC, "xh"},    {zLD, "xh, b"},  {zDB, "?"},
            {zDB, "?"},      {zADD, "x, x"},  {zLD, "x, (w)"}, {zDEC, "x"},
            {zINC, "xl"},    {zDEC, "xl"},    {zLD, "xl, b"},  {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zINC, "X"},     {zDEC, "X"},     {zLD, "X, b"},   {zDB, "?"},
            {zDB, "?"},      {zADD, "x, p"},  {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zLD, "y, xh"},  {zLD, "y, xl"},  {zLD, "y, X"},   {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zLD, "y, xh"},  {zLD, "y, xl"},  {zLD, "y, X"},   {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zLD, "y, xh"},  {zLD, "y, xl"},  {zLD, "y, X"},   {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zLD, "y, xh"},  {zLD, "y, xl"},  {zLD, "y, X"},   {zDB, "?"},
            {zLD, "xh, z"},  {zLD, "xh, z"},  {zLD, "xh, z"},  {zLD, "xh, z"},
            {zLD, "xh, xh"}, {zLD, "xh, xl"}, {zLD, "y, X"},   {zLD, "xh, z"},
            {zLD, "xl, z"},  {zLD, "xl, z"},  {zLD, "xl, z"},  {zLD, "xl, z"},
            {zLD, "xl, xh"}, {zLD, "xl, xl"}, {zLD, "y, X"},   {zLD, "xl, z"},
            {zLD, "X, z"},   {zLD, "X, z"},   {zLD, "X, z"},   {zLD, "X, z"},
            {zLD, "X, z"},   {zLD, "X, z"},   {zDB, "?"},      {zLD, "X, z"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zLD, "y, xh"},  {zLD, "y, xl"},  {zLD, "y, X"},   {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zADD, "A, xh"}, {zADD, "A, xl"}, {zADD, "A, X"},  {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zADC, "A, xh"}, {zADC, "A, xl"}, {zADC, "A, X"},  {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zSUB, "xh"},    {zSUB, "xl"},    {zSUB, "X"},     {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zSBC, "A, xh"}, {zSBC, "A, xl"}, {zSBC, "A, X"},  {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zAND, "xh"},    {zAND, "xl"},    {zAND, "X"},     {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zXOR, "xh"},    {zXOR, "xl"},    {zXOR, "X"},     {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zOR, "xh"},     {zOR, "xl"},     {zOR, "X"},      {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zCP, "xh"},     {zCP, "xl"},     {zCP, "X"},      {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, NULL},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zPOP, "x"},     {zDB, "?"},      {zEX, "(SP), x"},
            {zDB, "?"},      {zPUSH, "x"},    {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zJP, "(x)"},    {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zLD, "SP, x"},  {zDB, "?"},      {zDB, "?"},
            {zDB, "?"},      {zDB, "?"},      {zDB, "?"},      {zDB, "?"}
        };
        Table ed_prefix[0x100] = {
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zIN, "y, (C)"},  {zOUT, "(C), y"}, {zSBC, "HL, p"},  {zLD, "(w), p"},
            {zNEG, NULL},     {zRETN, NULL},    {zIM, "0"},       {zLD, "I, A"},
            {zIN, "y, (C)"},  {zOUT, "(C), y"}, {zADC, "HL, p"},  {zLD, "p, (w)"},
            {zNEG, NULL},     {zRETI, NULL},    {zIM, "0"},       {zLD, "R, A"},
            {zIN, "y, (C)"},  {zOUT, "(C), y"}, {zSBC, "HL, p"},  {zLD, "(w), p"},
            {zNEG, NULL},     {zRETN, NULL},    {zIM, "1"},       {zLD, "A, I"},
            {zIN, "y, (C)"},  {zOUT, "(C), y"}, {zADC, "HL, p"},  {zLD, "p, (w)"},
            {zNEG, NULL},     {zRETI, NULL},    {zIM, "2"},       {zLD, "A, R"},
            {zIN, "y, (C)"},  {zOUT, "(C), y"}, {zSBC, "HL, p"},  {zLD, "(w), p"},
            {zNEG, NULL},     {zRETN, NULL},    {zIM, "0"},       {zRRD, "A, (HL)"},
            {zIN, "y, (V)"},  {zOUT, "(C), y"}, {zADC, "HL, p"},  {zLD, "p, (w)"},
            {zNEG, NULL},     {zRETI, NULL},    {zIM, "0"},       {zRLD, "A, (HL)"},
            {zIN, "0, (C)"},  {zOUT, "(C), 0"}, {zSBC, "HL, p"},  {zLD, "(w), p"},
            {zNEG, NULL},     {zRETN, NULL},    {zIM, "1"},       {zDB, "?"},
            {zIN, "y, (C)"},  {zOUT, "(C), y"}, {zADC, "HL, p"},  {zLD, "p, (w)"},
            {zNEG, NULL},     {zRETI, NULL},    {zIM, "2"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zLDI, NULL},     {zCPI, NULL},     {zINI, NULL},     {zOUTI, NULL},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zLDD, NULL},     {zCPD, NULL},     {zIND, NULL},     {zOUTD, NULL},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zLDIR, NULL},    {zCPIR, NULL},    {zINIR, NULL},    {zOTIR, NULL},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zLDDR, NULL},    {zCPDR, NULL},    {zINDR, NULL},    {zOTDR, NULL},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"},
            {zDB, "?"},       {zDB, "?"},       {zDB, "?"},       {zDB, "?"}
        };
        Table ddfdcb_prefix[0x100] = {
            {zRLC,"Y, z"},    {zRLC,"Y, z"},    {zRLC,"Y, z"},    {zRLC,"Y, z"},
            {zRLC,"Y, z"},    {zRLC,"Y, z"},    {zRLC,"Y"},       {zRLC,"Y, z"},
            {zRRC,"Y, z"},    {zRRC,"Y, z"},    {zRRC,"Y, z"},    {zRRC,"Y, z"},
            {zRRC,"Y, z"},    {zRRC,"Y, z"},    {zRRC,"Y"},       {zRRC,"Y, z"},
            {zRL,"Y, z"},     {zRL,"Y, z"},     {zRL,"Y, z"},     {zRL,"Y, z"},
            {zRL,"Y, z"},     {zRL,"Y, z"},     {zRL,"Y"},        {zRL,"Y, z"},
            {zRR,"Y, z"},     {zRR,"Y, z"},     {zRR,"Y, z"},     {zRR,"Y, z"},
            {zRR,"Y, z"},     {zRR,"Y, z"},     {zRR,"Y"},        {zRR,"Y, z"},
            {zSLA,"Y, z"},    {zSLA,"Y, z"},    {zSLA,"Y, z"},    {zSLA,"Y, z"},
            {zSLA,"Y, z"},    {zSLA,"Y, z"},    {zSLA,"Y"},       {zSLA,"Y, z"},
            {zSRA,"Y, z"},    {zSRA,"Y, z"},    {zSRA,"Y, z"},    {zSRA,"Y, z"},
            {zSRA,"Y, z"},    {zSRA,"Y, z"},    {zSRA,"Y"},       {zSRA,"Y, z"},
            {zSLL,"Y, z"},    {zSLL,"Y, z"},    {zSLL,"Y, z"},    {zSLL,"Y, z"},
            {zSLL,"Y, z"},    {zSLL,"Y, z"},    {zSLL,"Y"},       {zSLL,"Y, z"},
            {zSRL,"Y, z"},    {zSRL,"Y, z"},    {zSRL,"Y, z"},    {zSRL,"Y, z"},
            {zSRL,"Y, z"},    {zSRL,"Y, z"},    {zSRL,"Y"},       {zSRL,"Y, z"},
            {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"},
            {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y"},    {zBIT,"s, Y, z"},
            {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"},
            {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y"},    {zBIT,"s, Y, z"},
            {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"},
            {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y"},    {zBIT,"s, Y, z"},
            {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"},
            {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y"},    {zBIT,"s, Y, z"},
            {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"},
            {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y"},    {zBIT,"s, Y, z"},
            {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"},
            {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y"},    {zBIT,"s, Y, z"},
            {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"},
            {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y"},    {zBIT,"s, Y, z"},
            {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"},
            {zBIT,"s, Y, z"}, {zBIT,"s, Y, z"}, {zBIT,"s, Y"},    {zBIT,"s, Y, z"},
            {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y, z"},
            {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y"},    {zRES,"s, Y, z"},
            {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y, z"},
            {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y"},    {zRES,"s, Y, z"},
            {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y, z"},
            {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y"},    {zRES,"s, Y, z"},
            {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y, z"},
            {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y"},    {zRES,"s, Y, z"},
            {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y, z"},
            {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y"},    {zRES,"s, Y, z"},
            {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y, z"},
            {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y"},    {zRES,"s, Y, z"},
            {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y, z"},
            {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y"},    {zRES,"s, Y, z"},
            {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y, z"},
            {zRES,"s, Y, z"}, {zRES,"s, Y, z"}, {zRES,"s, Y"},    {zRES,"s, Y, z"},
            {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y, z"},
            {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y"},    {zSET,"s, Y, z"},
            {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y, z"},
            {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y"},    {zSET,"s, Y, z"},
            {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y, z"},
            {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y"},    {zSET,"s, Y, z"},
            {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y, z"},
            {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y"},    {zSET,"s, Y, z"},
            {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y, z"},
            {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y"},    {zSET,"s, Y, z"},
            {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y, z"},
            {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y"},    {zSET,"s, Y, z"},
            {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y, z"},
            {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y"},    {zSET,"s, Y, z"},
            {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y, z"},
            {zSET,"s, Y, z"}, {zSET,"s, Y, z"}, {zSET,"s, Y"},    {zSET,"s, Y, z"}
        };

unsigned short line(char *p_dst, unsigned short pointer, Z80 *p_cpu, Memory *p_memory, bool info){
    int offset = 0, idx_reg = 0;
    unsigned short start = pointer;
    char *p_op = p_dst + OPCODE_POS;
    char *p_bt = p_dst;
    Table *p_decode;
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
    unsigned char byte = p_memory->read_byte_ex(pointer++);
    switch(byte){
        case 0xCB:
            byte = p_memory->read_byte_ex(pointer++);
            p_decode = &cb_prefix[byte];
            break;
        case 0xDD:
        case 0xFD:
            idx_reg = byte == 0xDD ? REG::IX : REG::IY;
            byte = p_memory->read_byte_ex(pointer++);
            if (byte == 0xCB){
                offset = (signed char)p_memory->read_byte_ex(pointer++);
                byte = p_memory->read_byte_ex(pointer++);
                p_decode = &ddfdcb_prefix[byte];
            }else
                p_decode = ddfd_prefix[byte].opcode_id == OPCODE::zDB ? &no_prefix[byte] : &ddfd_prefix[byte];
            break;
        case 0xED:
            byte = p_memory->read_byte_ex(pointer++);
            p_decode = &ed_prefix[byte];
            break;
       default:
            p_decode = &no_prefix[byte];
            break;
    }
    p_op += sprintf(p_op, "%s ", p_opcode[p_decode->opcode_id]);
    if (p_decode->p_data){
        for (int i = 0; p_decode->p_data[i]; i++){
            switch (p_decode->p_data[i]){
                case 'p': // Register pair.
                    p_op += sprintf(p_op, "%s", p_op16_1[((byte >> 4) & 0x03)]);
                    break;
                case 'i':
                    p_op += sprintf(p_op, "%s", p_op16_2[((byte >> 4) & 0x03)]);
                    break;
                case 'b': // LD B, #NN
                    p_op += sprintf(p_op, "#%02x", p_memory->read_byte_ex(pointer++));
                    break;
                case 'w': // LD BC, #NNNN
                    byte = p_memory->read_byte_ex(pointer++);
                    p_op += sprintf(p_op, "#%02x%02x", p_memory->read_byte_ex(pointer++), byte);
                    break;
               case 'l': // Relative ptr.  DJNZ, JR.
                    offset = (signed char)p_memory->read_byte_ex(pointer++);
                    p_op += sprintf(p_op, "#%04x", pointer + offset);
                    break;
               case 'y': // Operand 8 bit 5-3.
                    p_op += sprintf(p_op, "%s", p_op8[(byte >> 3) & 0x07]);
                    break;
               case 'z': // Operand 8 bit 2-0.
                    p_op += sprintf(p_op, "%s", p_op8[byte & 0x07]);
                    break;
               case 'f': // Condition flags, bit 5-3.
                    p_op += sprintf(p_op, "%s", p_flag[(byte >> 3) & 0x07]);
                    break;
               case 'c': // Condition flags, bit 5-3.
                    p_op += sprintf(p_op, "%s", p_flag[((byte >> 3) & 0x07) - 4]); //!!!!!!!!!!!!
                    break;
               case 't': // RST
                    p_op += sprintf(p_op, "#%02x", byte & (0x07 << 3));
                    break;
               case 's': // RES 1, B
                    p_op += sprintf(p_op, "%d", (byte >> 3) & 0x07);
                    break;
               case 'x': // IX / IY
                    p_op += sprintf(p_op, "%s", p_idx_reg[idx_reg]);
                    break;
               case 'X': // Read relative pointer.
                    offset = (signed char)p_memory->read_byte_ex(pointer++);
                    break;
               case 'Y': // Out IX+S pointer.
                    p_op += sprintf(p_op, "(%s %c#%02x)", p_idx_reg[idx_reg], offset >=0 ? '+' : '-', offset & 0x7F);
                    break;
               default:
                    *p_op++ = p_decode->p_data[i];
                    break;
            }
        }
    }
    *p_op = 0x00;
    p_bt += sprintf(p_bt, "%04X ", start);
    for (int i = 0; i < pointer - start; i++)
        p_bt += sprintf(p_bt, " %02x", p_memory->read_byte_ex(start + i));
    if (p_bt - p_dst < OPCODE_POS)
        memset(p_bt, ' ', OPCODE_POS - (p_bt - p_dst));
    return pointer;
}

};
