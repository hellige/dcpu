all: dcpu forth.img

dcpu: dcpu.c
	gcc -ggdb3 -std=gnu99 -Wall -O0 -o dcpu dcpu.c

forth.img: forth.asm masm
	m4 forth.asm > forth.s
	./masm forth.s $@

clean:
	-rm -f dcpu
	-rm -f forth.s forth.img

.PHONY: clean all
