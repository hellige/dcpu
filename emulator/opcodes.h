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
  apply(OP_MLI, "mli") \
  apply(OP_DIV, "div") \
  apply(OP_DVI, "dvi") \
  apply(OP_MOD, "mod") \
  apply(OP_MDI, "mdi") \
  apply(OP_AND, "and") \
  apply(OP_BOR, "bor") \
  apply(OP_XOR, "xor") \
  apply(OP_SHR, "shr") \
  apply(OP_ASR, "asr") \
  apply(OP_SHL, "shl") \
  apply(OP_IFB, "ifb") \
  apply(OP_IFC, "ifc") \
  apply(OP_IFE, "ife") \
  apply(OP_IFN, "ifn") \
  apply(OP_IFG, "ifg") \
  apply(OP_IFA, "ifa") \
  apply(OP_IFL, "ifl") \
  apply(OP_IFU, "ifu") \
  apply(OP_XX0, "xx0") \
  apply(OP_XX1, "xx1") \
  apply(OP_ADX, "adx") \
  apply(OP_SBX, "sbx") \
  apply(OP_XX2, "xx2") \
  apply(OP_XX3, "xx3") \
  apply(OP_STI, "sti") \
  apply(OP_STD, "std")

#define ID(x, _) x,
enum opcode {
  FOR_EACH_OP(ID)
};
#undef ID


#define FOR_EACH_SPOP(apply) \
  apply(OP_SP_JSR, "jsr", 0x01) \
  /* custom ops */ \
  apply(OP_SP_IMG, "img", 0x02) /* save core to core.img, up to addr in a */ \
  apply(OP_SP_DIE, "die", 0x03) /* exit emulator */ \
  apply(OP_SP_DBG, "dbg", 0x04) /* enter the emulator debugger */ \
  \
  apply(OP_SP_INT, "int", 0x08) \
  apply(OP_SP_IAG, "iag", 0x09) \
  apply(OP_SP_IAS, "ias", 0x0a) \
  apply(OP_SP_RFI, "rfi", 0x0b) \
  apply(OP_SP_IAQ, "iaq", 0x0c) \
  apply(OP_SP_HWN, "hwn", 0x10) \
  apply(OP_SP_HWQ, "hwq", 0x11) \
  apply(OP_SP_HWI, "hwi", 0x12)

#define ID(x, _, n) x = n,
enum spopcode {
  FOR_EACH_SPOP(ID)
  NUM_SPOPCODES // max number, may be sparse...
};
#undef ID


extern const char *opnames[];
extern const char *spopnames[];


#endif
