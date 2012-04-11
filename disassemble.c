#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef uint16_t u16;

static const char *opcode[] = {
  "xxx", "mov", "add", "sub", "mul", "div", "mod", "shl",
  "shr", "and", "bor", "xor", "ife", "ifn", "ifg", "ifb",
};

static const char *nb_opcode[] = {
  "xxx", "jsr", "out", "kbd", "img", "die", "dbg",
};

static const char regs[8] = "abcxyzij";

static u16 *dis_operand(u16 *pc, u16 n, char *out) {
  if (n < 0x08) {
    sprintf(out,"%c",regs[n & 7]);
  } else if (n < 0x10) {
    sprintf(out,"[%c]",regs[n & 7]);
  } else if (n < 0x18) {
    sprintf(out,"[%c, 0x%04x]",regs[n & 7], *pc++);
  } else if (n > 0x1f) {
    sprintf(out,"%d", n - 0x20);
  } else switch (n) {
    case 0x18: strcpy(out,"pop"); break;
    case 0x19: strcpy(out,"peek"); break;
    case 0x1A: strcpy(out,"push"); break;
    case 0x1B: strcpy(out,"sp"); break;
    case 0x1C: strcpy(out,"pc"); break;
    case 0x1D: strcpy(out,"o"); break;
    case 0x1e: sprintf(out,"[0x%04x]",*pc++); break;
    case 0x1f: sprintf(out,"0x%04x",*pc++); break;
  }
  return pc;  
}

u16 *disassemble(u16 *pc, char *out) {
  u16 n = *pc++;
  u16 op = n & 0xF;
  u16 a = (n >> 4) & 0x3F;
  u16 b = (n >> 10);
  if (op > 0) {
    if ((n & 0x03FF) == 0x1C1) {
      sprintf(out,"jmp ");
      pc = dis_operand(pc, b, out+strlen(out));
      return pc;
    }
    sprintf(out, "%s ", opcode[op]);
    pc = dis_operand(pc, a, out+strlen(out));
    sprintf(out+strlen(out),", ");
    pc = dis_operand(pc, b, out+strlen(out)); 
    return pc;
  }
  if (a > 0 && a < 7) {
    sprintf(out, "%s ", nb_opcode[a]);
    pc = dis_operand(pc, b, out+strlen(out));
    return pc;
  }
  sprintf(out,"unk[%02x] ", a);
  pc = dis_operand(pc, b, out+strlen(out));
  return pc;
}

