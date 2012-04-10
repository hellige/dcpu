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
;     | 16 ... name
;     | 16 codeword          |
;     | 16 ... definition    |
;     +----------------------+
; in the case of primitives, codeword simply points to definition, which is
; machine code. (it could be anywhere, but will generally follow immediately
; after codeword.) in the case of forth words, we have:
;     +----------------------+         
;     | 16 link to prev word |
;     | 16 flags/name length |          DOCOL
;     | 16 ... name          |         +---------------------+
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

; TODO all the io primitives and the dictionary use whole words for ascii chars.
; this is much easier in the absence of byte addressing, but it's also terribly
; wasteful. fix.

define(s0_, 0xf000)

define(next,
           `set x, [y]
            add y, 1
            set pc, [x]')

define(pushrsp,
           `set push, $1')

define(poprsp,
           `set $1, pop')

define(link, 0)

define(f_immed, 0x80)
define(f_hidden, 0x20)
define(f_lenmask, 0x1f)

; defword(forthname, flags, asmname)
define(defword, `
name_$3:    dw link
            define(`link', name_$3)
            dw eval($2 | len($1))
            dzw "$1"
$3:         dw docol')
            ; word ptrs follow...

; defcode(forthname, flags, asmname)
define(defcode, `
name_$3:    dw link
            define(`link', name_$3)
            dw eval($2 | len(`$1'))
            dzw "$1"
$3:         dw code_$3
code_$3:')  ; code follows...

; defvar(forthname, asmname, value)
define(defvar, `
            defcode($1, 0, $2)
            sub z, 1
            set [z], var_$2
            next
var_$2:     dw $3
')

; defconst(forthname, asmname, value)
define(defconst, `
            defcode($1, 0, $2)
            sub z, 1
            set [z], $3
            next
')

; boot...
            set sp, 0               ; return stack grows down from 0xffff
            set z, s0_              ; data stack grows down from here
            set y, boot             ; boot up
            next

boot:       dw quit

; interpreter for forth words
docol:      pushrsp(y)
            set y, x                ; set ip to word definition...
            add y, 1                ; which is one word after the code field
            next

; built-in constants and variables...
; 'latest' and 'h' are defined at the very end, so they can be properly initialized...
            defconst(tib, tib, s0_)
            defvar(>in, inptr, 0)
            defvar(srcptr, srcptr, 0)
            defvar(srclen, srclen, 0)
            defvar(state, state, 0)
            defvar(r0, r0, 0)
            defvar(s0, s0, s0_)
            ; `defvar'(base, base, 10)   TODO

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

; basic arithmetic
            defcode(-, 0, minus)
            sub [1+z], [z]
            add z, 1
            next

            ; TODO more...

; comparisons
            defcode(0=, 0, zequ)
            ife [z], 0
            set pc, zequ.1
            set [z], 0
            next
zequ.1:     set [z], 0xffff
            next

            ; TODO more...

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

            ; ( src dest u -- )
            defcode(move, 0, move)
            set a, [2+z]            ; src in a
            set b, [1+z]            ; dest in b
            set c, b                ; limit in c
            add c, [z]
            add z, 3                ; pop args
move.2:     ife b, c                ; done?
            set pc, move.1
            set [b], [a]            ; assign
            add a, 1                ; increment
            add b, 1
            set pc, move.2          ; again...
move.1:     next

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

            defcode(0branch, 0, zbranch)
            ife [z], 0              ; check condition
            set pc, zbranch.1
            add z, 1                ; pop
            add y, 1                ; skip offset
            next
zbranch.1:  add z, 1                ; pop
            add y, [y]              ; take the branch
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

            defcode(cr, 0, cr)
            out 0x0a
            next

            ; TODO handle backspace
            ; ( a u1 -- u2 )
            defcode(accept, 0, accept)
            set a, [1+z]            ; dest addr in a
            add [z], a              ; limit addr in [z]
accept.1:   ife a, [z]              ; bail if at limit
            set pc, accept.2
            kbd [a]                 ; read
            ife [a], 0x0a           ; bail if newline
            set pc, accept.2
            add a, 1
            set pc, accept.1
accept.2:   add z, 1                ; pop 1 arg
            sub a, [z]              ; compute length
            set [z], a              ; push it
            next

            ; TODO handle non-printing chars
            ; ( a u -- )
            defcode(type, 0, type)
            set a, [1+z]            ; src addr in a
            add [z], a              ; limit addr in [z]
type.1:     ife a, [z]              ; bail if at limit
            set pc, type.2
            out [a]                 ; write
            add a, 1
            set pc, type.1
type.2:     add z, 2                ; pop args
            next

            ; ( -- ? )
            defword(refill, 0, refill)
            dw tib
            dw dup, srcptr, store   ; set srcptr to tib
            dw lit, 80, accept      ; read line into tib
            dw srclen, store        ; set srclen to result
            dw lit, 0, inptr, store ; set >in to 0
            dw lit, 0xffff          ; push true
            dw exit

            ; ( -- a len )
            defword(source, 0, source)
            dw srcptr, fetch        ; push ptr and len
            dw srclen, fetch
            dw exit


; parsing, compiling
            ; TODO byte addressing!
            ; ( char "ccc<char>" -- a u )
            defcode(parse, 0, parse)
            set i, [z]              ; load terminator
            sub z, 1                ; we'll have one more result than arg
            set a, [var_srcptr]     ; load srcptr
            set c, a
            add c, [var_srclen]     ; store limit in c
            add a, [var_inptr]      ; add >in, a is current parse location
parse.2:    set [1+z], a            ; push start of word
parse.3:    ife a, c
            set pc, parse.end       ; input exhausted, bail
            ife [a], i              ; check for terminator
            set pc, parse.end2      ; if found, done!
            add a, 1
            set pc, parse.3
parse.end2: add a, 1                ; include terminator
parse.end:  set [z], a              ; push parsed length
            sub [z], [1+z]
            set [var_inptr], a      ; store `next' location back to >in
            sub [var_inptr], [var_srcptr]
            next

            ; TODO other space chars!
            ; TODO byte addressing!
            ; ( "<spaces>name" -- a u )
            defcode(parse-word, 0, parseword)
            sub z, 2                ; we'll have two results
            set a, [var_srcptr]     ; load srcptr
            set c, a
            add c, [var_srclen]     ; store limit in c
            add a, [var_inptr]      ; add >in, a is current parse location
pword.1:    ife a, c
            set pc, pword.2         ; input exhausted, bail
            ifn [a], 0x20           ; skip initial spaces
            set pc, pword.2
            add a, 1
            set pc, pword.1
pword.2:    set [1+z], a            ; push start of word
pword.3:    ife a, c
            set pc, pword.end       ; input exhausted, bail
            ife [a], 0x20
            set pc, pword.end       ; space, all done!
            add a, 1
            set pc, pword.3
pword.end:  set [z], a              ; push parsed length
            sub [z], [1+z]
            set [var_inptr], a      ; store `next' location back to >in
            sub [var_inptr], [var_srcptr]
            next


            ; >number ( u1 a1 u1 -- u2 a2 u2 ) TODO wrap!

            ; TODO other bases!
            ; convert word to number. word len in a, ptr in b, accumulator in i.
            ; modifies i, advances b and decrements a as it parses...
tonum_:     ife a, 0                ; bail if exhausted
            set pc, tonum_.end
            ifg [b], 0x39           ; bail if char > '9'
            set pc, tonum_.end
            ifg 0x30, [b]           ; bail if char < '0'
            set pc, tonum_.end
            mul i, 10               ; shift i one place
            set j, [b]
            sub j, 0x30             ; remove '0'
            add i, j                ; add it
            sub a, 1                ; advance to `next' char
            add b, 1
            set pc, tonum_          ; and begin again...
tonum_.end: set pc, pop

            ; convert word to number, possibly negative.
            ; takes word len in a, ptr in b.
            ; returns result in i, advances b and decrements a as it parses...
number_:    set i, 0                ; no accumulator so far
            set push, 0             ; number is positive
            ife a, 0                ; bail if exhausted
            set pc, numb_.end
            ifn [b], 0x2d           ; if first char is '-'
            set pc, numb_.pos
            set peek, 1             ; number is negative
            sub a, 1                ; skip '-'
            add b, 1
numb_.pos:  jsr tonum_
numb_.end:  ife pop, 0              ; if positive, we're done
            set pc, pop
            xor i, 0xffff           ; else negate
            add i, 1
            set pc, pop

            defword(allot, 0, allot)
            dw h, addstore          ; increment h
            dw exit

            defword(create, 0, create)
            dw here                 ; save current h
            dw latest, fetch, comma ; compile link ptr
            dw latest, store        ; update latest
            dw parseword            ; parse a word TODO check for empty!
            dw dup, comma           ; compile name length
            dw here, swap           ; set up to copy, stack ( src dest n -- )
            dw dup, allot           ; advance h first
            dw move                 ; copy name
            dw exit

            ; ( addr -- )  toggles hidden given dict entry
            defcode(hidden, 0, hidden)
            set a, [z]
            add a, 1                ; find flags/len
            xor [a], f_hidden       ; toggle
            add z, 1                ; pop
            next

            defcode(immediate, f_immed, immediate)
            set a, [var_latest]     ; get latest
            add a, 1                ; find flags/len
            xor [a], f_immed        ; toggle
            next

            defword(here, 0, here)
            dw h, fetch
            dw exit

            ; we've reached the limits of masm's hokey parsing...
            ; this is just `defcode' expanded and without dzw:
            ;`defcode'(`,', 0, comma)
name_comma: dw link
            define(`link', name_comma)
            dw 1                    ; no flags, length 1
            dw 0x2c                 ; comma
comma:      dw code_comma
code_comma: ; code follows...
            set j, [z]              ; set up the call
            jsr comma_
            add z, 1                ; pop stack
            next

            ; factored out so we can also call it natively.
            ; expects word to compile in j
comma_:     set b, [var_h]          ; fetch `next' dest
            set [b], j              ; write the value
            add [var_h], 1          ; increment h
            set pc, pop

            defcode([, f_immed, lbrac)
            set [var_state], 0      ; done compiling
            next

            defword(], 0, rbrac)
            dw lit, 1, state, store ; begin compiling
            dw parseword
            dw dup, zbranch, 12     ; done?
            dw compile_             ; call the primitive
            dw succ, fetch
            dw zbranch, 9           ; error, nothing to drop, just bail
            dw state, fetch         ; must retest state flag, an immediate word
            dw zbranch, 4           ;   can change it...
            dw branch, -14
            dw twodrop
            dw exit
            ; TODO error handling! really we should abort here or whatever...
            dw lit, 0x65, emit      ; but for now, we can at least spew an 'e'
            dw exit

            ; this can't return anything, since execution will cause us to
            ; eventually `next' our way back into forth code. we could make
            ; it work by fiddling around with the forth ip, but it doesn't seem
            ; worth it... we use a var to communicate errors...
            ; ( a u -- )
compile_:   dw compile__
compile__:  set a, [z]              ; load length
            set b, [1+z]            ; load ptr
            add z, 2
            set [var_succ], 0xffff
            jsr find_               ; dictionary search, results in i, j
            ife i, 0
            set pc, compile_.1      ; not in dict...
            ife i, 1
            set pc, compile_.4      ; not an immediate word.
            set x, j                ; immediate word. update the codeword pointer
            set pc, [x]             ; and execute it...
compile_.4: jsr comma_              ; non-immediate. compile the value in j.
            set pc, compile_.3
compile_.1: jsr number_
            ife a, 0
            set pc, compile_.2
            set [var_succ], 0x0     ; not a number. bail.
            set pc, compile_.3
compile_.2: set j, lit
            jsr comma_              ; compile ptr to 'lit'
            set j, i
            jsr comma_              ; compile the numeric value in j
compile_.3: next

            defword(:, 0, colon)
            dw create               ; make the dict entry
            dw lit, docol, comma    ; append the codeword
            dw latest, fetch, hidden; hide it
            dw rbrac                ; compile
            dw exit

            defword(;, f_immed, semi)
            dw lit, exit, comma     ; append exit
            dw latest, fetch, hidden; unhide it
            dw lbrac                ; stop compiling
            dw exit


; dictionary
            ; ( a u -- 0 0  |  xt 1  |  xt -1 )
            defcode(find, 0, find)
            set a, [z]
            set b, [1+z]
            jsr find_
            set [1+z], j
            set [z], i
            next

            ; find word in dictionary. word len in a, ptr in b.
            ; returns xt in j, i = 1, -1, 0 for found, immediate, and not found
            ; does NOT clobber a or b
find_:      set c, [var_latest]     ; start with most recent word
find_.1:    ife c, 0                ; end of dict?
            set pc, find_.fail
            set j, [1+c]            ; load flags/len
            ifb j, f_hidden         ; ignore if hidden
            set pc, find_.2
            and j, f_lenmask        ; mask off flags
            ifn j, a                ; compare length
            set pc, find_.2

            set i, b                ; i points into input word
            set j, c                ; j points into dict word
            add j, 2
            set push, i             ; stack gets limit of input word
            add peek, a
find_.3:    ife i, peek             ; check for full match
            set pc, find_.succ
            ifn [i], [j]
            set pc, find_.4
            add i, 1
            add j, 1
            set pc, find_.3

find_.4:    set i, pop              ; pop limit, dest doesn't matter
find_.2:    set c, [c]              ; `next' word in dict
            set pc, find_.1

find_.succ: set i, pop              ; pop limit, dest doesn't matter
            ; j already points to the xt...
            set i, 1                ; found
            ifb [1+c], f_immed      ; immediate?
            set i, -1               ; if so, mark it
            set pc, pop

find_.fail: set i, 0
            set j, 0
            set pc, pop


; the outer interpreter
            defvar(succ, succ, 0)

            ; ( -- ? )
            defword(interpret, 0, interpret)
            dw parseword
            dw dup, zbranch, 8      ; done?
            dw interpret_           ; call the primitive
            dw succ, fetch          ; check for error
            dw zbranch, 7           ; error!
            dw branch, -10          ; keep parsing...
            dw twodrop, lit, 0xffff ; push true
            dw exit
            dw lit, 0x0             ; push false
            dw exit

            ; this can't return anything, since execution will cause us to
            ; eventually `next' our way back into forth code. we could make
            ; it work by fiddling around with the forth ip, but it doesn't seem
            ; worth it... we use a var to communicate errors...
            ; ( a u -- )
interpret_: dw interp_
interp_:    set a, [z]              ; load length
            set b, [1+z]            ; load ptr
            add z, 2
            set [var_succ], 0xffff
            jsr find_               ; dictionary search, results in i, j
            ife i, 0
            set pc, interp_.1       ; not in dict...
            set x, j                ; found. update the codeword pointer
            set pc, [x]             ; and execute it...
interp_.1:  jsr number_
            ife a, 0
            set pc, interp_.2
            set [var_succ], 0x0     ; not a number. bail.
            set pc, interp_.3
interp_.2:  sub z, 1                ; got a number. push it.
            set [z], i
interp_.3:  next

; quit is also the bootstrap word...
            ; TODO check refill result (although it can't currently fail)
            defword(quit, 0, quit)
            dw r0, rspstore         ; empty return stack
            dw refill, drop         ; refill input buffer
            dw interpret, zequ      ; run
            dw zbranch, 2           ; check result
            dw err
            dw prompt               ; display prompt
            dw branch, -9           ; again

err:        dw err_
err_:       out 0x20
            out 0x45
            out 0x52
            out 0x52
            out 0x4f
            out 0x52
            next

prompt:     dw prompt_
prompt_:    out 0x20
            out 0x6f
            out 0x6b
            out 0x0a
            next

; define h down here, so it's clear where it's coming from...
            defvar(h, h, h_init)
; define latest last, so that it can start out pointing to itself.
            defvar(latest, latest, name_latest)
; label for initial value for h
h_init:     ; nothing here!

