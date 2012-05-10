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
void dcpu_initlem(dcpu *dcpu) { (void)dcpu; }
u16 dcpu_killlem(void) { return 0; }

#else /* USE_SDL */

// the real SDL implementation follows...

#include <SDL.h>

#include "font.xbm"

#define WIN_WIDTH   (SCR_WIDTH * 4 + 2 * SCR_BORDER)
#define WIN_HEIGHT  (SCR_HEIGHT * 8 + 2 * SCR_BORDER)

struct tile_t {
  uint32_t glyph;
  uint32_t fg;
  uint32_t bg;
};

struct screen_t {
  tstamp_t tickns;
  tstamp_t nexttick;
  tstamp_t blinkns;
  tstamp_t nextblink;
  bool curblink;
  struct tile_t contents[SCR_HEIGHT * SCR_WIDTH];
  uint32_t curborder;
  u16 nextborder;
  SDL_Surface *scr;
  u16 vram;
  u16 palram;
  u16 fontram;
  bool dirty;
};

static struct screen_t screen;

// default palette
static const u16 palette[] = {
    0x0000, 0x000a, 0x00a0, 0x00aa,
    0x0a00, 0x0a0a, 0x0a50, 0x0aaa,
    0x0555, 0x055f, 0x05f5, 0x05ff,
    0x0f55, 0x0f5f, 0x0ff5, 0x0fff};

// default font
static u16 font[256];

static u16 lem_hwi(dcpu *dcpu) {
  switch (dcpu->reg[REG_A]) {
    case 0: // MEM_MAP_SCREEN
      screen.vram = dcpu->reg[REG_B];
      break;
    case 1: // MEM_MAP_FONT
      screen.fontram = dcpu->reg[REG_B];
      break;
    case 2: // MEM_MAP_PALETTE
      screen.palram = dcpu->reg[REG_B];
      break;
    case 3: // SET_BORDER_COLOR
      screen.nextborder = dcpu->reg[REG_B] & 0xf;
      break;
    case 4: { // MEM_DUMP_FONT
      u16 addr = dcpu->reg[REG_B];
      for (int i = 0; i < 256; i++)
        dcpu->ram[addr++] = font[i];
      return 256; // halts for 256 extra cycles
    }
    case 5: { // MEM_DUMP_PALETTE
      u16 addr = dcpu->reg[REG_B];
      for (int i = 0; i < 16; i++)
        dcpu->ram[addr++] = palette[i];
      return 16; // halts for 16 extra cycles
    }
  }
  return 0; // no extra cycles
}

static void lem_redraw(dcpu *dcpu, tstamp_t now); // TODO delete
static void lem_tick(dcpu *dcpu, tstamp_t now) {
  if (now > screen.nexttick) {
    // we need to drain the event queue on os x, even if we don't care about
    // events. otherwise, our graphics window gets the fearsome beachball.
    SDL_Event event;
    while (SDL_PollEvent(&event));
    lem_redraw(dcpu, now); // TODO how to hook into dcpu_redraw?
    screen.nexttick += screen.tickns;
  }
}

uint8_t font_pixel(int n) {
  return ((font_bits[n/8] >> n%8) & 1) ^ 1;
}

void dcpu_initlem(dcpu *dcpu) {
  screen.tickns = 1000000000 / DISPLAY_HZ;
  screen.nexttick = dcpu_now();
  screen.blinkns = 1000000000 / BLINK_HZ;
  screen.nextblink = dcpu_now();
  screen.curblink = false;
  screen.curborder = 0;
  screen.nextborder = 0;
  screen.vram = 0;
  screen.palram = 0;
  screen.fontram = 0;

  for (int i = 0; i < SCR_HEIGHT * SCR_WIDTH; i++)
    screen.contents[i] = (struct tile_t) {0, 0, 0};

  // initialize font from xbm
  int w = font_width;
  for (int j = 0; j < font_height; j += 8) {
    for (int i = 0; i < w; i += 2) {
      int idx = j*w + i;
      u16 ch = 0;
      for (int k = 0; k < 8; k++) {
        ch |= font_pixel(idx   + k*w) << (k+8);
        ch |= font_pixel(idx+1 + k*w) << k;
      }
      font[i/2 + j*w/16] = ch;
    }
  }

  // set up hardware descriptors
  device *lem = dcpu_addhw(dcpu);
  lem->id = 0x7349f615;
  lem->version = 0x1802;
  lem->mfr = 0x1c6c8b36;
  lem->hwi = &lem_hwi;
  lem->tick = &lem_tick;

  // set up the window
  if (SDL_Init(SDL_INIT_VIDEO) < 0) {
    dcpu_exitmsg("unable to init sdl: %s\n", SDL_GetError());
    exit(1);
  }

  screen.scr = SDL_SetVideoMode(WIN_WIDTH * SCR_SCALE, WIN_HEIGHT * SCR_SCALE,
      32 /* bits-per-pixel*/,
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

static uint32_t color(dcpu *dcpu, char idx) {
  // careful. palram can be near the high end, in which case we need to be sure
  // to handle wrapping correctly...
  // this is always fine, since screen.palram is 0 when unmapped:
  u16 ramidx = screen.palram + idx;
  u16 col = (screen.palram ? dcpu->ram : palette)[ramidx];

  // convert to rrggbb by duplicating each nibble...
  u16 r = (col & 0x0f00) >> 8; r |= r << 4;
  u16 g = (col & 0x00f0) >> 4; g |= g << 4;
  u16 b = (col & 0x000f);      b |= b << 4;
  return SDL_MapRGB(screen.scr->format, r, g, b);
}

static void draw(dcpu *dcpu, u16 addr, u16 row, u16 col) {
  u16 word = dcpu->ram[screen.vram+addr];

  int left = col * 4 + SCR_BORDER;
  int top = row * 8 + SCR_BORDER;
  
  char charidx = word & 0x7f;
  bool blink = word & 0x80;
  char fgidx = word >> 12;
  char bgidx = (word >> 8) & 0xf;
  uint32_t fg = color(dcpu, fgidx);
  uint32_t bg = color(dcpu, bgidx);

  // default to blank in case of blinked-out glyph...
  uint32_t glyph = 0;
  if (!blink || screen.curblink) {
    // be very careful here. fontram can be near the high end, in which case we
    // need to be sure to handle wrapping correctly, even if we wrap in the
    // middle of a single glyph...
    // this is always fine, since screen.fontram is 0 when unmapped:
    u16 lglyphidx = screen.fontram + (charidx*2);
    u16 rglyphidx = lglyphidx+1;
    u16 lglyph = (screen.fontram ? dcpu->ram : font)[lglyphidx];
    u16 rglyph = (screen.fontram ? dcpu->ram : font)[rglyphidx];
    glyph = (lglyph << 16) | rglyph;
  }

  // check cache...
  struct tile_t curtile = { glyph, fg, bg };
  struct tile_t *cached = &screen.contents[addr];
  if (curtile.glyph == cached->glyph
      && curtile.fg == cached->fg
      && curtile.bg == cached->bg)
    return;
  *cached = curtile;
  screen.dirty = true;

  for (int x = 0; x < 4; x++) {
    for (int y = 7; y >= 0; y--) {
      SDL_Rect pix;
      pix.x = (left+x) * SCR_SCALE;
      pix.y = (top+y) * SCR_SCALE;
      pix.w = pix.h = SCR_SCALE;
      SDL_FillRect(screen.scr, &pix,
        glyph & (1<<31) && (!blink || screen.curblink) ? fg : bg);
      glyph <<= 1;
    }
  }
}

static void draw_border(dcpu *dcpu) {
  uint32_t col = color(dcpu, screen.nextborder);
  if (screen.curborder != col) {
    screen.curborder = col;
    screen.dirty = true;

    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = WIN_WIDTH * SCR_SCALE;
    rect.h = SCR_BORDER * SCR_SCALE;
    SDL_FillRect(screen.scr, &rect, col);
    rect.y = (WIN_HEIGHT - SCR_BORDER) * SCR_SCALE;
    SDL_FillRect(screen.scr, &rect, col);
    rect.y = SCR_BORDER * SCR_SCALE;
    rect.w = SCR_BORDER * SCR_SCALE;
    rect.h = (WIN_HEIGHT - 2*SCR_BORDER) * SCR_SCALE;
    SDL_FillRect(screen.scr, &rect, col);
    rect.x = (WIN_WIDTH - SCR_BORDER) * SCR_SCALE;
    SDL_FillRect(screen.scr, &rect, col);
  }
}

void lem_redraw(dcpu *dcpu, tstamp_t now) {
  screen.dirty = false;

  if (now > screen.nextblink) {
    screen.curblink = !screen.curblink;
    screen.dirty = true;
    screen.nextblink += screen.blinkns;
  }

  if (SDL_MUSTLOCK(screen.scr)) SDL_LockSurface(screen.scr);
  draw_border(dcpu);
  if (screen.vram) {
    // don't perform the indirection outside the loop. vid ram can be mapped
    // toward the high end of the range, in which case we need to be careful
    // that it wraps around to the start.
    u16 vaddr = 0;
    for (u16 i = 0; i < SCR_HEIGHT; i++) 
      for (u16 j = 0; j < SCR_WIDTH; j++)
        draw(dcpu, vaddr++, i, j);
  }
  if (screen.dirty) SDL_Flip(screen.scr);
  if (SDL_MUSTLOCK(screen.scr)) SDL_UnlockSurface(screen.scr);
}

#endif /* USE_SDL */
