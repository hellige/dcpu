        ; Try some basic stuff
                      set a, 0x30              ; 7c01 0030
                      set [0x1000], 0x20       ; 7de1 1000 0020
                      sub a, [0x1000]          ; 7803 1000
                      ifn a, 0x10              ; c00d 
                         set pc, crash         ; 7dc1 001a [*]
                      
        ; Do a loopy thing
                      set i, 10                ; a861
                      set a, 0x2000            ; 7c01 2000
        loop:         set [0x2000+i], [a]      ; 2161 2000
                      sub i, 1                 ; 8463
                      ifn i, 0                 ; 806d
                         set pc, loop          ; 7dc1 000d [*]
        
        ; Call a subroutine
                      set x, 0x4               ; 9031
                      jsr testsub              ; 7c10 0018 [*]
                      set pc, crash            ; 7dc1 001a [*]
        
        testsub:      shl x, 4                 ; 9037
                      set pc, pop              ; 61c1
                        
        ; Hang forever. X should now be 0x40 if everything went right.
        crash:        set pc, crash            ; 7dc1 001a [*]
        
        ; [*]: Note that these can be one word shorter and one cycle faster by using the short form (0x00-0x1f) of literals,
        ;      but my assembler doesn't support short form labels yet.    

                      dw 1, 2, 3, 4, 5, crash, testsub, 6, 7
                      dz "ABCabc Hi World!"
