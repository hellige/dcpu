/*
 * Copyright (c) 2012, Matt Hellige
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

#ifndef dcpu_h
#define dcpu_h

#include <stdint.h>
#include <stdbool.h>


typedef uint16_t u16;

#define DCPU_VERSION  "1.1-mh"
#define DCPU_MODS     "+out +kbd +img +die"
#define COREFILE_NAME "core.img"

#define RAM_WORDS 0x10000
// A, B, C, X, Y, Z, I, J
#define NREGS     8

typedef struct term_t term;

typedef struct dcpu_t {
  u16 sp;
  u16 pc;
  u16 o;
  u16 reg[NREGS];
  u16 ram[RAM_WORDS];
  term *term;
} dcpu;

typedef enum {
  A_CONTINUE,
  A_BREAK,
  A_EXIT
} action_t;


// disassembler.c
extern u16 *disassemble(u16 *pc, char *out);

// emulate.c
extern bool dcpu_init(dcpu *dcpu, const char *image);
extern void dcpu_coredump(dcpu *dcpu, uint32_t limit);
extern void dcpu_run(dcpu *dcpu);
extern action_t dcpu_step(dcpu *dcpu);

// debug.c
extern bool dcpu_debug(dcpu *dcpu);

// terminal.c
extern void dcpu_initterm(dcpu *dcpu);
extern void dcpu_runterm(dcpu *dcpu);
extern void dcpu_dbgterm(dcpu *dcpu);
extern void dcpu_killterm(dcpu *dcpu);
extern volatile bool dcpu_break;


#endif
