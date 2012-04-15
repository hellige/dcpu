CC = gcc
DEBUG = 
CFLAGS = -ggdb3 -std=gnu99 -O3 -Wall -Wextra -pedantic $(DEBUG) $(PLATCFLAGS)
LIBS = -lncurses $(PLATLIBS)

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
MAIN_S = dcpu.c debugger.c disassembler.c emulator.c opcodes.c terminal.c
MAIN_O = $(patsubst %.c,out/%.o,$(MAIN_S))

ALL_O = $(MAIN_O)
ALL_T = dcpu goforth.img


default: all

all: $(ALL_T)

dcpu: $(MAIN_O)
	@mkdir -p $(dir $@)
	$(CC) -o $@ $(PLATLDFLAGS) $^ $(LIBS)

$(MAIN_O):out/%.o: $(MAIN_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) -c -o $@ $(CFLAGS) -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" \
	    -MT"$(@:%.o=%.d)" $<

out/boot.img: goforth.dasm masm
	m4 $< > out/goforth.s
	./masm out/goforth.s $@

goforth.img: goforth.ft out/boot.img dcpu
	cat goforth.ft | ./dcpu -k 10000 out/boot.img > /dev/null
	mv core.img $@

clean:
	-rm -f $(ALL_T) $(ALL_O)
	-rm -f out/boot.img
	-rm -f out/goforth.s

spotless: clean
	-rm -f $(ALL_O:.o=.d)

-include $(ALL_O:.o=.d)

.PHONY: default all clean spotless
