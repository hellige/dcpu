CC = gcc
DEBUG = 
CFLAGS = -ggdb3 -std=gnu99 -O3 -Wall -Wextra -pedantic $(DEBUG) $(PLATCFLAGS) \
    `pkg-config --silence-errors --cflags sdl` \
    $(shell pkg-config --exists sdl && echo "-DUSE_SDL")

LIBS = -lncurses `pkg-config --silence-errors --libs sdl` $(PLATLIBS)

PLATCFLAGS = 
PLATLDFLAGS = 
PLATLIBS = 

system := $(shell uname)
ifeq ($(system),Linux)
    PLATCFLAGS = -fdiagnostics-show-option -fpic -DDCPU_LINUX
    PLATLIBS = -lrt
endif
ifeq ($(system),Darwin)
    DARWIN_ARCH = x86_64 # i386
    PLATCFLAGS = -fPIC -DDCPU_MACOSX # -arch $(DARWIN_ARCH)
    PLATLDFLAGS = # -arch $(DARWIN_ARCH)
endif

MAIN_DIR = emulator
MAIN_S = clock.c dcpu.c debugger.c disassembler.c emulator.c opcodes.c \
    sdl_lem.c terminal.c
MAIN_O = $(patsubst %.c,out/%.o,$(MAIN_S))

ALL_O = $(MAIN_O)
ALL_T = dcpu goforth.img colortest.img


default: all

all: $(ALL_T)

dcpu: $(MAIN_O)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $(PLATLDFLAGS) $^ $(LIBS)

$(MAIN_O):out/%.o: $(MAIN_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c -o $@ $(CFLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" \
	    -MT"$(@:%.o=%.d)" $<

colortest.img: colortest.dasm masm
	./masm $< $@

out/boot.img: forth/goforth.dasm masm
	m4 $< > out/goforth.s
	./masm out/goforth.s $@

goforth.img: forth/goforth.ft forth/asm.ft forth/disasm.ft out/boot.img dcpu
	cat forth/goforth.ft | ./dcpu -k 10000 out/boot.img > /dev/null
	cat forth/asm.ft | ./dcpu -k 10000 core.img > /dev/null
	cat forth/disasm.ft | ./dcpu -k 10000 core.img > /dev/null
	mv core.img $@

clean:
	-rm -f $(ALL_T) $(ALL_O)
	-rm -f out/boot.img
	-rm -f out/goforth.s

spotless: clean
	-rm -rf out

-include $(ALL_O:.o=.d)

.PHONY: default all clean spotless
