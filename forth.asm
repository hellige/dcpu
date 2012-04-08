; m4 is a pretty broke-ass way to get macros, but i want to invest as little
; as possible in the dcpu assembler. i'd rather bootstrap everything from
; within forth as soon as possible...

; registers:
;  SP  return stack pointer (rsp sometimes below)
;  Z   data stack pointer
;  Y   forth instruction pointer (ip)
;  X   forth codeword pointer

; dictionary entries:
;     +----------------------+
;     | 16 link to prev word |
;     | 16 flags/name length |
;     |  8 ... name/pad      |
;     | 16 codeword          |
;     | 16 ... definition    |
;     +----------------------+
; in the case of primitives, codeword simply points to definition, which is
; machine code. (it could be anywhere, but will generally follow immediately
; after codeword.) in the case of forth words, we have:
;     +----------------------+         
;     | 16 link to prev word |
;     | 16 flags/name length |          DOCOL
;     |  8 ... name/pad      |         +---------------------+
;     | 16 ptr to DOCOL     ---------->| 16 ... machine code |
;     | 16 ... ptr to words  |         | 16 ... NEXT         |
;     | 16 ptr to EXIT       |         +---------------------+
;     +----------------------+
; 'ptr to ...' means a pointer directly to the codeword field of the definition.

; the overall memory layout:
;     +----------------------+
;     | block buffers (2kb)  |<--- r0
;     | return stack         |
;     |        |             |
;     |        v             |
;     | text input buffer    |<--- tib,s0
;     | data stack           |
;     |        |             |
;     |        v             |
;     | pad (n words from h) |<--- pad
;     |        ^             |
;     |        |             |<--- h
;     | user dictionary      |
;     | system variables     |
;     | core image           |
;     +----------------------+


define(next,
           `set x, [y]
            add y, 1
            set pc, [x]')

define(pushrsp,
           `set push, $1')

define(poprsp,
           `set $1, pop')

define(link, 0)

define(defword, `
name_$3:    dw link
            define(`link', name_$3)
            dw len($1)  ; TODO flags!
            dz "$1"
$3:         dw docol')
            ; word ptrs follow...

define(defcode, `
name_$3:    dw link
            define(`link', name_$3)
            dw len($1)  ; TODO flags!
            dz "$1"
$3:         dw code_$3
code_$3:')  ; code follows...

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

; now we start right in with a bunch of primitive words...

; stack manipulation:
            defcode(drop, 0, drop)
            add z, 1
            next

            defcode(swap, 0, swap)
            set a, [z]
            set [z], [1+z]
            set [1+z], a
            next

            defcode(dup, 0, dup)
            sub z, 1
            set [z], [1+z]
            next

            defcode(over, 0, over)
            sub z, 1
            set [z], [2+z]
            next

            defcode(rot, 0, rot)
            set a, [z]
            set [z], [2+z]
            set [2+z], [1+z]
            set [1+z], a
            next

            defcode(-rot, 0, nrot)
            set a, [z]
            set [z], [1+z]
            set [1+z], [2+z]
            set [2+z], a
            next

            defcode(2drop, 0, twodrop)
            add z, 2
            next

            defcode(2dup, 0, twodup)
            sub z, 2
            set [1+z], [3+z]
            set [z], [2+z]
            next

            defcode(2swap, 0, twoswap)
            set a, [z]
            set b, [1+z]
            set [z], [2+z]
            set [1+z], [3+z]
            set [2+z], a
            set [3+z], b
            next

            defcode(?dup, 0, qdup)
            ifn [z], 0
            add pc, 2
            sub z, 1
            set [z], [1+z]
            next

; TODO basic arithmetic
; TODO comparisons
; TODO logic

; internals
            defcode(exit, 0, exit)
            poprsp(y)
            next

            defcode(lit, 0, lit)
            ; push the value at the forth ip and advance it
            sub z, 1
            set [z], [y]
            add y, 1
            next

; memory
            defcode(!, 0, store)
            set a, [z]              ; fetch destination address
            set [a], [1+z]          ; store the `next' stack value to dest
            add z, 2                ; pop both values
            next

            defcode(@, 0, fetch)
            set a, [z]              ; fetch source address
            set [z], [a]            ; read, replace top of stack
            next

            defcode(+!, 0, addstore)
            set a, [z]              ; fetch destination address
            add [a], [1+z]          ; add `next' value to dest
            add z, 2                ; pop both values
            next

            defcode(-!, 0, substore)
            set a, [z]              ; fetch destination address
            sub [a], [1+z]          ; subtract `next' value from dest
            add z, 2                ; pop both values
            next

; direct stack access
            defcode(>r, 0, tor)
            pushrsp([z])
            add z, 1
            next

            defcode(<r, 0, fromr)
            sub z, 1
            poprsp([z])
            next

            defcode(rsp!, 0, rspstore)
            set sp, [z]
            add z, 1
            next

            defcode(rsp@, 0, rspfetch)
            sub z, 1
            set [z], sp
            next

            defcode(rdrop, 0, rdrop)
            poprsp(a)               ; destination is irrelevant
            next

            defcode(dsp!, 0, dspstore)
            set z, [z]
            next

            defcode(dsp@, 0, dspfetch)
            set a, z
            sub z, 1
            set [z], a
            next

            defcode(branch, 0, branch)
            add y, [y]
            next

; i/o
            defcode(key, 0, key)
            sub z, 1
            kbd [z]
            next

            defcode(emit, 0, emit)
            out [z]
            add z, 1
            next

; quit is also the bootstrap word...
            defword(quit, 0, quit)
            dw prompt, key, emit, branch, -4

prompt:     dw prompt_
prompt_:    out 0x0a
            out 0x6b
            out 0x65
            out 0x79
            out 0x3a
            out 0x20
            next
