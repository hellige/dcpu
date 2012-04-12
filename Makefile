all: dcpu forth.img

%.o: %.c
	gcc -c -ggdb3 -std=gnu99 -Wall -O2 -o $@ $<

dcpu: dcpu.o disassemble.o
	gcc -ggdb3 -std=gnu99 -Wall -O2 -o $@ $^

boot.img: forth.dasm masm
	m4 $< > forth.s
	./masm forth.s $@

forth.img: forth.ft boot.img dcpu
	cat forth.ft | ./dcpu boot.img #> /dev/null
	mv core.img $@

clean:
	-rm -f *.o
	-rm -f dcpu
	-rm -f boot.img
	-rm -f forth.s forth.img

.PHONY: clean all
