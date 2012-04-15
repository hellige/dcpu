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
typedef uint64_t tstamp_t;

#define DCPU_VERSION  "1.1-mh"
#define DCPU_MODS     "+img +die"
#define COREFILE_NAME "core.img"
#define DEFAULT_KHZ   150

#define RAM_WORDS 0x10000
// A, B, C, X, Y, Z, I, J
#define NREGS     8

#define DISPLAY_HZ    60
#define VRAM_ADDR     0x8000
#define SCR_HEIGHT    16
#define SCR_WIDTH     32
#define KBD_BAUD      2000
#define KBD_ADDR      0x9000
#define KBD_BUFSIZ    16

typedef struct dcpu_t {
  bool detect_loops;
  int tickns;
  tstamp_t nexttick;
  u16 sp;
  u16 pc;
  u16 o;
  u16 reg[NREGS];
  u16 ram[RAM_WORDS];
} dcpu;

typedef enum {
  A_CONTINUE,
  A_BREAK,
  A_EXIT
} action_t;


// disassembler.c
extern u16 *disassemble(u16 *pc, char *out);

// emulate.c
extern tstamp_t dcpu_now();
extern bool dcpu_init(dcpu *dcpu, const char *image, uint32_t khz,
  bool bigend);
extern void dcpu_coredump(dcpu *dcpu, uint32_t limit);
extern void dcpu_run(dcpu *dcpu, bool debugboot);
extern action_t dcpu_step(dcpu *dcpu);

// debug.c
extern bool dcpu_debug(dcpu *dcpu);

// terminal.c
extern void dcpu_initterm(void);
extern void dcpu_msg(char *fmt, ...)
  __attribute__ ((format (printf, 1, 2)));
extern int dcpu_getch(void);
extern int dcpu_getstr(char *buf, int n);
extern void dcpu_redraw(dcpu *dcpu);
extern void dcpu_termtick(dcpu *dcpu, tstamp_t now);
extern void dcpu_runterm(void);
extern void dcpu_dbgterm(void);
extern void dcpu_killterm(void);
extern volatile bool dcpu_break;
extern volatile bool dcpu_die;


#endif
