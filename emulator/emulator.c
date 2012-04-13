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

#include <errno.h>
#include <sched.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#if defined(DCPU_LINUX)
#include <time.h>
#elif defined(DCPU_MACOSX)
#include <mach/mach_time.h>
#endif

#include "dcpu.h"
#include "opcodes.h"

#define OP_MASK   0x7
#define OP_SIZE   4
#define ARG_MASK  0x3f
#define ARG_SIZE  6


static inline tstamp_t now() {
#if defined(DCPU_LINUX)
  struct timespec tp;
  clock_gettime(CLOCK_MONOTONIC, &tp);
  uint64_t nanos = 1000000000;
  nanos *= tp.tv_sec;
  return nanos + tp.tv_nsec;
#elif defined(DCPU_MACOSX)
  static mach_timebase_info_data_t info = {0,0};
  if (!info.denom) mach_timebase_info(&info);
  uint64_t t = mach_absolute_time();
  return t * (info.numer/info.denom);
#else
#error i have no timer for this platform.
#endif
}


static inline void await_tick(dcpu *dcpu) {
  struct timespec ts = { 0, dcpu->nexttick - now() };
  // don't care about failures. if we get a signal, we're gonna bail anyway.
  nanosleep(&ts, NULL);
  dcpu->nexttick += dcpu->tickns;
}


bool dcpu_init(dcpu *dcpu, const char *image, uint32_t khz) {
  dcpu->tickns = 1000000 / khz;

  dcpu->sp = 0;
  dcpu->pc = 0;
  dcpu->o = 0;
  for (int i = 0; i < NREGS; i++) dcpu->reg[i] = 0;
  for (int i = 0; i < RAM_WORDS; i++) dcpu->ram[i] = 0;

  FILE *img = fopen(image, "r");
  if (!img) {
    fprintf(stderr, "error reading image '%s': %s\n", image, strerror(errno));
    return false;
  }

  int img_size = fread(dcpu->ram, 2, RAM_WORDS, img);
  if (ferror(img)) {
    fprintf(stderr, "error reading image '%s': %s\n", image, strerror(errno));
    return false;
  }

  printf("loaded image from %s: 0x%05x words\n", image, img_size);

  // swap byte order...
  for (int i = 0; i < RAM_WORDS; i++)
    dcpu->ram[i] = (dcpu->ram[i] >> 8) | ((dcpu->ram[i] & 0xff) << 8);

  fclose(img);
  return true;
}


// note limit is 32-bit, so that we can distinguish 0 from RAM_WORDS
void dcpu_coredump(dcpu *dcpu, uint32_t limit) {
  char *image = COREFILE_NAME;
  FILE *img = fopen(image, "w");
  if (!img) {
    fprintf(stderr, "error opening image '%s': %s\n", image, strerror(errno));
    return;
  }

  if (limit == 0) limit = RAM_WORDS;

  for (uint32_t i = 0; i < limit; i++) {
    if (fputc(dcpu->ram[i] >> 8, img) == EOF
        || fputc(dcpu->ram[i] & 0xff, img) == EOF) {
      fprintf(stderr, "error writing to image '%s': %s\n", image, strerror(errno));
      return;
    }
  }

  fclose(img);
}


static inline u16 next(dcpu *dcpu, bool effects) {
  // decoding a word generally takes a cycle, *except* when we're decoding
  // args for a skipped instruction. ugly, but that's the spec...
  if (effects) await_tick(dcpu);
  return dcpu->ram[dcpu->pc++];
}

static inline uint8_t opcode(u16 instr) {
  return instr & 0xf;
}

static inline uint8_t arg_a(u16 instr) {
  return (instr >> OP_SIZE) & ARG_MASK;
}

static inline uint8_t arg_b(u16 instr) {
  return (instr >> (OP_SIZE + ARG_SIZE)) & ARG_MASK;
}


#define ARG_POP  0x18
#define ARG_PEEK 0x19
#define ARG_PUSH 0x1a
#define ARG_SP   0x1b
#define ARG_PC   0x1c
#define ARG_O    0x1d
#define ARG_NXA  0x1e  // next word, deref
#define ARG_NXL  0x1f  // next word, literal

static u16 decode_arg(dcpu *dcpu, uint8_t arg, u16 **addr, bool effects) {
  // in case caller doesn't need addr...
  u16 *tmp;
  if (addr == NULL) addr = &tmp;

  // initialize to null, the right result for a literal
  *addr = NULL;

  // literal. no address.
  if (arg & 0x20) return arg - 0x20;

  // special operator
  if (arg & 0x18) {
    switch (arg) {
      case ARG_POP:
        *addr = &dcpu->ram[dcpu->sp];
        if (effects) dcpu->sp++;
        return **addr;
      case ARG_PEEK:
        *addr = &dcpu->ram[dcpu->sp];
        return **addr;
      case ARG_PUSH:
        if (effects) dcpu->sp--;
        *addr = &dcpu->ram[dcpu->sp];
        return **addr;
      case ARG_SP:
        *addr = &dcpu->sp;
        return **addr;
      case ARG_PC:
        *addr = &dcpu->pc;
        return **addr;
      case ARG_O:
        *addr = &dcpu->o;
        return **addr;
      case ARG_NXA:
        *addr = &dcpu->ram[next(dcpu, effects)];
        return **addr;
      case ARG_NXL:
        // literal. no address (by fiat in this case) TODO is that right?
        await_tick(dcpu);
        return next(dcpu, effects);
    }
  }

  // register or register[+offset] deref
  uint8_t reg = arg & 0x7;
  if (arg & 0x10)
    *addr = &dcpu->ram[dcpu->reg[reg] + next(dcpu, effects)]; // TODO: what if reg is PC?
  else if (arg & 0x8)
    *addr = &dcpu->ram[dcpu->reg[reg]];
  else
    *addr = &dcpu->reg[reg];
  return **addr;
}


static inline void set(u16 *dest, u16 val) {
  if (dest) *dest = val;
  // otherwise, attempt to write a literal: a silent fault.
}


// decode (but do not execute) next instruction...
static inline void skip(dcpu *dcpu) {
  u16 instr = next(dcpu, true);
  decode_arg(dcpu, arg_a(instr), NULL, false);
  decode_arg(dcpu, arg_b(instr), NULL, false);
}


static void exec_basic(dcpu *dcpu, u16 instr) {
  u16 *dest;
  u16 a = decode_arg(dcpu, arg_a(instr), &dest, true);
  u16 b = decode_arg(dcpu, arg_b(instr), NULL, true);

  switch (opcode(instr)) {
    case OP_SET:
      set(dest, b);
      break;

    case OP_ADD: {
      u16 sum = a + b;
      set(dest, sum);
      dcpu->o = sum < a ? 0x1 : 0;
      await_tick(dcpu);
      break;
    }

    case OP_SUB:
      set(dest, a - b);
      dcpu->o = a < b ? 0xffff : 0;
      await_tick(dcpu);
      break;

    case OP_MUL:
      set(dest, a * b);
      dcpu->o = ((a * b) >> 16) & 0xffff; // per spec
      await_tick(dcpu);
      break;

    case OP_DIV:
      if (b == 0) {
        set(dest, 0);
        dcpu->o = 0;
      } else {
        set(dest, a / b);
        dcpu->o = ((a << 16) / b) & 0xffff; // per spec
      }
      await_tick(dcpu);
      await_tick(dcpu);
      break;

    case OP_MOD:
      if (b == 0)
        set(dest, 0);
      else
        set(dest, a % b);
      await_tick(dcpu);
      await_tick(dcpu);
      break;

    case OP_SHL:
      set(dest, a << b);
      dcpu->o = ((a << b) >> 16) & 0xffff; // per spec
      await_tick(dcpu);
      break;

    case OP_SHR:
      set(dest, a >> b);
      dcpu->o = ((a << 16) >> b) & 0xffff; // per spec
      await_tick(dcpu);
      break;

    case OP_AND:
      set(dest, a & b);
      break;

    case OP_BOR:
      set(dest, a | b);
      break;

    case OP_XOR:
      set(dest, a ^ b);
      break;

    case OP_IFE:
      if (!(a == b)) skip(dcpu);
      await_tick(dcpu);
      break;

    case OP_IFN:
      if (!(a != b)) skip(dcpu);
      await_tick(dcpu);
      break;

    case OP_IFG:
      if (!(a > b)) skip(dcpu);
      await_tick(dcpu);
      break;

    case OP_IFB:
      if (!(a & b)) skip(dcpu);
      await_tick(dcpu);
      break;
  }
}


static action_t exec_nonbasic(dcpu *dcpu, u16 instr) {
  uint8_t opcode = arg_a(instr);
  u16 *dest;
  u16 a = decode_arg(dcpu, arg_b(instr), &dest, true);

  switch (opcode) {
    case OP_NB_JSR:
      dcpu->ram[--dcpu->sp] = dcpu->pc;
      dcpu->pc = a;
      break;

    case OP_NB_OUT:
      putchar(a);
      fflush(stdout);
      break;

    case OP_NB_KBD: {
      // will set dest to -1 if no key is available...
      int c = getchar();
      // since we're non-canon, we need to check for ctrl-d ourselves...
      if (c == 0x04) return A_EXIT;
      // we'll also map delete to backspace to avoid goofy terminals...
      if (c == 0x7f) c = 0x08;
      set(dest, c);
      break;
    }

    case OP_NB_IMG:
      dcpu_coredump(dcpu, a);
      break;

    case OP_NB_DIE:
      return A_EXIT;

    case OP_NB_DBG:
      return A_BREAK;

    default:
      fprintf(stderr, "\nreserved instruction: 0x%04x, pc now 0x%04x.",
        opcode, dcpu->pc);
      return A_BREAK;
  }

  return A_CONTINUE;
}


action_t dcpu_step(dcpu *dcpu) {
  u16 instr = next(dcpu, true);
  int result = A_CONTINUE;
  if (opcode(instr) == OP_NON)
    result = exec_nonbasic(dcpu, instr);
  else
    exec_basic(dcpu, instr);
  return result;
}


void dcpu_run(dcpu *dcpu) {
  dcpu_runterm(dcpu);
  dcpu->nexttick = now() + dcpu->tickns;
  bool running = true;
  while (running) {
    action_t action = dcpu_step(dcpu);
    if (action == A_EXIT) running = false;
    if (action == A_BREAK || dcpu_break) {
      dcpu_break = false;
      dcpu_dbgterm(dcpu);
      running = dcpu_debug(dcpu);
      dcpu_runterm(dcpu);
    }
  }
  dcpu_dbgterm(dcpu);
}
