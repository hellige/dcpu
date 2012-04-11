// TODO cycle counting

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

typedef uint16_t u16;

#define DCPU_VERSION "1.1-mh"
#define DCPU_MODS    "+out +kbd +img +die"

#define RAM_WORDS 0x10000
#define NREGS     8 // A, B, C, X, Y, Z, I, J

#define OP_MASK   0x7
#define OP_SIZE   4
#define ARG_MASK  0x3f
#define ARG_SIZE  6

extern u16 *disassemble(u16 *pc, char *out);

typedef struct dcpu_t {
  u16 sp;
  u16 pc;
  u16 o;
  u16 reg[NREGS];
  u16 ram[RAM_WORDS];
  struct termios old_tio;
  struct termios run_tio;
} dcpu;

static void run(dcpu *dcpu);
static bool init(dcpu *dcpu, const char *image);

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("usage: dcpu <image>\n");
    return 1;
  }

  puts("\nwelcome to dcpu-16, version " DCPU_VERSION);
  puts("mods: " DCPU_MODS);

  dcpu dcpu;
  if (!init(&dcpu, argv[1])) return -1;

  puts("press ctrl-c or send SIGINT for debugger, ctrl-d to exit.");
  puts("booting...\n");
  fflush(stdout);
  run(&dcpu);

  puts("\ndcpu-16 halted.\n");

  return 0;
}

void termset(struct termios *tio) {
  tcsetattr(STDIN_FILENO, TCSANOW, tio);
}

bool init(dcpu *dcpu, const char *image) {
  // set stdin unbuffered for immediate keyboard input
  tcgetattr(STDIN_FILENO, &dcpu->old_tio);
  dcpu->run_tio = dcpu->old_tio;
  dcpu->run_tio.c_lflag &= ~ICANON;
  dcpu->run_tio.c_lflag &= ~ECHO;
  dcpu->run_tio.c_cc[VTIME] = 0;
  dcpu->run_tio.c_cc[VMIN] = 0;

  dcpu->sp = 0;
  dcpu->pc = 0;
  dcpu->o = 0;
  for (int i = 0; i < NREGS; i++) dcpu->reg[i] = 0;
  for (int i = 0; i < RAM_WORDS; i++) dcpu->ram[i] = 0;

  FILE *img = fopen(image, "r");
  if (!img) {
    fprintf(stderr, "error reading image '%s': %s\n", image, strerror(errno));
    return false;
  }

  int img_size = fread(dcpu->ram, 2, RAM_WORDS, img);
  if (ferror(img)) {
    fprintf(stderr, "error reading image '%s': %s\n", image, strerror(errno));
    return false;
  }

  printf("loaded image from %s: 0x%05x words\n", image, img_size);

  // swap byte order...
  for (int i = 0; i < RAM_WORDS; i++)
    dcpu->ram[i] = (dcpu->ram[i] >> 8) | ((dcpu->ram[i] & 0xff) << 8);

  fclose(img);
  return true;
}

// note limit is 32-bit, so that we can distinguish 0 from RAM_WORDS
void coredump(dcpu *dcpu, uint32_t limit) {
  char *image = "core.img";
  FILE *img = fopen(image, "w");
  if (!img) {
    fprintf(stderr, "error opening image '%s': %s\n", image, strerror(errno));
    return;
  }

  if (limit == 0) limit = RAM_WORDS;

  for (uint32_t i = 0; i < limit; i++) {
    if (fputc(dcpu->ram[i] >> 8, img) == EOF
        || fputc(dcpu->ram[i] & 0xff, img) == EOF) {
      fprintf(stderr, "error writing to image '%s': %s\n", image, strerror(errno));
      return;
    }
  }

  fclose(img);
}

static inline u16 next(dcpu *dcpu) {
  return dcpu->ram[dcpu->pc++];
}

static inline uint8_t opcode(u16 instr) {
  return instr & 0xf;
}

static inline uint8_t arg_a(u16 instr) {
  return (instr >> OP_SIZE) & ARG_MASK;
}

static inline uint8_t arg_b(u16 instr) {
  return (instr >> (OP_SIZE + ARG_SIZE)) & ARG_MASK;
}

#define ARG_POP  0x18
#define ARG_PEEK 0x19
#define ARG_PUSH 0x1a
#define ARG_SP   0x1b
#define ARG_PC   0x1c
#define ARG_O    0x1d
#define ARG_NXA  0x1e  // next word, deref
#define ARG_NXL  0x1f  // next word, literal

static u16 decode_arg(dcpu *dcpu, uint8_t arg, u16 **addr, bool effects) {
  // in case caller doesn't need addr...
  u16 *tmp;
  if (addr == NULL) addr = &tmp;

  // initialize to null, the right result for a literal
  *addr = NULL;

  // literal. no address.
  if (arg & 0x20) return arg - 0x20;

  // special operator
  if (arg & 0x18) {
    switch (arg) {
      case ARG_POP:
        *addr = &dcpu->ram[dcpu->sp];
        if (effects) dcpu->sp++;
        return **addr;
      case ARG_PEEK:
        *addr = &dcpu->ram[dcpu->sp];
        return **addr;
      case ARG_PUSH:
        if (effects) dcpu->sp--;
        *addr = &dcpu->ram[dcpu->sp];
        return **addr;
      case ARG_SP:
        *addr = &dcpu->sp;
        return **addr;
      case ARG_PC:
        *addr = &dcpu->pc;
        return **addr;
      case ARG_O:
        *addr = &dcpu->o;
        return **addr;
      case ARG_NXA:
        *addr = &dcpu->ram[next(dcpu)];
        return **addr;
      case ARG_NXL:
        // literal. no address (by fiat in this case) TODO is that right?
        return next(dcpu);
    }
  }

  // register or register[+offset] deref
  uint8_t reg = arg & 0x7;
  if (arg & 0x10)
    *addr = &dcpu->ram[dcpu->reg[reg] + next(dcpu)]; // TODO: what if reg is PC?
  else if (arg & 0x8)
    *addr = &dcpu->ram[dcpu->reg[reg]];
  else
    *addr = &dcpu->reg[reg];
  return **addr;
}

static inline void set(u16 *dest, u16 val) {
  if (dest) *dest = val;
  // otherwise, attempt to write a literal: a silent fault.
}

// decode (but do not execute) next instruction...
static inline void skip(dcpu *dcpu) {
  u16 instr = next(dcpu);
  decode_arg(dcpu, arg_a(instr), NULL, false);
  decode_arg(dcpu, arg_b(instr), NULL, false);
}

#define OP_NON 0x0
#define OP_SET 0x1
#define OP_ADD 0x2
#define OP_SUB 0x3
#define OP_MUL 0x4
#define OP_DIV 0x5
#define OP_MOD 0x6
#define OP_SHL 0x7
#define OP_SHR 0x8
#define OP_AND 0x9
#define OP_BOR 0xa
#define OP_XOR 0xb
#define OP_IFE 0xc
#define OP_IFN 0xd
#define OP_IFG 0xe
#define OP_IFB 0xf

static void exec_basic(dcpu *dcpu, u16 instr) {
  u16 *dest;
  u16 a = decode_arg(dcpu, arg_a(instr), &dest, true);
  u16 b = decode_arg(dcpu, arg_b(instr), NULL, true);

  switch (opcode(instr)) {

    case OP_SET:
      set(dest, b);
      break;

    case OP_ADD: {
      u16 sum = a + b;
      set(dest, sum);
      dcpu->o = sum < a ? 0x1 : 0;
      break;
    }

    case OP_SUB:
      set(dest, a - b);
      dcpu->o = a < b ? 0xffff : 0;
      break;

    case OP_MUL:
      set(dest, a * b);
      dcpu->o = ((a * b) >> 16) & 0xffff; // per spec
      break;

    case OP_DIV:
      if (b == 0) {
        set(dest, 0);
        dcpu->o = 0;
      } else {
        set(dest, a / b);
        dcpu->o = ((a << 16) / b) & 0xffff; // per spec
      }
      break;

    case OP_MOD:
      if (b == 0)
        set(dest, 0);
      else
        set(dest, a % b);
      break;

    case OP_SHL:
      set(dest, a << b);
      dcpu->o = ((a << b) >> 16) & 0xffff; // per spec
      break;

    case OP_SHR:
      set(dest, a >> b);
      dcpu->o = ((a << 16) >> b) & 0xffff; // per spec
      break;

    case OP_AND:
      set(dest, a & b);
      break;

    case OP_BOR:
      set(dest, a | b);
      break;

    case OP_XOR:
      set(dest, a ^ b);
      break;

    case OP_IFE:
      if (!(a == b)) skip(dcpu);
      break;

    case OP_IFN:
      if (!(a != b)) skip(dcpu);
      break;

    case OP_IFG:
      if (!(a > b)) skip(dcpu);
      break;

    case OP_IFB:
      if (!(a & b)) skip(dcpu);
      break;

  }
}

#define OP_NB_RSV 0x00 // reserved
#define OP_NB_JSR 0x01

// custom ops
#define OP_NB_OUT 0x02 // ascii char to console
#define OP_NB_KBD 0x03 // ascii char from keybaord, store in a
#define OP_NB_IMG 0x04 // save core image to core.img, up to address in a
#define OP_NB_DIE 0x05 // exit emulator
#define OP_NB_DBG 0x06 // enter the emulator debugger

typedef enum {
  A_CONTINUE,
  A_BREAK,
  A_EXIT
} action_t;

static action_t exec_nonbasic(dcpu *dcpu, u16 instr) {
  uint8_t opcode = arg_a(instr);
  u16 *dest;
  u16 a = decode_arg(dcpu, arg_b(instr), &dest, true);

  switch (opcode) {
    case OP_NB_JSR:
      dcpu->ram[--dcpu->sp] = dcpu->pc;
      dcpu->pc = a;
      break;

    case OP_NB_OUT:
      putchar(a);
      fflush(stdout);
      break;

    case OP_NB_KBD: {
      // will set dest to -1 if no key is available...
      int c = getchar();
      // since we're non-canon, we need to check for ctrl-d ourselves...
      if (c == 0x04) return A_EXIT;
      // we'll also map delete to backspace to avoid goofy terminals...
      if (c == 0x7f) c = 0x08;
      set(dest, c);
      break;
    }

    case OP_NB_IMG:
      coredump(dcpu, a);
      break;

    case OP_NB_DIE:
      return A_EXIT;

    case OP_NB_DBG:
      return A_BREAK;

    default:
      fprintf(stderr, "reserved instruction: 0x%04x, pc now 0x%04x.",
        opcode, dcpu->pc);
      return A_BREAK;
  }

  return A_CONTINUE;
}

static action_t step(dcpu *dcpu) {
  u16 instr = next(dcpu);
  int result = A_CONTINUE;
  if (opcode(instr) == OP_NON)
    result = exec_nonbasic(dcpu, instr);
  else
    exec_basic(dcpu, instr);
  return result;
}

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


static bool debug(dcpu *dcpu) {
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
      termset(&dcpu->run_tio);
      step(dcpu);
      termset(&dcpu->old_tio);
      dumpstate(dcpu);
    } else if (matches(tok, "d", "dump")) {
      dumpheader();
      dumpstate(dcpu);
    } else if (matches(tok, "cor", "core")) {
      coredump(dcpu, 0);
      puts("core written to core.img");
    } else if (matches(tok, "e", "exit")
        || matches(tok, "q", "quit")) {
      return false;
    } else {
      printf("unrecognized or ambiguous command: %s\n", tok);
    }
  }
}

static bool block(int how, sigset_t *sigs) {
  if (sigprocmask(how, sigs, 0)) {
    fprintf(stderr, "error setting sigmask: %s\n", strerror(errno));
    return false;
  }
  return true;
}

static void run(dcpu *dcpu) {
  sigset_t sigs;
  sigemptyset(&sigs);
  sigaddset(&sigs, SIGINT);
  if (!block(SIG_BLOCK, &sigs)) return;

  termset(&dcpu->run_tio);

  bool running = true;
  while (running) {
    bool brk = false;
    action_t action = step(dcpu);
    if (action == A_EXIT) running = false;
    if (action == A_BREAK) brk = true;

    struct timespec ts = {0, 0};
    siginfo_t info;
    if (sigtimedwait(&sigs, &info, &ts) == -1) {
      if (errno != EAGAIN) {
        fprintf(stderr, "error checking signals: %s\n", strerror(errno));
        return;
      }
    } else {
      brk = true;
    }
    
    if (brk) {
      termset(&dcpu->old_tio);
      if (!block(SIG_UNBLOCK, &sigs)) return;
      running = debug(dcpu);
      termset(&dcpu->run_tio);
      if (!block(SIG_BLOCK, &sigs)) return;
    }
  }

  termset(&dcpu->old_tio);
}
