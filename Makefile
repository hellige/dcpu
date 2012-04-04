emulator/dcpu: emulator/ecpu.c
	gcc -ggdb3 -std=gnu99 -Wall -O0 -o emulator/dcpu emulator/dcpu.c

clean:
	-rm -f emulator/dcpu

.PHONY: clean
