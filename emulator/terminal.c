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
#include <ncurses.h>
#include <stdarg.h>
#include <string.h>

#include "dcpu.h"


struct term_t {
  WINDOW *dbgwin;
  tstamp_t tickns;
  tstamp_t nexttick;
  tstamp_t keyns;
  tstamp_t nextkey;
  u16 keypos;
};

static struct term_t term;

static inline u16 color(int fg, int bg) {
  return COLORS > 8
    ? fg * 16 + bg
    : (fg % 8) * 8 + (bg % 8);
}

void dcpu_initterm(void) {
  term.tickns = 1000000000 / DISPLAY_HZ;
  term.nexttick = dcpu_now();
  term.keyns = 1000000000 / KBD_BAUD;
  term.nextkey = dcpu_now();
  term.keypos = 0;

  // should be done prior to initscr, and it doesn't matter if we do it twice
  initscr();
  start_color();
  cbreak();
  keypad(stdscr, true);
  term.dbgwin = subwin(stdscr, LINES - (SCR_HEIGHT+3), COLS, SCR_HEIGHT+2, 0);
  scrollok(term.dbgwin, true);
  keypad(term.dbgwin, true);

  // set up colors...
  if (COLORS > 8) {
    // nice terminals...
    int colors[] = {0, 4, 2, 6, 1, 5, 3, 7, 8, 12, 10, 14, 9, 13, 11, 15};
    for (int i = 0; i < 16; i++)
      for (int j = 0; j < 16; j++)
        init_pair(color(i, j), colors[i], colors[j]);
  } else {
    // crappy terminals at least get something...
    int colors[] = {0, 4, 2, 6, 1, 5, 3, 7};
    for (int i = 0; i < 8; i++)
      for (int j = 0; j < 8; j++)
        init_pair(color(i, j), colors[i], colors[j]);
  }
}

void dcpu_killterm(void) {
  endwin();
}

void readkey(dcpu *dcpu) {
  int c = getch();
  // always check for ctrl-d, even if kbd buf is full.
  if (c == 0x04) {
    // ctrl-d. exit.
    dcpu_die = true;
    return;
  }
  if (dcpu->ram[KBD_ADDR + term.keypos]) {
    dcpu_msg("refusing to overflow kbd ring buffer!\n");
    ungetch(c);
    return;
  }
  switch (c) {
    case ERR: return; // no key. no problem.

    // remap bs and del to bs, just in case
    case KEY_BACKSPACE: c = 0x08; break;
    case 0x7f: c = 0x08; break;
  }
  dcpu->ram[KBD_ADDR + term.keypos] = c;
  term.keypos += 1;
  term.keypos %= KBD_BUFSIZ;
}

int dcpu_getstr(char *buf, int n) {
  return wgetnstr(term.dbgwin, buf, n) == OK;
}

void dcpu_msg(char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  vwprintw(term.dbgwin, fmt, args);
  wrefresh(term.dbgwin);
  va_end(args);
}

void dcpu_runterm(void) {
  curs_set(0);
  timeout(0);
  noecho();
}

void dcpu_dbgterm(void) {
  curs_set(1);
  timeout(-1);
  echo();
}

static void draw(u16 word, u16 row, u16 col) {
  move(row+1, col+1);

  char letter = word & 0x7f;
  char fg = word >> 12;
  char bg = (word >> 8) & 0xf;
  bool blink = word & 0x80;

  if (!letter) letter = ' ';

  attrset(COLOR_PAIR(color(fg, bg)));
  if (blink) attron(A_BLINK);
  addch(letter);
}

void dcpu_redraw(dcpu *dcpu) {
  u16 *addr = &dcpu->ram[VRAM_ADDR];
  for (u16 i = 0; i < SCR_HEIGHT; i++) 
    for (u16 j = 0; j < SCR_WIDTH; j++)
      draw(*addr++, i, j);
  refresh();
}

void dcpu_termtick(dcpu *dcpu, tstamp_t now) {
  if (now > term.nexttick) {
    dcpu_redraw(dcpu);
    term.nexttick += term.tickns;
  }

  if (now > term.nextkey) {
    readkey(dcpu);
    term.nextkey += term.keyns;
  }
}
