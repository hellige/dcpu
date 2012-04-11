all: dcpu forth.img

dcpu: dcpu.c
	gcc -ggdb3 -std=gnu99 -Wall -O0 -o dcpu dcpu.c

boot.img: forth.dasm masm
	m4 $< > forth.s
	./masm forth.s $@

forth.img: forth.ft boot.img dcpu
	cat forth.ft | ./dcpu boot.img > /dev/null
	mv core.img $@

clean:
	-rm -f dcpu
	-rm -f boot.img
	-rm -f forth.s forth.img

.PHONY: clean all run
