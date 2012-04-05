; registers:
;  SP  return stack pointer (rsp sometimes below)
;  Z   data stack pointer
;  Y   forth instruction pointer (ip)
;  X   forth codeword pointer

; dictionary entries:
;     +----------------------+
;     | 16 link to prev word |
;     |  8 flags/name length |
;     |  8 ... name/pad      |
;     | 16 codeword          |
;     | 16 ... definition    |
;     +----------------------+
; in the case of primitives, codeword simply points to definition, which is
; machine code. in the case of forth words, we have:
;     +----------------------+
;     | 16 link to prev word |
;     |  8 flags/name length |
;     |  8 ... name/pad      |
;     | 16 ptr to DOCOL      |
;     | 16 ... ptr to words  |
;     | 16 ptr to EXIT       |
;     +----------------------+
; 'ptr to ...' means a pointer directly to the codeword field of the definition.

define(next,
           `set x, [y]
            add y, 1
            set pc, [x]')

define(pushrsp,
           `set push, $1')

define(poprsp,
           `set $1, pop')

; boot...
            set sp, 0               ; return stack grows down from 0xffff
            set z, 0xf000           ; data stack grows down fom 0xefff
            set y, boot             ; boot up
            next

boot:       dw quit

; interpreter for forth words
docol:      pushrsp(y)
            set y, x                ; set ip to word definition...
            add y, 1                ; which is one word after the code field
            next

quit:       dw docol
            dw print0               ; for now, we just print '0' and halt
            dw halt

print0:     dw print0_
print0_:    out 0x30
            next

halt:       dw halt_
halt_:      set pc, halt_
