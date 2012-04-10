all: dcpu forth.img

dcpu: dcpu.c
	gcc -ggdb3 -std=gnu99 -Wall -O0 -o dcpu dcpu.c

forth.img: forth.dasm masm
	m4 $< > forth.s
	./masm forth.s $@

run: forth.ft all
	cat $< - | ./dcpu forth.img
clean:
	-rm -f dcpu
	-rm -f forth.s forth.img

.PHONY: clean all run
