/*
 * Copyright (c) 2012, Brian Swetland, Matt Hellige
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 *   Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright 
 *   notice, this list of conditions and the following disclaimer in the 
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* A DCPU-16 Disassembler */

/* DCPU-16 Spec is Copyright 2012 Mojang */

#include <stdio.h>
#include <string.h>

#include "dcpu.h"
#include "opcodes.h"

static const char regs[8] = "abcxyzij";

static u16 *dis_operand(u16 *pc, u16 n, char *out, char *pshp) {
  if (n < 0x08) {
    sprintf(out, "%c", regs[n & 7]);
  } else if (n < 0x10) {
    sprintf(out, "[%c]", regs[n & 7]);
  } else if (n < 0x18) {
    sprintf(out, "[0x%x+%c]", *pc++, regs[n & 7]);
  } else if (n > 0x1f) {
    sprintf(out, "0x%x", (u16)(n - 0x21));
  } else switch (n) {
    case 0x18: strcpy(out, pshp); break;
    case 0x19: strcpy(out, "peek"); break;
    case 0x1a: sprintf(out, "[0x%x+sp]", *pc++); break;
    case 0x1b: strcpy(out, "sp"); break;
    case 0x1c: strcpy(out, "pc"); break;
    case 0x1d: strcpy(out, "ex"); break;
    case 0x1e: sprintf(out, "[0x%04x]", *pc++); break;
    case 0x1f: sprintf(out, "0x%x", *pc++); break;
  }
  return pc;  
}

u16 *disassemble(u16 *pc, char *out) {
  u16 n = *pc++;
  u16 op = get_opcode(n);
  u16 b = arg_b(n);
  u16 a = arg_a(n);
  if (op > 0) {
    sprintf(out, "%s ", opnames[op]);
    pc = dis_operand(pc, b, out+strlen(out), "push");
    sprintf(out+strlen(out),", ");
    pc = dis_operand(pc, a, out+strlen(out), "pop"); 
    return pc;
  }
  if (b > 0 && b < NUM_NBOPCODES && nbopnames[b]) {
    sprintf(out, "%s ", nbopnames[b]);
    pc = dis_operand(pc, a, out+strlen(out), "pop");
    return pc;
  }
  sprintf(out,"unk[%02x] ", b);
  pc = dis_operand(pc, a, out+strlen(out), "pop");
  return pc;
}

