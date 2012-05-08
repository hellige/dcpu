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

// this number is relatively arbitrary, but it really doesn't matter
// what it is. the terminal driver will continue to buffer keys anyway,
// we just won't trigger interrupts for them until the keybuf is
// drained a bit.
#define KEYBUF_SIZE 256

struct term_t {
  WINDOW *border;
  WINDOW *vidwin;
  WINDOW *dbgwin;
  tstamp_t tickns;
  tstamp_t nexttick;
  tstamp_t keyns;
  tstamp_t nextkey;
  u16 vram;
  u16 curborder;
  u16 nextborder;
  int keybuf[KEYBUF_SIZE];
  int keybufwrite;
  int keybufread;
  u16 kbdints;
};

static struct term_t term;

static inline u16 color(int fg, int bg) {
  return COLORS > 8
    ? fg * 16 + bg + 1
    : (fg % 8) * 8 + (bg % 8) + 1;
}

static void checkkey(dcpu *dcpu) {
  int nextwrite = (term.keybufwrite+1) % KEYBUF_SIZE;
  if (nextwrite != term.keybufread) {
    // buf has an empty slot, try to use it...
    int c = getch();
    if (c == ERR) return; // no key, no problem.

    // enqueue key, raise interrupt if possible
    term.keybuf[term.keybufwrite] = c;
    if (term.kbdints) dcpu_interrupt(dcpu, term.kbdints);
    term.keybufwrite = nextwrite;
  }
}

static void readkey(dcpu *dcpu) {
  int c = 0;
  if (term.keybufread != term.keybufwrite) {
    c = term.keybuf[term.keybufread++];
    term.keybufread %= KEYBUF_SIZE;
  }
  switch (c) {
    // these key codes are non-ascii, obviously, per the keyboard spec.
    // also, certain keys aren't easily supported: insert, for instance, 
    // or detection of shift/control by themselves. so this support is only 
    // partial.
    // we remap bs and del to bs, just in case. termio is a pain.
    case KEY_BACKSPACE: c = 0x10; break;
    case 0x7f: c = 0x10; break;
    // likewise for KEY_ENTER and \n, which should be the same, sort of, but
    // aren't.
    case KEY_ENTER: c = 0x11; break;
    case '\n': c = 0x11; break;
    case KEY_UP: c = 0x80; break;
    case KEY_DOWN: c = 0x81; break;
    case KEY_LEFT: c = 0x82; break;
    case KEY_RIGHT: c = 0x83; break;
  }
  dcpu->reg[REG_C] = c; // always set c
}

static u16 kbd_hwi(dcpu *dcpu) {
  switch (dcpu->reg[REG_A]) {
    case 0:
      // clear kbd buf. of course the terminal could still have stuff buffered.
      term.keybufwrite = 0;
      term.keybufread = 0;
      break;
    case 1:
      readkey(dcpu);
      break;
    case 2:
      // check if key is currently pressed. curses keypresses are
      // effectively instantaneous, so the answer is always 'no'.
      dcpu->reg[REG_C] = 0;
      break;
    case 3:
      term.kbdints = dcpu->reg[REG_B];
      break;
    default:
      dcpu_msg("warning: unknown keyboard HWI: 0x%04x\n", dcpu->reg[REG_A]);
  }
  return 0; // no extra cycles
}

static void kbd_tick(dcpu *dcpu, tstamp_t now) {
  if (now > term.nextkey) {
    checkkey(dcpu);
    term.nextkey += term.keyns;
  }
}

static u16 lem_hwi(dcpu *dcpu) {
  switch (dcpu->reg[REG_A]) {
    case 0: // MEM_MAP_SCREEN
      term.vram = dcpu->reg[REG_B];
      break;
    case 1: // MEM_MAP_FONT
      dcpu_msg("warning: MEM_MAP_FONT unsupported on text-only terminal.\n");
      break;
    case 2: // MEM_MAP_PALETTE
      // TODO should be able to approx. this on on changeable terminals
      dcpu_msg("warning: MEM_MAP_PALETTE unsupported on text-only terminal.\n");
      break;
    case 3: // SET_BORDER_COLOR
      term.nextborder = dcpu->reg[REG_B] & 0xf;
      break;
  }
  return 0; // no extra cycles
}

static void lem_tick(dcpu *dcpu, tstamp_t now) {
  if (now > term.nexttick) {
    dcpu_redraw(dcpu);
    term.nexttick += term.tickns;
  }
}

void dcpu_initterm(dcpu *dcpu) {
  term.tickns = 1000000000 / DISPLAY_HZ;
  term.nexttick = dcpu_now();
  term.keyns = 1000000000 / KBD_BAUD;
  term.nextkey = dcpu_now();
  term.curborder = 0;
  term.nextborder = 0;
  term.vram = 0;
  term.keybufwrite = 0;
  term.keybufread = 0;
  term.kbdints = 0;

  // set up hardware descriptors
  device *kbd = dcpu_addhw(dcpu);
  kbd->id = 0x30cf7406;
  kbd->version = 1;
  kbd->mfr = 0x01220423;
  kbd->hwi = &kbd_hwi;
  kbd->tick = &kbd_tick;
  device *lem = dcpu_addhw(dcpu);
  lem->id = 0x7349f615;
  lem->version = 0x1802;
  lem->mfr = 0x1c6c8b36;
  lem->hwi = &lem_hwi;
  lem->tick = &lem_tick;

  // set up curses...
  initscr();
  start_color();
  cbreak();
  keypad(stdscr, true);
  term.border = subwin(stdscr, 14, 36, 0, 0);
  term.vidwin = subwin(stdscr, 12, 32, 1, 2);
  term.dbgwin = subwin(stdscr, LINES - (SCR_HEIGHT+3), COLS, SCR_HEIGHT+2, 0);
  keypad(term.vidwin, true);
  keypad(term.border, true);
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
  dcpu_msg("terminal colors: %d, pairs %d, %s change colors: \n", COLORS,
      COLOR_PAIRS, can_change_color() ? "*can*" : "*cannot*");
}

u16 dcpu_killterm(void) {
  endwin();
  return term.vram;
}

void dcpu_exitmsg(char *fmt, ...) {
  dcpu_killterm();
  va_list args;
  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
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
  wmove(term.vidwin, row, col);

  char letter = word & 0x7f;
  bool blink = word & 0x80;
  char fg = word >> 12;
  char bg = (word >> 8) & 0xf;

  if (!letter) letter = ' ';

  // use color_set() rather than COLOR_PAIR() since we may have more than 256
  // colors. unfortunately that doesn't really solve the problem, since every
  // linux that i can find still doesn't ship ncurses 6. how sad.
  wcolor_set(term.vidwin, color(fg, bg), NULL);
  if (blink) wattron(term.vidwin, A_BLINK);
  waddch(term.vidwin, letter);
  if (blink) wattroff(term.vidwin, A_BLINK);
}

static void draw_border() {
  if (term.curborder != term.nextborder) {
    term.curborder = term.nextborder;
    wbkgd(term.border, A_NORMAL | COLOR_PAIR(color(0, term.curborder)) | ' '); 
    wrefresh(term.border);
  }
}

void dcpu_redraw(dcpu *dcpu) {
  draw_border();
  if (term.vram) {
    // don't perform the indirection outside the loop. vid ram can be mapped
    // toward the high end of the range, in which case we need to be careful
    // that it wraps around to the start.
    u16 addr = term.vram;
    for (u16 i = 0; i < SCR_HEIGHT; i++) 
      for (u16 j = 0; j < SCR_WIDTH; j++)
        draw(dcpu->ram[addr++], i, j);
  }
  wrefresh(term.vidwin);
}
