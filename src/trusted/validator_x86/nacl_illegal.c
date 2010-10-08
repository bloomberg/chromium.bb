/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
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

#include "native_client/src/trusted/validator_x86/nacl_illegal.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/validator_x86/ncdecode_forms.h"
#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"

/* List of instruction mnemonics that are illegal. */
static const NaClMnemonic kNaClIllegalOp[] = {
  /* TODO(karl) This list is incomplete. As we fix instructions to use the new
   * generator model, this list will be extended.
   */
  InstDas,
  InstLeave,
  /* Note: Ud2 is special. We except the instruction sequence "0f0b" (with no
   * no prefix bytes) as a special case on a nop instruction. The entry below
   * amkes all forms with a prefix bytes illegal.
   */
  InstUd2,
  /* Note: Disallow 3DNow, since it is a placeholder for decoding 3DNOW
   * instructions that we can detect, but not properly decode.
   */
  Inst3DNow
};

static const NaClNameOpcodeSeq kNaClIllegalOpSeq[] = {
  /* The AMD manual shows 0x82 as a synonym for 0x80 in 32-bit mode only.
   * They are illegal in 64-bit mode. We omit them for both cases.
   */
  { InstAdd , { 0x82 , SL(0) , END_OPCODE_SEQ } },
  { InstOr  , { 0x82 , SL(1) , END_OPCODE_SEQ } },
  { InstAdc , { 0x82 , SL(2) , END_OPCODE_SEQ } },
  { InstSbb , { 0x82 , SL(3) , END_OPCODE_SEQ } },
  { InstAnd , { 0x82 , SL(4) , END_OPCODE_SEQ } },
  { InstSub , { 0x82 , SL(5) , END_OPCODE_SEQ } },
  { InstXor , { 0x82 , SL(6) , END_OPCODE_SEQ } },
  { InstCmp , { 0x82 , SL(7) , END_OPCODE_SEQ } },

  /* It may be the case that these can also be enabled by CPUID_ECX_PRE; */
  /* This enabling is not supported by the validator at this time. */
  { InstPrefetch , { 0x0f , 0x0d , SL(2) , END_OPCODE_SEQ } },
  { InstPrefetch , { 0x0f , 0x0d , SL(3) , END_OPCODE_SEQ } },
  { InstPrefetch , { 0x0f , 0x0d , SL(4) , END_OPCODE_SEQ } },
  { InstPrefetch , { 0x0f , 0x0d , SL(5) , END_OPCODE_SEQ } },
  { InstPrefetch , { 0x0f , 0x0d , SL(6) , END_OPCODE_SEQ } },
  { InstPrefetch , { 0x0f , 0x0d , SL(7) , END_OPCODE_SEQ } },
};

void NaClAddNaClIllegalIfApplicable() {
  Bool is_illegal = FALSE;  /* until proven otherwise. */
  NaClInst* inst = NaClGetDefInst();

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
    case NACLi_LONGMODE:
    case NACLi_SVM:
    case NACLi_3BYTE:
    case NACLi_CMPXCHG16B:
    case NACLi_UNDEFINED:
    case NACLi_INVALID:
    case NACLi_SYSCALL:
    case NACLi_SYSENTER:
      is_illegal = TRUE;
      break;
    default:
      if (NaClInInstructionSet(
              kNaClIllegalOp, NACL_ARRAY_SIZE(kNaClIllegalOp),
              kNaClIllegalOpSeq, NACL_ARRAY_SIZE(kNaClIllegalOpSeq))) {
        is_illegal = TRUE;
      }
      break;
  }
  if (is_illegal) {
    NaClAddIFlags(NACL_IFLAG(NaClIllegal));
  }
}
