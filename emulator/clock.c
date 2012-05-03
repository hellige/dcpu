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

#include "dcpu.h"

struct clock_t {
  tstamp_t tickns;
  tstamp_t nexttick;
  u16 msg;
  u16 ticks;
};

static struct clock_t clock;

static void set_rate(u16 rate) {
  // TODO can the clock tick at < 1hz, say once every two seconds? the spec is totally unclear.
  if (rate == 0) {
    // disable clock
    clock.tickns = 0;
    clock.nexttick = UINT64_MAX;
  } else {
    u16 hz = CLOCKDEV_HZ / rate;
    clock.tickns = 1000000000 / hz;
    clock.nexttick = dcpu_now() + clock.tickns;
  }
  clock.ticks = 0;
}

static u16 clock_hwi(dcpu *dcpu) {
  switch (dcpu->reg[REG_A]) {
    case 0:
      set_rate(dcpu->reg[REG_B]);
      break;
    case 1:
      dcpu->reg[REG_C] = clock.ticks;
      break;
    case 2:
      clock.msg = dcpu->reg[REG_B];
      break;
  }
  return 0; // no extra cycles
}

static void clock_tick(dcpu *dcpu, tstamp_t now) {
  if (now > clock.nexttick) {
    clock.ticks++;
    if (clock.msg) dcpu_interrupt(dcpu, clock.msg);
    clock.nexttick += clock.tickns;
  }
}

void dcpu_initclock(dcpu *dcpu) {
  // it's unspecified what state the clock is in prior to the first hwi. we'll
  // just have it turned off.
  set_rate(0);
  clock.msg = 0;

  // set up hardware descriptors
  device *dev = dcpu_addhw(dcpu);
  dev->id = 0x12d0b402;
  dev->version = 1;
  dev->mfr = 0x01220423;
  dev->hwi = &clock_hwi;
  dev->tick = &clock_tick;
}
