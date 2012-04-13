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
#include <string.h>

#include "dcpu.h"


static bool prefix(char *pre, char *full) {
  return !strncasecmp(pre, full, strlen(pre));
}

static bool matches(char *tok, char *min, char *full) {
  return prefix(min, tok) && prefix(tok, full);
}


static void dumpheader(void) {
  printf(
      "pc   sp   o    a    b    c    x    y    z    i    j    instruction\n"
      "---- ---- ---- ---- ---- ---- ---- ---- ---- ---- ---- -----------\n");
}

static void dumpstate(dcpu *d) {
  char out[128];
  disassemble(d->ram + d->pc, out);
  printf(
      "%04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %s\n",
      d->pc, d->sp, d->o,
      d->reg[0], d->reg[1], d->reg[2], d->reg[3],
      d->reg[4], d->reg[5], d->reg[6], d->reg[7],
      out);
}


bool dcpu_debug(dcpu *dcpu) {
  static char buf[BUFSIZ];
  puts("\nentering emulator debugger: enter 'h' for help.");
  dumpheader();
  dumpstate(dcpu);
  for (;;) {
    printf(" * ");
    if (!fgets(buf, BUFSIZ, stdin)) return false;

    char *delim = " \t\n";
    char *tok = strtok(buf, delim);
    if (!tok) continue;
    if (matches(tok, "h", "help")
        || matches(tok, "?", "?")) {
      printf(
          "  help, ?: show this message\n"
          "  continue: resume running\n"
          "  step: execute a single instruction\n"
          "  dump: display the state of the cpu\n"
          "  core: dump ram image to core.img\n"
          "  exit, quit: exit emulator\n"
          "unambiguous abbreviations are recognized "
            "(e.g., s for step or con for continue).\n"
          );
    } else if (matches(tok, "con", "continue")) {
      return true;
    } else if (matches(tok, "s", "step")) {
      dcpu_runterm(dcpu);
      dcpu_step(dcpu);
      dcpu_dbgterm(dcpu);
      dumpstate(dcpu);
    } else if (matches(tok, "d", "dump")) {
      dumpheader();
      dumpstate(dcpu);
    } else if (matches(tok, "cor", "core")) {
      dcpu_coredump(dcpu, 0);
      puts("core written to core.img");
    } else if (matches(tok, "e", "exit")
        || matches(tok, "q", "quit")) {
      return false;
    } else {
      printf("unrecognized or ambiguous command: %s\n", tok);
    }
  }
}

