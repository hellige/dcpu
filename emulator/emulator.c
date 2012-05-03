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
#include <time.h>
#if defined(DCPU_MACOSX)
#include <mach/mach_time.h>
#endif

#include "dcpu.h"
#include "opcodes.h"

#define S(x) ((int16_t)(x))

tstamp_t dcpu_now() {
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
  tstamp_t now = dcpu_now();
  dcpu_termtick(dcpu, now);
  if (now < dcpu->nexttick) {
    struct timespec ts = { 0, dcpu->nexttick - now };
    // don't care about failures. if we get a signal, we're gonna bail anyway.
    nanosleep(&ts, NULL);
  }
  dcpu->nexttick += dcpu->tickns;
}


void dcpu_init(dcpu *dcpu, uint32_t khz) {
  dcpu->tickns = 1000000 / khz;

  dcpu->sp = 0;
  dcpu->pc = 0;
  dcpu->ex = 0;
  dcpu->ia = 0;
  for (int i = 0; i < NREGS; i++) dcpu->reg[i] = 0;
  for (int i = 0; i < RAM_WORDS; i++) dcpu->ram[i] = 0;
  dcpu->intqwrite = 0;
  dcpu->intqread = 0;
  dcpu->nhw = 0;
}

bool dcpu_loadcore(dcpu *dcpu, const char *image, bool bigend) {
  FILE *img = fopen(image, "r");
  if (!img) {
    dcpu_msg("error reading image '%s': %s\n", image, strerror(errno));
    return false;
  }

  int img_size = fread(dcpu->ram, 2, RAM_WORDS, img);
  if (ferror(img)) {
    dcpu_msg("error reading image '%s': %s\n", image, strerror(errno));
    return false;
  }

  dcpu_msg("loaded image from %s: 0x%05x words\n", image, img_size);

  // swap byte order...
  if (bigend)
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
    dcpu_msg("error opening image '%s': %s\n", image, strerror(errno));
    return;
  }

  if (limit == 0) limit = RAM_WORDS;

  for (uint32_t i = 0; i < limit; i++) {
    if (fputc(dcpu->ram[i] >> 8, img) == EOF
        || fputc(dcpu->ram[i] & 0xff, img) == EOF) {
      dcpu_msg("error writing to image '%s': %s\n", image, strerror(errno));
      return;
    }
  }

  fclose(img);
}


static action_t enqueue_int(dcpu *dcpu, u16 interrupt) {
  u16 nextwrite = (dcpu->intqwrite+1) % INTQ_SIZE;
  if (nextwrite != dcpu->intqread) {
    dcpu->intq[dcpu->intqwrite] = interrupt;
    dcpu->intqwrite = nextwrite;
    return A_CONTINUE;
  } else {
    dcpu_msg("interrupt queue overflow! discarding: 0x%04x\n", interrupt);
    return A_BREAK;
  }
}

static void trigger_int(dcpu *dcpu) {
  // check if interrupts are unblocked and queue is non-empty
  if (!dcpu->qints && dcpu->intqread != dcpu->intqwrite) {
    if (dcpu->ia != 0) {
      // handler is configured, deliver the interrupt.
      dcpu->qints = true;
      dcpu->ram[--dcpu->sp] = dcpu->pc;
      dcpu->ram[--dcpu->sp] = dcpu->reg[REG_A];
      dcpu->pc = dcpu->ia;
      dcpu->reg[REG_A] = dcpu->intq[dcpu->intqread];
    }
    // and either way, discard the queued interrupt...
    dcpu->intqread++;
    dcpu->intqread  %= INTQ_SIZE;
  }
}


static inline u16 next(dcpu *dcpu, bool effects) {
  // decoding a word generally takes a cycle, *except* when we're decoding
  // args for a skipped instruction. ugly, but that's the spec...
  if (effects) await_tick(dcpu);
  return dcpu->ram[dcpu->pc++];
}


#define ARG_PSHP 0x18
#define ARG_PEEK 0x19
#define ARG_PICK 0x1a
#define ARG_SP   0x1b
#define ARG_PC   0x1c
#define ARG_EX   0x1d
#define ARG_NXA  0x1e  // next word, deref
#define ARG_NXL  0x1f  // next word, literal

static u16 decode_arg(dcpu *dcpu, uint8_t arg, u16 **addr, bool effects,
    bool a) {
  // in case caller doesn't need addr...
  u16 *tmp;
  if (addr == NULL) addr = &tmp;

  // initialize to null, the right result for a literal
  *addr = NULL;

  // literal. no address. range is [-1, 30]
  if (arg & 0x20) return arg - 0x21;

  // special operator
  if (arg & 0x18) {
    switch (arg) {
      case ARG_PSHP:
        if (a) {
          *addr = &dcpu->ram[dcpu->sp];
          if (effects) dcpu->sp++;
          return **addr;
        } else {
          if (effects) dcpu->sp--;
          *addr = &dcpu->ram[dcpu->sp];
          return **addr;
        }
      case ARG_PEEK:
        *addr = &dcpu->ram[dcpu->sp];
        return **addr;
      case ARG_PICK: {
        // compute the address as a separate variable to guarantee wrap on
        // overflow
        u16 address = dcpu->sp + next(dcpu, effects);
        *addr = &dcpu->ram[address];
        return **addr;
      }
      case ARG_SP:
        *addr = &dcpu->sp;
        return **addr;
      case ARG_PC:
        *addr = &dcpu->pc;
        return **addr;
      case ARG_EX:
        *addr = &dcpu->ex;
        return **addr;
      case ARG_NXA:
        *addr = &dcpu->ram[next(dcpu, effects)];
        return **addr;
      case ARG_NXL:
        // literal. no address (by fiat in this case).
        await_tick(dcpu);
        return next(dcpu, effects);
    }
  }

  // register or register[+offset] deref
  uint8_t reg = arg & 0x7;
  if (arg & 0x10) {
    // compute the address as a separate variable to guarantee wrap on overflow
    u16 address = dcpu->reg[reg] + next(dcpu, effects); // TODO: what if reg is PC?
    *addr = &dcpu->ram[address];
  }
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


// decode (but do not execute) next instruction(s)...
static inline void skip(dcpu *dcpu) {
  // this is slightly imperfect. we risk an infinite loop if memory 
  // is nothing but if* instructions. but that's a bit perverse.
  u16 instr;
  do {
    instr = next(dcpu, true);
    decode_arg(dcpu, arg_a(instr), NULL, false, true);
    decode_arg(dcpu, arg_b(instr), NULL, false, false);
  } while (get_opcode(instr) >= OP_IFB && get_opcode(instr) <= OP_IFU);
}


static action_t exec_special(dcpu *dcpu, u16 instr);

static action_t execute(dcpu *dcpu, u16 instr) {
  u16 *dest;
  u16 opcode = get_opcode(instr);

  // dispatch special instruction before decoding args
  if (!opcode) return exec_special(dcpu, instr);

  u16 a = decode_arg(dcpu, arg_a(instr), NULL, true, true);
  u16 b = decode_arg(dcpu, arg_b(instr), &dest, true, false);

  // TODO all these instructions set EX *after* setting destination. this
  // *may* be wrong: http://www.reddit.com/r/dcpu16/comments/t0yps/psa_fix_your_emulators_set_ex_7_add_ex_ex_should/
  // TODO values of EX should be tested for virtually all instructions. i've
  // mostly coded to spec without much testing, and there are bound to be bugs.

  switch (opcode) {
    case OP_SET:
      set(dest, a);
      break;

    case OP_ADD: {
      u16 sum = b + a;
      set(dest, sum);
      dcpu->ex = sum < b ? 0x1 : 0;
      await_tick(dcpu);
      break;
    }

    case OP_SUB:
      set(dest, b - a);
      dcpu->ex = b < a ? 0xffff : 0;
      await_tick(dcpu);
      break;

    case OP_MUL:
      set(dest, b * a);
      dcpu->ex = ((b * a) >> 16) & 0xffff; // per spec
      await_tick(dcpu);
      break;

    case OP_MLI:
      set(dest, S(b) * S(a));
      dcpu->ex = ((S(b) * S(a)) >> 16) & 0xffff; // per spec
      await_tick(dcpu);
      break;

    case OP_DIV:
      if (a == 0) {
        set(dest, 0);
        dcpu->ex = 0;
      } else {
        set(dest, b / a);
        dcpu->ex = ((b << 16) / a) & 0xffff; // per spec
      }
      await_tick(dcpu);
      await_tick(dcpu);
      break;

    case OP_DVI:
      if (a == 0) {
        set(dest, 0);
        dcpu->ex = 0;
      } else {
        // TODO this should be enforced manually, but gcc/x86 does what we want...
        set(dest, S(b) / S(a));
        dcpu->ex = ((S(b) << 16) / S(a)) & 0xffff; // per spec
      }
      await_tick(dcpu);
      await_tick(dcpu);
      break;

    case OP_MOD:
      if (a == 0)
        set(dest, 0);
      else
        set(dest, b % a);
      await_tick(dcpu);
      await_tick(dcpu);
      break;

    case OP_MDI:
      if (a == 0)
        set(dest, 0);
      else
        // TODO this should be enforced manually, but gcc/x86 does what we want...
        set(dest, S(b) % S(a));
      await_tick(dcpu);
      await_tick(dcpu);
      break;

    case OP_AND:
      set(dest, b & a);
      break;

    case OP_BOR:
      set(dest, b | a);
      break;

    case OP_XOR:
      set(dest, b ^ a);
      break;

    case OP_SHR:
      set(dest, b >> a);
      dcpu->ex = ((b << 16) >> a) & 0xffff; // per spec
      break;

    case OP_ASR:
      // TODO this should be enforced manually, but gcc/x86 does what we want...
      set(dest, S(b) >> a);
      dcpu->ex = ((S(b) << 16) >> a) & 0xffff; // per spec
      break;

    case OP_SHL:
      set(dest, b << a);
      dcpu->ex = ((b << a) >> 16) & 0xffff; // per spec
      break;

    case OP_IFB:
      if (!(b & a)) skip(dcpu);
      await_tick(dcpu);
      break;

    case OP_IFC:
      if (b & a) skip(dcpu);
      await_tick(dcpu);
      break;

    case OP_IFE:
      if (!(b == a)) skip(dcpu);
      await_tick(dcpu);
      break;

    case OP_IFN:
      if (!(b != a)) skip(dcpu);
      await_tick(dcpu);
      break;

    case OP_IFG:
      if (!(b > a)) skip(dcpu);
      await_tick(dcpu);
      break;

    case OP_IFA:
      if (!(S(b) > S(a))) skip(dcpu);
      await_tick(dcpu);
      break;

    case OP_IFL:
      if (!(b < a)) skip(dcpu);
      await_tick(dcpu);
      break;

    case OP_IFU:
      if (!(S(b) < S(a))) skip(dcpu);
      await_tick(dcpu);
      break;

    case OP_ADX: {
      u16 sum = b + a + dcpu->ex;
      set(dest, sum);
      dcpu->ex = sum < b ? 0x1 : 0; // TODO wrong
      await_tick(dcpu);
      await_tick(dcpu);
      break;
    }

    case OP_SBX:
      set(dest, b - a + dcpu->ex);
      dcpu->ex = b < a ? 0xffff : 0; // TODO wrong
      await_tick(dcpu);
      await_tick(dcpu);
      break;

    case OP_STI:
      set(dest, a);
      dcpu->reg[REG_I]++;
      dcpu->reg[REG_J]++;
      await_tick(dcpu);
      break;

    case OP_STD:
      set(dest, a);
      dcpu->reg[REG_I]--;
      dcpu->reg[REG_J]--;
      await_tick(dcpu);
      break;

    default:
      dcpu_msg("reserved instruction: 0x%04x, pc now 0x%04x.\n",
        opcode, dcpu->pc);
      return A_BREAK;
  }

  return A_CONTINUE;
}


static action_t exec_special(dcpu *dcpu, u16 instr) {
  uint8_t opcode = arg_b(instr);
  u16 *dest;
  u16 a = decode_arg(dcpu, arg_a(instr), &dest, true, true);

  switch (opcode) {
    case OP_SP_JSR:
      dcpu->ram[--dcpu->sp] = dcpu->pc;
      dcpu->pc = a;
      break;

    case OP_SP_IMG:
      dcpu_coredump(dcpu, a);
      break;

    case OP_SP_DIE:
      return A_EXIT;

    case OP_SP_DBG:
      return A_BREAK;

    case OP_SP_INT:
      await_tick(dcpu);
      await_tick(dcpu);
      await_tick(dcpu);
      return enqueue_int(dcpu, a);

    case OP_SP_IAG:
      set(dest, dcpu->ia);
      break;

    case OP_SP_IAS:
      dcpu->ia = a;
      break;

    case OP_SP_RFI:
      dcpu->qints = false;
      dcpu->reg[REG_A] = dcpu->ram[dcpu->sp++];
      dcpu->pc = dcpu->ram[dcpu->sp++];
      await_tick(dcpu);
      await_tick(dcpu);
      break;

    case OP_SP_IAQ:
      dcpu->qints = a;
      await_tick(dcpu);
      break;

    case OP_SP_HWN:
      set(dest, dcpu->nhw);
      await_tick(dcpu);
      break;

    case OP_SP_HWQ:
      if (a < dcpu->nhw) {
        uint32_t hwid = dcpu->hw[a].id;
        uint32_t hwmfr = dcpu->hw[a].mfr;
        dcpu->reg[REG_A] = hwid & 0xffff;
        dcpu->reg[REG_B] = hwid >> 16;
        dcpu->reg[REG_C] = dcpu->hw[a].version;
        dcpu->reg[REG_X] = hwmfr & 0xffff;
        dcpu->reg[REG_Y] = hwmfr >> 16;
      }
      await_tick(dcpu);
      await_tick(dcpu);
      await_tick(dcpu);
      break;

    case OP_SP_HWI:
      if (a < dcpu->nhw) {
        u16 cycles = dcpu->hw[a].hwi(dcpu);
        while (cycles--) await_tick(dcpu);
      }
      await_tick(dcpu);
      await_tick(dcpu);
      await_tick(dcpu);
      break;

    default:
      dcpu_msg("reserved non-basic instruction: 0x%04x, pc now 0x%04x.\n",
        opcode, dcpu->pc);
      return A_BREAK;
  }

  return A_CONTINUE;
}


action_t dcpu_step(dcpu *dcpu) {
  u16 oldpc = dcpu->pc;
  u16 instr = next(dcpu, true);
  int result = execute(dcpu, instr);
  trigger_int(dcpu);
  if (dcpu->detect_loops && dcpu->pc == oldpc) {
    dcpu_msg("loop detected.\n");
    return A_BREAK;
  }
  return result;
}


void dcpu_run(dcpu *dcpu, bool debugboot) {
  bool running = true;
  if (debugboot) running = dcpu_debug(dcpu);
  dcpu_msg("running...\n");
  dcpu_runterm();
  dcpu->nexttick = dcpu_now() + dcpu->tickns;
  while (running && !dcpu_die) {
    action_t action = dcpu_step(dcpu);
    if (action == A_EXIT) running = false;
    if (action == A_BREAK || dcpu_break) {
      dcpu_break = false;
      dcpu_redraw(dcpu); // force a vram redraw before entering debugger
      dcpu_dbgterm();
      running = dcpu_debug(dcpu);
      if (running) dcpu_msg("running...\n");
      dcpu_runterm();
    }
  }
  dcpu_dbgterm();
}
