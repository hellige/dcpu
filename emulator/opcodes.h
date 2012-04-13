/*
 * Copyright (c) 2012, Matt Hellige
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions are met:
 *
 *   Redistributions of source code must retain the above copyright notice, 
 *   this list of conditions and the following disclaimer.
 *
 *   Redistributions in binary form must reproduce the above copyright 
 *   notice, this list of conditions and the following disclaimer in the 
 *   documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR 
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT 
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT 
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, 
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY 
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT 
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE 
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef opcodes_h
#define opcodes_h


#define FOR_EACH_OP(apply) \
  apply(OP_NON, "xxx") \
  apply(OP_SET, "set") \
  apply(OP_ADD, "add") \
  apply(OP_SUB, "sub") \
  apply(OP_MUL, "mul") \
  apply(OP_DIV, "div") \
  apply(OP_MOD, "mod") \
  apply(OP_SHL, "shl") \
  apply(OP_SHR, "shr") \
  apply(OP_AND, "and") \
  apply(OP_BOR, "bor") \
  apply(OP_XOR, "xor") \
  apply(OP_IFE, "ife") \
  apply(OP_IFN, "ifn") \
  apply(OP_IFG, "ifg") \
  apply(OP_IFB, "ifb")

#define ID(x, _) x,
enum opcode {
  FOR_EACH_OP(ID)
  NUM_OPCODES
};
#undef ID


#define FOR_EACH_NBOP(apply) \
  apply(OP_NB_NON, "xxx") \
  apply(OP_NB_JSR, "jsr") \
  /* custom ops */ \
  apply(OP_NB_OUT, "out") /* ascii char to console */ \
  apply(OP_NB_KBD, "kbd") /* ascii char from keybaord, store in a */ \
  apply(OP_NB_IMG, "img") /* save core image to core.img, up to address in a */ \
  apply(OP_NB_DIE, "die") /* exit emulator */ \
  apply(OP_NB_DBG, "dbg") /* enter the emulator debugger */

#define ID(x, _) x,
enum nbopcode {
  FOR_EACH_NBOP(ID)
  NUM_NBOPCODES
};
#undef ID


extern const char *opnames[];
extern const char *nbopnames[];


#endif
