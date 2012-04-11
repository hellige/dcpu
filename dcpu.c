// TODO cycle counting
// TODO debugger

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define DCPU_VERSION "1.1-mh"
#define DCPU_MODS    "+out +kbd +img +die"

#define RAM_WORDS 0x10000
#define NREGS     8 // A, B, C, X, Y, Z, I, J

#define OP_MASK   0x7
#define OP_SIZE   4
#define ARG_MASK  0x3f
#define ARG_SIZE  6

typedef struct dcpu_t {
  uint16_t sp;
  uint16_t pc;
  uint16_t o;
  uint16_t reg[NREGS];
  uint16_t ram[RAM_WORDS];
} dcpu;

static struct termios old_tio;

static int run(dcpu *dcpu);
static int init(dcpu *dcpu, const char *image);

int main(int argc, char **argv) {
  if (argc < 2) {
    printf("usage: dcpu <image>\n");
    return 1;
  }

  // set stdin unbuffered for immediate keyboard input
  struct termios new_tio;
  tcgetattr(STDIN_FILENO, &old_tio);
  new_tio = old_tio;
  new_tio.c_lflag &= ~ICANON;
  new_tio.c_lflag &= ~ECHO;
  new_tio.c_cc[VTIME] = 0;
  new_tio.c_cc[VMIN] = 0;
  tcsetattr(STDIN_FILENO, TCSANOW, &new_tio);

  puts("\nwelcome to dcpu-16, version " DCPU_VERSION ".");
  puts("mods: " DCPU_MODS ".");

  dcpu dcpu;
  int result = init(&dcpu, argv[1]);
  if (result) return result;

  puts("booting...\n");
  fflush(stdout);
  result = run(&dcpu);

  // restore the former settings
  tcsetattr(STDIN_FILENO, TCSANOW, &old_tio);

  return result;
}

int init(dcpu *dcpu, const char *image) {
  dcpu->sp = 0;
  dcpu->pc = 0;
  dcpu->o = 0;
  for (int i = 0; i < NREGS; i++) dcpu->reg[i] = 0;
  for (int i = 0; i < RAM_WORDS; i++) dcpu->ram[i] = 0;

  FILE *img = fopen(image, "r");
  if (!img) {
    fprintf(stderr, "error reading image '%s': %s\n", image, strerror(errno));
    return -1;
  }

  int img_size = fread(dcpu->ram, 2, RAM_WORDS, img);
  if (ferror(img)) {
    fprintf(stderr, "error reading image '%s': %s\n", image, strerror(errno));
    return -1;
  }

  printf("loaded image from %s: 0x%05x words\n", image, img_size);

  // swap byte order...
  for (int i = 0; i < RAM_WORDS; i++)
    dcpu->ram[i] = (dcpu->ram[i] >> 8) | ((dcpu->ram[i] & 0xff) << 8);

  fclose(img);
  return 0;
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

static inline uint16_t next(dcpu *dcpu) {
  return dcpu->ram[dcpu->pc++];
}

static inline uint8_t opcode(uint16_t instr) {
  return instr & 0xf;
}

static inline uint8_t arg_a(uint16_t instr) {
  return (instr >> OP_SIZE) & ARG_MASK;
}

static inline uint8_t arg_b(uint16_t instr) {
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

static uint16_t decode_arg(dcpu *dcpu, uint8_t arg, uint16_t **addr,
    bool effects) {
  // in case caller doesn't need addr...
  uint16_t *tmp;
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

static inline void set(uint16_t *dest, uint16_t val) {
  if (dest) *dest = val;
  // otherwise, attempt to write a literal: a silent fault.
}

// decode (but do not execute) next instruction...
static inline void skip(dcpu *dcpu) {
  uint16_t instr = next(dcpu);
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

static void exec_basic(dcpu *dcpu, uint16_t instr) {
  uint16_t *dest;
  uint16_t a = decode_arg(dcpu, arg_a(instr), &dest, true);
  uint16_t b = decode_arg(dcpu, arg_b(instr), NULL, true);

  switch (opcode(instr)) {

    case OP_SET:
      set(dest, b);
      break;

    case OP_ADD: {
      uint16_t sum = a + b;
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
#define OP_NB_DIE 0x05 // exit emulator, exit code in a

static int exec_nonbasic(dcpu *dcpu, uint16_t instr) {
  uint8_t opcode = arg_a(instr);
  uint16_t *dest;
  uint16_t a = decode_arg(dcpu, arg_b(instr), &dest, true);

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
      if (c == 0x04) exit(0);
      set(dest, c);
      break;
    }

    case OP_NB_IMG:
      coredump(dcpu, a);
      break;

    case OP_NB_DIE:
      puts("\ndcpu-16 halted.");
      exit(a); // TODO a bit abrupt...

    default:
      fprintf(stderr, "reserved instruction. abort.\n");
      return -1;
  }

  return 0;
}

static int run(dcpu *dcpu) {
  for (;;) {
    uint16_t instr = next(dcpu);
    int result = 0;
    if (opcode(instr) == OP_NON)
      result = exec_nonbasic(dcpu, instr);
    else
      exec_basic(dcpu, instr);
    if (result) return result;
  }
}
