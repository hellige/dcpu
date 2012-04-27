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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dcpu.h"


static bool prefix(char *pre, char *full) {
  return !strncasecmp(pre, full, strlen(pre));
}

static bool matches(char *tok, char *min, char *full) {
  return prefix(min, tok) && prefix(tok, full);
}

static void dumpram(dcpu *dcpu, u16 addr, int len) {
  while (len > 0) {
    u16 base = addr & ~7;
    dcpu_msg("\n%04x:", base);
    int pad = addr % 8;
    dcpu_msg("%*s", 5 * pad, "");
    do {
      dcpu_msg(" %04x", dcpu->ram[addr++]);
      len--;
    } while (len && addr % 8);
  }
  dcpu_msg("\n");
}

static void dumpheader(void) {
  dcpu_msg(
      "pc   sp   ex   ia   a    b    c    x    y    z    i    j    instruction\n"
      "---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- -----------\n");
}

static void dumpstate(dcpu *d) {
  char out[128];
  dcpu_disassemble(d->ram + d->pc, out);
  dcpu_msg(
      "%04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %s\n",
      d->pc, d->sp, d->ex, d->ia,
      d->reg[0], d->reg[1], d->reg[2], d->reg[3],
      d->reg[4], d->reg[5], d->reg[6], d->reg[7],
      out);
}


bool dcpu_debug(dcpu *dcpu) {
  static char buf[BUFSIZ];
  dcpu_msg("entering emulator debugger: enter 'h' for help.\n");
  dumpheader();
  dumpstate(dcpu);
  for (;;) {
    dcpu_msg(" * ");
    if (!dcpu_getstr(buf, BUFSIZ)) return false;

    char *delim = " \t\n";
    char *tok = strtok(buf, delim);
    if (!tok) continue;
    if (matches(tok, "h", "help")
        || matches(tok, "?", "?")) {
      dcpu_msg(
          "  help, ?: show this message\n"
          "  continue: resume running\n"
          "  step [n]: execute a single instruction (or n instructions)\n"
          "  dump: display the state of the cpu\n"
          "  print addr [len]: display memory contents in hex\n"
          "      (addr and len are both hex)\n"
          "  core: dump ram image to core.img\n"
          "  exit, quit: exit emulator\n"
          "unambiguous abbreviations are recognized "
            "(e.g., s for step or con for continue).\n"
          );
    } else if (matches(tok, "con", "continue")) {
      return true;
    } else if (matches(tok, "s", "step")) {
      uint32_t steps = 1;
      tok = strtok(NULL, delim);
      if (tok) {
        char *endptr;
        steps = strtoul(tok, &endptr, 10);
        if (*endptr) {
          dcpu_msg("argument to 'step' must be a decimal number\n");
          continue;
        }
      }
      for (uint32_t i = 0; i < steps; i++) {
        dcpu_runterm();
        dcpu_step(dcpu);
        dcpu_dbgterm();
        dumpstate(dcpu);
      }
    } else if (matches(tok, "d", "dump")) {
      dumpheader();
      dumpstate(dcpu);
    } else if (matches(tok, "p", "print")) {
      tok = strtok(NULL, delim);
      if (!tok) {
        dcpu_msg("print requires an argument\n");
        continue;
      }
      char *endptr;
      u16 addr = strtoul(tok, &endptr, 16);
      if (*endptr) {
        dcpu_msg("addr argument to 'print' must be a hex number: %s\n", endptr);
        continue;
      }
      u16 length = 1;
      tok = strtok(NULL, delim);
      if (tok) {
        length = strtoul(tok, &endptr, 16);
        if (*endptr) {
          dcpu_msg("len argument to 'print' must be a hex number\n");
          continue;
        }
      }
      dumpram(dcpu, addr, length);
    } else if (matches(tok, "cor", "core")) {
      dcpu_coredump(dcpu, 0);
      dcpu_msg("core written to core.img\n");
    } else if (matches(tok, "e", "exit")
        || matches(tok, "q", "quit")) {
      return false;
    } else {
      dcpu_msg("unrecognized or ambiguous command: %s\n", tok);
    }
  }
}

