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

#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

/* SDL on Mac OS wants to replace main, and must use funky macros
   to do it. we don't need any SDL stuff here, but we do need to include
   SDL.h prior to the definition of main(). ugly. */
#ifdef USE_SDL
#include <SDL.h>
#endif

#include "dcpu.h"
#include "opcodes.h"

volatile bool dcpu_break = false;
volatile bool dcpu_die = false;
static struct termios old_termios;

static void usage(char **argv) {
  fprintf(stderr, "usage: %s [options] <image>\n", argv[0]);
  fprintf(stderr, "   -h, --help           display this message\n");
  fprintf(stderr, "   -v, --version        display the version and exit\n");
  fprintf(stderr, "   -g, --graphics       enable graphical display window\n");
  fprintf(stderr, "   -k, --khz=k          set emulator clock rate (in kHz)\n");
  fprintf(stderr, "   -e, --little-endian  image file is little-endian\n");
  fprintf(stderr, "   -d, --debug-boot     enter debugger on boot\n");
  fprintf(stderr, "   -l, --detect-loops   "
      "enter debugger on single-instruction loop\n");
  fprintf(stderr, "   -s, --dump-screen    "
      "dump (ascii) contents of video ram to stdout on exit\n");
  fprintf(stderr, "\n");
  fprintf(stderr,
      "the maximum achievable clock rate depends on the host cpu as well\n");
  fprintf(stderr,
      "as the program being run. the default is 150kHz.\n");
  fprintf(stderr, "\n");
  fprintf(stderr,
      "the -e option controls the endianness of the input image only. core\n");
  fprintf(stderr, "dump files are *always* big-endian.\n");
} 

static void int_handler(int signum) {
  (void)signum;
  dcpu_break = true;
}

static void quit_handler(int signum) {
  (void)signum;
  dcpu_die = true;
}

static void block_signals() {
  struct sigaction sa;
  sa.sa_handler = int_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGINT, &sa, NULL)) {
    fprintf(stderr, "error setting signal handler: %s\n", strerror(errno));
    fprintf(stderr, "continuing without signal support...");
  }

  sa.sa_handler = quit_handler;
  if (sigaction(SIGQUIT, &sa, NULL)) {
    fprintf(stderr, "error setting signal handler: %s\n", strerror(errno));
    fprintf(stderr, "continuing without signal support...");
  }

  struct termios new_termios;
  tcgetattr(0, &old_termios);
  new_termios = old_termios;
  new_termios.c_cc[VQUIT] = 0x04; // ctrl-d
  tcsetattr(0, TCSANOW, &new_termios);
}

int main(int argc, char **argv) {
  uint32_t khz = DEFAULT_KHZ;
  bool bigend = true;
  bool debug = false;
  bool dump_screen = false;
  bool graphics = false;
  dcpu dcpu;
  dcpu.detect_loops = false;

  for (;;) {
    int c;

    static struct option long_options[] = {
      {"help", 0, 0, 'h'},
      {"version", 0, 0, 'v'},
      {"graphics", 0, 0, 'g'},
      {"khz", 1, 0, 'k'},
      {"debug-boot", 0, 0, 'd'},
      {"little-endian", 0, 0, 'e'},
      {"detect-loops", 0, 0, 'l'},
      {"dump-screen", 0, 0, 's'},
      {0, 0, 0, 0},
    };

    c = getopt_long(argc, argv, "hvgk:dels", long_options, NULL);

    if (c == -1) break;

    switch (c) {
      case 'h':
        usage(argv);
        return 0;
      case 'v':
        puts("dcpu-16, version " DCPU_VERSION);
        return 0;
      case 'g':
#ifdef USE_SDL
        graphics = true;
        break;
#else
        fprintf(stderr, "graphics not supported in this build!\n");
        fprintf(stderr, "  (perhaps try installing SDL and rebuilding?)\n");
        return 1;
#endif
      case 'k': {
        char *endptr;
        khz = strtoul(optarg, &endptr, 10);
        if (*endptr) {
          fprintf(stderr, "--khz requires an unsigned integer argument\n");
          return 1;
        }
        break;
      }
      case 'd':
        debug = true;
        break;
      case 'e':
        bigend = false;
        break;
      case 'l':
        dcpu.detect_loops = true;
        break;
      case 's':
        dump_screen = true;
        break;
      default:
        usage(argv);
        return 1;
    }
  }

  if (argc - optind != 1) {
    usage(argv);
    return 1;
  }
  
  const char *image = argv[optind];

  // init term first so that image load status is visible...
  block_signals();
  dcpu_initops();
  dcpu_init(&dcpu, khz);
  dcpu_initterm(&dcpu, !graphics);
  dcpu_initclock(&dcpu);
  if (graphics) dcpu_initlem(&dcpu);
  if (!dcpu_loadcore(&dcpu, image, bigend)) {
    tcsetattr(0, TCSANOW, &old_termios);
    return -1;
  }

  dcpu_msg("welcome to dcpu-16, version " DCPU_VERSION "\n");
  dcpu_msg("clock rate: %dkHz\n", khz);
  dcpu_msg("mods: " DCPU_MODS "\n");

  dcpu_msg("press ctrl-c or send SIGINT for debugger, ctrl-d to exit.\n");
  dcpu_run(&dcpu, debug);

  if (graphics) dcpu_killlem();
  u16 vram = dcpu_killterm();
  puts(" * dcpu-16 halted.");

  if (dump_screen) {
    if (vram) {
      puts(" * final screen buffer contents:");
      for (u16 i = 0; i < SCR_HEIGHT; i++) {
        printf("\n   ");
        for (u16 j = 0; j < SCR_WIDTH; j++) {
          char ch = dcpu.ram[vram++] & 0x7f;
          if (isprint(ch)) putchar(ch);
          else putchar(' ');
        }
      }
    } else {
      puts(" * final screen buffer contents: unmapped");
    }
    printf("\n\n");
  }

  tcsetattr(0, TCSANOW, &old_termios);
  return 0;
}
