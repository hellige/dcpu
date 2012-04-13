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
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "dcpu.h"


struct term_t {
  struct termios old_tio;
  struct termios run_tio;
};


void dcpu_initterm(dcpu *dcpu) {
  term *t = malloc(sizeof(term)); // TODO check return
  dcpu->term = t;

  // set stdin unbuffered for immediate keyboard input
  tcgetattr(STDIN_FILENO, &t->old_tio);
  t->run_tio = t->old_tio;
  t->run_tio.c_lflag &= ~ICANON;
  t->run_tio.c_lflag &= ~ECHO;
  t->run_tio.c_cc[VTIME] = 0;
  t->run_tio.c_cc[VMIN] = 1;
}

void dcpu_killterm(dcpu *dcpu) {
  free(dcpu->term); // TODO check return
}

volatile bool dcpu_break = false;

static void handler(int signum) {
  (void)signum;
  dcpu_break = true;
}

static void block_sigint(bool block) {
  struct sigaction sa;
  sa.sa_handler = block ? handler : SIG_DFL;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  if (sigaction(SIGINT, &sa, NULL)) {
    fprintf(stderr, "error setting signal handler: %s\n", strerror(errno));
    fprintf(stderr, "continuing without signal support...");
  }
}

void dcpu_runterm(dcpu *dcpu) {
  block_sigint(true);
  fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) | O_NONBLOCK);
  tcsetattr(STDIN_FILENO, TCSANOW, &dcpu->term->run_tio);
}

void dcpu_dbgterm(dcpu *dcpu) {
  block_sigint(false);
  fcntl(STDIN_FILENO, F_SETFL, fcntl(STDIN_FILENO, F_GETFL, 0) & ~O_NONBLOCK);
  tcsetattr(STDIN_FILENO, TCSANOW, &dcpu->term->old_tio);
}

