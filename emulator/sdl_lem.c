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

#include <stdlib.h>

#include "dcpu.h"

#ifndef USE_SDL

// stubs for building without graphics support:
void dcpu_initlem(dcpu *dcpu) {}
u16 dcpu_killlem(void) { return 0; }

#else /* USE_SDL */

// the real SDL implementation follows...

#include <SDL.h>


struct screen_t {
  tstamp_t tickns;
  tstamp_t nexttick;
  u16 vram;
  u16 curborder;
  u16 nextborder;
  SDL_Surface *scr;
  // TODO palette backing store
  // TODO font backing store
};

static struct screen_t screen;

// TODO needed?
static inline u16 color(int fg, int bg) {
  return fg * 16 + bg + 1;
}

static u16 lem_hwi(dcpu *dcpu) {
  switch (dcpu->reg[REG_A]) {
    case 0: // MEM_MAP_SCREEN
      screen.vram = dcpu->reg[REG_B];
      break;
    case 1: // MEM_MAP_FONT
      // TODO
      break;
    case 2: // MEM_MAP_PALETTE
      // TODO
      break;
    case 3: // SET_BORDER_COLOR
      screen.nextborder = dcpu->reg[REG_B] & 0xf;
      break;
  }
  return 0; // no extra cycles
}

static void lem_tick(dcpu *dcpu, tstamp_t now) {
  if (now > screen.nexttick) {
    dcpu_redraw(dcpu); // TODO how to hook into dcpu_redraw?
    screen.nexttick += screen.tickns;
  }
}

void dcpu_initlem(dcpu *dcpu) {
  screen.tickns = 1000000000 / DISPLAY_HZ;
  screen.nexttick = dcpu_now();
  screen.curborder = 0;
  screen.nextborder = 0;
  screen.vram = 0;

  // set up hardware descriptors
  /* TODO
  device *lem = dcpu_addhw(dcpu);
  lem->id = 0x7349f615;
  lem->version = 0x1802;
  lem->mfr = 0x1c6c8b36;
  lem->hwi = &lem_hwi;
  lem->tick = &lem_tick;
  */

  // set up the window
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    dcpu_exitmsg("unable to init sdl: %s\n", SDL_GetError());
    exit(1);
  }

  screen.scr = SDL_SetVideoMode(
      SCR_WIDTH * 4 + 2 * SCR_BORDER,
      SCR_HEIGHT * 8 + 2 * SCR_BORDER,
      32, // bits-per-pixel
      SDL_SWSURFACE | SDL_DOUBLEBUF);
  if (!screen.scr) {
    dcpu_exitmsg("unable to set sdl video mode: %s\n", SDL_GetError());
    exit(1);
  }

  SDL_WM_SetCaption("DCPU-16 LEM-1802", NULL);
}

u16 dcpu_killlem(void) {
  SDL_Quit();
  return screen.vram; // TODO generalize!
}

/*
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
*/

#endif /* USE_SDL */
