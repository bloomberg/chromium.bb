/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Captures instructions that zero extend 32 bit values.
 *
 * Extracted from table B-1 in AMD document 25494 - AMD64 Architecture
 * Programmer's Manual, Volume 3: General-Purpose and System Instructions.
 *
 * Note: This is used by the x86-64 validator to decide what operations can
 * be used to mask control/memory accesses. Therefore, not all instructions
 * listed in table B-1 (above) are defined to zero-extend. In particular,
 * this is a NaCl-specific flag that is used to whitelist instructions
 * that can be used to zero-extend (mask) 32-bit addresses. As such, although
 * some instruction can zero-extend, we don't necessarily allow them here.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator/x86/decoder/generator/zero_extends.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_forms.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_tablegen.h"

/* List of instruction mnemonics that zero extend 32 bit results. */
static const NaClMnemonic kZeroExtend32Op[] = {
  /* Note: The following instructions are listed in table B-1 (above), but are
   * not included for the following reasons:
   *
   * Bsf and Bsr instructions are not included because they conditionally
   * write the destination. See
   * http://code.google.com/p/nativeclient/issues/detail?id=2010
   *
   * Cmovcc instructions (0f40 through 0f4f) are not included out of caution.
   * Table B-1 (above) states that these instrucitons always zero extends 32 bit
   * register results to 64 bits, and that this occurs even if the condition
   * is false. However, we decided to be safe and omit these instructions
   * anyway.
   *
   * Cmpxchg is not included because it conditionally writes to
   * different destinations.
   *
   * Bswap is not included because we don't really think that
   * swapping the bottom bytes is a good thing to do to mask a memory
   * reference.
   *
   * Bt, Btc, Btr, Bts, Rcl, Rcr, Rol, Rol, Sar, Shl, Shld, Shr, and
   * Shrd instructions are not included out of caution. We assume that
   * these instructions are used for bit manipulation rather than
   * computing viable addresses.
   *
   * Adc and Sbb are not included since borrows/carries should not be
   * need when calculating an address.
   *
   * Cpuid, In, and Lar instructions are not included just because they
   * shouldn't be.
   *
   * Div, Idiv, and Mul instructions are not included because we never
   * allowed them.
   *
   * Lfs, Lgs, Lss, and Lsl instructions are not included because segment
   * addresses shouldn't be used.
   *
   * Lzcnt is not included because it is unlikely to be useful for
   * computing an address based on the number of leading zeros of a value.
   *
   * Pextrb is not included because you shouldn't use MMX registers
   * to hold addresses.
   *
   * Popcnt is not included because it is unlikely to be useful for computing
   * an address based on the number of ones in a value.
   *
   * Rdmsr is not included, because you shouldn't be reading model specific
   * registers.
   *
   * Rdpmc is not included because you shouldn't be basing addresses on
   * the performance-monitor counter.
   *
   * Rdtsc and Rdtscp are not included because you shouldn't be basing
   * addresses on the time-stamp counter or the processor id.
   *
   * Smsw is not included since you shouldn't be basing addresses on the
   * machine status word.
   */
  InstAdd,            /* 01, 03, 05, 81/0, 83/0 */
  InstAnd,            /* 21, 23, 25, 81/4, 83/4 */
  InstImul,           /* f7/5 , 0f af , 69 , 6b */
  InstLea,            /* 8D */
  InstMov,            /* 89, 8b, c7, b8 through bf, a1 (moffset),
                         a3 (moffset)
                      */
  InstMovd,           /* 0f 6e, 0f 7e */
  InstMovsx,          /* 0f be, 0f bf */
  InstMovsxd,         /* 63 */
  InstMovzx,          /* 0f b6, 0f b7 */
  InstNeg,            /* f7/3 */
  InstNot,            /* f7/2 */
  InstOr,             /* 09, 0b, 0d, 81/1, 83/1 */
  InstSub,            /* 29, 2b, 2d, 81/5, 83/5 */
  InstXadd,           /* 0f c1 */
  InstXchg,           /* 87, 90 through 97 */
  InstXor,            /* 31, 33, 35, 81/6, 83/6 */
};

/* List of instruction nmemonics, for specific opcode sequences, that zero
 * extend 32 bit results.
 */
static const NaClNameOpcodeSeq kZeroExtend32Opseq[] = {
  { InstDec , { 0xFF , SL(1) , END_OPCODE_SEQ } },
  { InstInc , { 0xFF , SL(0) , END_OPCODE_SEQ } },
};

/* Add OperandZeroExtends_v to instruction, if it can hold
 * a 32 bit operand.
 */
static void AddZeroExtendToOpDestArgs(NaClModeledInst* inst) {
  if (inst->flags & NACL_IFLAG(OperandSize_v)) {
    int i;
    for (i = 0; i < inst->num_operands; ++i) {
      /* Note: we currently don't allow zero extends for
       * implicit arguments. This is done just to be extra
       * cautious on what we allow to be masks in the
       * NaCl x64-64 validator.
       */
      if ((inst->operands[i].flags & NACL_OPFLAG(OpSet)) &&
          (NACL_EMPTY_OPFLAGS ==
           (inst->operands[i].flags & NACL_OPFLAG(OpImplicit)))) {
        NaClAddOpFlags(i, NACL_OPFLAG(OperandZeroExtends_v));
      }
    }
  }
}

/* Add OperandZeroExtends_v instruction flag if applicable. */
void NaClAddZeroExtend32FlagIfApplicable(void) {
  if ((X86_64 == NACL_FLAGS_run_mode) &&
      NaClInInstructionSet(kZeroExtend32Op,
                           NACL_ARRAY_SIZE(kZeroExtend32Op),
                           kZeroExtend32Opseq,
                           NACL_ARRAY_SIZE(kZeroExtend32Opseq))) {
    AddZeroExtendToOpDestArgs(NaClGetDefInst());
  }
}
