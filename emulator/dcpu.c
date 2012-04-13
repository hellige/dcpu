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
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dcpu.h"
#include "opcodes.h"

static void usage(char **argv) {
  fprintf(stderr, "usage: %s [options] <image>\n", argv[0]);
  fprintf(stderr, "   -h, --help      show this message\n");
  fprintf(stderr, "   -v, --version   show the version\n");
  fprintf(stderr, "   -k, --khz=k     set emulator clock rate (in kHz)\n");
  fprintf(stderr,
      "the maximum achievable clock rate depends on the host cpu as well\n");
  fprintf(stderr,
      "as the program being run. the default is 150kHz.\n");
} 

int main(int argc, char **argv) {
  uint32_t khz = DEFAULT_KHZ;

  for (;;) {
    int c;

    static struct option long_options[] = {
      {"help", 0, 0, 'h'},
      {"version", 0, 0, 'v'},
      {"khz", 1, 0, 'k'},
      {0, 0, 0, 0},
    };

    c = getopt_long(argc, argv, "hvk:", long_options, NULL);

    if (c == -1) break;

    switch (c) {
      case 'h':
        usage(argv);
        return 0;
      case 'v':
        puts("dcpu-16, version " DCPU_VERSION);
        return 0;
      case 'k': {
        char *endptr;
        khz = strtoul(optarg, &endptr, 10);
        if (*endptr) {
          fprintf(stderr, "--khz requires an unsigned integer argument\n");
          return 1;
        }
        break;
      }
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

  puts("\nwelcome to dcpu-16, version " DCPU_VERSION);
  printf("clock rate: %dkHz\n", khz);
  puts("mods: " DCPU_MODS);

  dcpu dcpu;
  if (!dcpu_init(&dcpu, image, khz)) return -1;
  dcpu_initterm(&dcpu);

  puts("press ctrl-c or send SIGINT for debugger, ctrl-d to exit.");
  puts("booting...\n");
  fflush(stdout);
  dcpu_run(&dcpu);

  dcpu_killterm(&dcpu);
  puts("\ndcpu-16 halted.\n");

  return 0;
}
