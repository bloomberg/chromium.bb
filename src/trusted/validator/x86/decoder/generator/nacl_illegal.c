/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Captures instructions that are considered illegal in native client.
 *
 * Note: This is used by the x86-64 validator to decide what instructions
 * should be flagged as illegal. It is expected to also be used for
 * the x86-32 validator sometime in the future.
 *
 * Note: This code doesn't include rules to check for the case of
 * instructions that are near (relative) jumps with operand word size,
 * when decoding 64-bit instructions. These instructions are marked
 * illegal separately by DEF_OPERAND(Jzw) in ncdecode_forms.c
 * See Call/Jcc instructions in Intel document
 * 253666-030US - March 2009, "Intel 654 and IA-32 Architectures
 * Software Developer's Manual, Volume2A", which specifies that
 * such instructions are not supported on all platforms.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator/x86/decoder/generator/nacl_illegal.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_forms.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_tablegen.h"

/* List of instruction mnemonics that are illegal. */
static const NaClMnemonic kNaClIllegalOp[] = {
  /* TODO(karl) This list is incomplete. As we fix instructions to use the new
   * generator model, this list will be extended.
   */
  /* ISE reviewers suggested making loopne, loope, loop, jcxz illegal */
  InstAaa,
  InstAad,
  InstAam,
  InstAas,
  InstBound,
  InstDaa,
  InstDas,
  InstEnter,
  InstIn,
  InstInsb,
  InstInsd,
  InstInsw,
  InstInt,
  InstInto,
  InstInt1,
  InstInt3,
  InstLes,
  InstLds,
  InstLfs,
  InstLgs,
  InstLss,
  InstIret,
  InstIretd,
  InstIretq,
  InstLeave,
  InstOut,
  InstOutsb,
  InstOutsd,
  InstOutsw,
  InstPopa,
  InstPopad,
  InstPopf,
  InstPopfd,
  InstPopfq,
  InstPrefetch_reserved,
  InstPusha,
  InstPushad,
  InstPushf,
  InstPushfd,
  InstPushfq,
  InstRet,
  /* TODO(Karl): Intel manual (see comments above) has a blank entry
   * for opcode 0xd6, which states that blank entries in the tables
   * correspond to reserved (undefined) values Should we treat this
   * accordingly? Making illegal till we know more.
   */
  InstSalc,
  InstSysret,
  /* Note: Ud2 is special. We except the instruction sequence "0f0b" (with no
   * no prefix bytes) as a special case on a nop instruction. The entry below
   * amkes all other forms, i.e. with a prefix bytes, illegal.
   */
  InstUd2,
  InstXlat,  /* ISE reviewers suggested this omision */
};

static const NaClNameOpcodeSeq kNaClIllegalOpSeq[] = {
  /* The AMD manual shows 0x82 as a synonym for 0x80 in 32-bit mode only.
   * They are illegal in 64-bit mode. We omit them for both cases.
   */
  { InstPush, { 0x06 , END_OPCODE_SEQ } },
  { InstPush, { 0x0e , END_OPCODE_SEQ } },

  /* The following are illegal since they are define by AMD(tm), but not
   * Intel(TM).
   */
  { InstNop,  { 0x0f , 0x19 , END_OPCODE_SEQ } },
  { InstNop,  { 0x0f , 0x1a , END_OPCODE_SEQ } },
  { InstNop,  { 0x0f , 0x1b , END_OPCODE_SEQ } },
  { InstNop,  { 0x0f , 0x1c , END_OPCODE_SEQ } },
  { InstNop,  { 0x0f , 0x1d , END_OPCODE_SEQ } },
  { InstNop,  { 0x0f , 0x1e , END_OPCODE_SEQ } },
  { InstNop,  { 0x0f , 0x1f , END_OPCODE_SEQ } },

  /* Disallow pushing/popping to segment registers. */
  { InstPush, { 0x06 , END_OPCODE_SEQ } },
  { InstPush, { 0x16 , END_OPCODE_SEQ } },
  { InstPush, { 0x0e , END_OPCODE_SEQ } },
  { InstPush, { 0x1e , END_OPCODE_SEQ } },
  { InstPush, { 0x0f , 0xa0 , END_OPCODE_SEQ } },
  { InstPush, { 0x0f , 0xa8 , END_OPCODE_SEQ } },
  { InstPop , { 0x07 , END_OPCODE_SEQ } },
  { InstPop , { 0x17 , END_OPCODE_SEQ } },
  { InstPop , { 0x1f , END_OPCODE_SEQ } },
  { InstPop , { 0x0f , 0xa1 , END_OPCODE_SEQ } },
  { InstPop , { 0x0f , 0xa9 , END_OPCODE_SEQ } },

  /* The following operations are provided as a synonym
   * for the corresponding 0x80 code. NaCl requires the
   * use of the 0x80 version.
   */
  { InstAdd , { 0x82 , SL(0) , END_OPCODE_SEQ } },
  { InstOr  , { 0x82 , SL(1) , END_OPCODE_SEQ } },
  { InstAdc , { 0x82 , SL(2) , END_OPCODE_SEQ } },
  { InstSbb , { 0x82 , SL(3) , END_OPCODE_SEQ } },
  { InstAnd , { 0x82 , SL(4) , END_OPCODE_SEQ } },
  { InstSub , { 0x82 , SL(5) , END_OPCODE_SEQ } },
  { InstXor , { 0x82 , SL(6) , END_OPCODE_SEQ } },
  { InstCmp , { 0x82 , SL(7) , END_OPCODE_SEQ } },

  /* TODO(Karl): Don't know why these are disallowed. */
  { InstMov , { 0x8c , END_OPCODE_SEQ } },
  { InstMov , { 0x8e , END_OPCODE_SEQ } },

  /* Don't allow far calls/jumps. */
  { InstCall , { 0x9a , END_OPCODE_SEQ } },
  /* Note: special case 64-bit with 66 prefix, which is not suppported on some
   * Intel Chips. See explicit rules in ncdecode_onebyte.c for specific
   * override.
   * See Call instruction in Intel document 253666-030US - March 2009,
   * "Intel 654 and IA-32 Architectures Software Developer's Manual, Volume2A".
   * { InstCall , { 0xe8 , END_OCCODE_SEQ } } with prefix 66
   */
  { InstJmp , { 0xea , END_OPCODE_SEQ } },
  { InstCall, { 0xff , SL(3), END_OPCODE_SEQ } },
  { InstJmp , { 0xff , SL(5), END_OPCODE_SEQ } },

  /* ISE reviewers suggested omitting bt. Issues have with how many bytes are
   * accessable when using memory for bit base. Note: Current solution is
   * to allow the form that uses a byte, but not general memory/registers.
   * This allows bit access to all standard size integers, but doesn't allow
   * accesses that are very far away.
   */
  { InstBt  , { 0x0f , 0xa3 , END_OPCODE_SEQ } },
  { InstBtc , { 0x0f , 0xbb , END_OPCODE_SEQ } },
  { InstBtr , { 0x0f , 0xb3 , END_OPCODE_SEQ } },
  { InstBts , { 0x0f , 0xab , END_OPCODE_SEQ } },

  /* Added the group17 form of this instruction, since xed does not implement,
   * just to be safe. Note: The form in 660F79 is still allowed.
   */
  { InstExtrq , { PR(0x66) , 0x0f, 0x78 , SL(0), END_OPCODE_SEQ } },
};

/* Holds illegal opcode sequences for 32-bit model only. */
static const NaClNameOpcodeSeq kNaClIllegal32OpSeq[] = {
  /* ISE reviewers suggested omitting bt, btc, btr and bts, but bt must
   * be kept in 64-bit mode, because the compiler needs it to access
   * the top 32-bits of a 64-bit value.
   * Note: For 32-bit mode, we followed the existing implementation
   * that doesn't even allow the one byte form.
   */
  { InstBt  , { 0x0f , 0xba , SL(4) , END_OPCODE_SEQ } },
  { InstBts , { 0x0f , 0xba , SL(5) , END_OPCODE_SEQ } },
  { InstBtr , { 0x0f , 0xba , SL(6) , END_OPCODE_SEQ } },
  { InstBtc , { 0x0f , 0xba , SL(7) , END_OPCODE_SEQ } },
};

void NaClAddNaClIllegalIfApplicable(void) {
  Bool is_illegal = FALSE;  /* until proven otherwise. */
  NaClModeledInst* inst = NaClGetDefInst();

  /* TODO(karl) Once all instructions have been modified to be explicitly
   * marked as illegal, remove the corresponding switch from nc_illegal.c.
   *
   * Note: As instructions are modified to use the new generator model,
   * The file testdata/64/modeled_insts.txt will reflect it by showing
   * the NaClIllegal flag.
   */
  /* Be sure to handle instruction groups we don't allow. */
  switch (inst->insttype) {
    case NACLi_RETURN:
    case NACLi_EMMX:
      /* EMMX needs to be supported someday but isn't ready yet. */
    case NACLi_ILLEGAL:
    case NACLi_SYSTEM:
    case NACLi_RDMSR:
    case NACLi_RDTSCP:
    case NACLi_SVM:
    case NACLi_3BYTE:
    case NACLi_UNDEFINED:
    case NACLi_INVALID:
    case NACLi_SYSCALL:
    case NACLi_SYSENTER:
    case NACLi_VMX:
    case NACLi_FXSAVE:  /* don't allow until we can handle. */
      is_illegal = TRUE;
      break;
    default:
      if (NaClInInstructionSet(
              kNaClIllegalOp, NACL_ARRAY_SIZE(kNaClIllegalOp),
              kNaClIllegalOpSeq, NACL_ARRAY_SIZE(kNaClIllegalOpSeq)) ||
          ((X86_32 == NACL_FLAGS_run_mode) &&
           NaClInInstructionSet(
               NULL, 0,
               kNaClIllegal32OpSeq, NACL_ARRAY_SIZE(kNaClIllegal32OpSeq)))) {
        is_illegal = TRUE;
      }
      break;
  }
  if (is_illegal) {
    NaClAddIFlags(NACL_IFLAG(NaClIllegal));
  }
}
