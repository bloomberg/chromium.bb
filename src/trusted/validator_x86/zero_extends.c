/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Captures instructions that zero extend 32 bit values.
 *
 * Extracted from table B-1 in AMD document 25494 - AMD64 Architecture
 * Programmer's Manual, Volume 3: General-Purpose and System Instructions.
 *
 * Note: This is used by the x86-64 validator to decide what operations can
 * be used for control/memory access.
 */

#include "native_client/src/trusted/validator_x86/zero_extends.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"

/* 0..256    => Opcode byte.
 * SL(N)     => /N
 * IGN       => Not part of prefix.
 */
typedef int16_t OpcodeSeq[NACL_MAX_OPCODE_BYTES];

/* Value out of range to denote no opcode entry. */
#define IGN 512

/* Define value in modrm (i.e. /n in opcode sequence). */
#define SL(n) (-(n))

/* Returns true if opcode sequence value is a SL (slash)
 * value.
 */
static Bool IsSLValue(int16_t val) {
  return val < 0;
}

/* Returns the opcode denoted by a SL (slash) value. */
static NaClOpKind SLOpcode(int16_t val) {
  return Opcode0 - val;
}

/* Model instruction that zero etends 32 bit values. */
typedef struct ZeroExtendOpcodeSeq {
  NaClMnemonic name;
  OpcodeSeq opcode_seq;
} ZeroExtendOpcodeSeq;

/* List of instruction nmemonics that zero extend 32 bit results.
 *
 * Note: Instructions commented out are not (yet) implemented by
 * the validator, and hence not defined. They have been added here
 * so that they can (easily) be added as necessary.
 */
static const NaClMnemonic kZeroExtend32Op[] = {
  InstAdc,            /* 11, 13, 15, 81/2, 83/2 */
  InstAdd,            /* 01, 03, 05, 81/0, 83/0 */
  InstAnd,            /* 21, 23, 25, 81/4, 83/4 */
  InstBsf,            /* OF BC */
  InstBsr,            /* OF DB */
  InstBswap,          /* OF C* through OF CF */
  InstBt,             /* OF A3, 0F BA/4 */
  InstBtc,            /* OF BB, OF BA/7 */
  InstBtr,            /* OF B3, OF BA/6 */
  InstBts,            /* OF AB, OF BA /5 */
  InstCmovo,          /* All CMovcc ops: 0F 40 through 0F 4F. */
  InstCmovno,
  InstCmovb,
  InstCmovnb,
  InstCmovz,
  InstCmovnz,
  InstCmovbe,
  InstCmovnbe,
  InstCmovs,
  InstCmovns,
  InstCmovp,
  InstCmovnp,
  InstCmovl,
  InstCmovnl,
  InstCmovle,
  InstCmovnle,
  InstCmpxchg,        /* 0F B1 */
  InstCpuid,          /* OF A2 */
  InstDiv,            /* f7/6 */
  InstIdiv,           /* f7/7 */
  InstImul,           /* f7/5 , 0f af , 69 , 6b */
  InstIn,             /* E5 , ED */
  /* InstLar,            // 0F , 02 */
  InstLea,            /* 8D */
  /* InstLfs,            // 0f b4 */
  /* InstLgs,            // 0f b5 */
  /* InstLsl,            // 0f 03 */
  /* InstLss,            // 0f b2 */
  /* InstLzcnt,          // f3 0f bd */
  InstMov,            /* 89, 8b, c7, b8 through bf, a1 (moffset),
                         a3 (moffset)
                      */
  InstMovd,           /* 0f 6e, 0f 7e */
  InstMovsx,          /* 0f be, 0f bf */
  InstMovsxd,         /* 63 */
  InstMovzx,          /* 0f b6, 0f b7 */
  InstMul,            /* f7/4 */
  InstNeg,            /* f7/3 */
  InstNot,            /* f7/2 */
  InstOr,             /* 09, 0b, 0d, 81/1, 83/1 */
  /* InstPopcnt,         // f3 0f b8 */
  InstRcl,            /* D1/2, D3/2, C1/2 */
  InstRcr,            /* D1/3, D3/3, c1/3 */
  /* InstRdmsr,          // 0f 32 */
  /* InstRdpmc,          // 0f 33 */
  /* InstRdtsc           // 0f 31 */
  /* InstRdtscp          // 0f 01 f9 */
  InstRol,            /* D1/0, D3/0, C1/0 */
  InstRor,            /* D1/1, D3/1, C1/1 */
  InstSar,            /* D1/7, D3/7, C1/7 */
  InstSbb,            /* 19, 1b, 1d, 81/3, 83/3 */
  InstShl,            /* D1/4, D3/4, C1/4 */
  InstShld,           /* 0f a4, 0f a5 */
  InstShr,            /* D1/5, D3/5, C1/5 */
  /* InstShrd,           // 0f ac, 0f ad */
  /* InstSmsw,           // 0f 01/4 */
  InstSub,            /* 29, 2b, 2d, 81/5, 83/5 */
  InstXadd,           /* 0f c1 */
  InstXchg,           /* 87, 90 through 97 */
  InstXor,            /* 31, 33, 35, 81/6, 83/6 */
};

/* List of instruction nmemonics, for specific opcode sequences, that zero
 * extend 32 bit results.
 */
static const ZeroExtendOpcodeSeq kZeroExtend32Opseq[] = {
  { InstDec , { 0xFF , SL(1) } },
  { InstInc , { 0xFF , SL(0) } },
};

/* Add OperandZeroExtends_v to instruction, if it can hold
 * a 32 bit operand.
 */
static void AddZeroExtendToOpDestArgs(NaClInst* inst) {
  if (inst->flags & NACL_IFLAG(OperandSize_v)) {
    int i;
    for (i = 0; i < inst->num_operands; ++i) {
      if (inst->operands[i].flags & NACL_OPFLAG(OpDest)) {
        NaClAddOpFlags(i, NACL_OPFLAG(OperandZeroExtends_v));
      }
    }
  }
}

/* Add OperandZeroExtends_v instruction flag if applicable. */
void NaClAddZeroExtend32FlagIfApplicable() {
  int i;
  NaClInst* inst = NaClGetDefInst();

  /* First handle cases where all instances of an operation should
   * zero extend.
   */
  for (i = 0; i < NACL_ARRAY_SIZE(kZeroExtend32Op); ++i) {
    if (inst->name == kZeroExtend32Op[i]) {
      AddZeroExtendToOpDestArgs(inst);
      return;
    }
  }

  /* Now handle cases where only specific operand sequences of
   * an operation should update.
   */
  for (i = 0; i < NACL_ARRAY_SIZE(kZeroExtend32Opseq); ++i) {
    if (inst->name == kZeroExtend32Opseq[i].name) {
      int j;
      Bool is_good = TRUE;
      /* First compare opcode bytes. */
      for (j = 0; j < inst->num_opcode_bytes; ++j) {
        if (inst->opcode[j] != kZeroExtend32Opseq[i].opcode_seq[j]) {
          is_good = FALSE;
          break;
        }
      }
      if (is_good) {
        /* Now compare any values that must by in modrm. */
        for (j = inst->num_opcode_bytes; j < NACL_MAX_OPCODE_BYTES; ++j) {
          int16_t val = kZeroExtend32Opseq[i].opcode_seq[j];
          if (val == IGN) {
            /* At end of descriptor, matched! */
            break;
          }
          if (IsSLValue(val)) {
            /* Compare to opcode in operand[0]. */
            if ((inst->num_operands > 0) &&
                (inst->operands[0].flags &&
                 NACL_OPFLAG(OperandExtendsOpcode)) &&
                (SLOpcode(val) ==  inst->operands[0].kind)) {
              /* good, continue search. */
            } else {
              is_good = FALSE;
              break;
            }
          }
        }
        if (is_good) {
          AddZeroExtendToOpDestArgs(inst);
          return;
        }
      }
    }
  }
}
