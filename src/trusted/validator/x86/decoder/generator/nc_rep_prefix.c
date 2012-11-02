/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Captures instructions that allow REP and REPNE prefixes (other than
 * multibyte instructions that require the REP/REPNE prefix bytes).
 *
 * Extracted from tables 1-6. 1-7, and 1-8 in AMD document 25494 - AMD64
 * Architecture Programmer's Manual, Volume 3: General-Purpose and System
 * Instructions.
 *
 * Note: This is used by the x86-664 validator to decide what instructions
 * can have the REP/REPE/REPZ (F3) and the REPNE/REPNZ (F2) prefix byte
 * on an instruction.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator/x86/decoder/generator/nc_rep_prefix.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_forms.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_tablegen.h"

/* List of instruction mnemonics that always can have a REP/REPE/REPZ (F3)
 * prefix associated with the instruction.
 */
static const NaClMnemonic kAllowableRepMnemonic[] = {
  InstInsb,        /* 6c */
  InstInsd,        /* 6d */
  InstInsw,        /* 6d */
  InstLodsb,       /* ac */
  InstLodsd,       /* ad */
  InstLodsq,       /* ad */
  InstLodsw,       /* ad */
  InstOutsb,       /* 6e */
  InstOutsd,       /* 6f */
  InstOutsw,       /* 6f */
  InstStosb,       /* aa */
  InstStosd,       /* ab */
  InstStosq,       /* ab */
  InstStosw,       /* ab */
  InstCmpsb,       /* a6 */
  InstCmpsd,       /* a7 */
  InstCmpsq,       /* a7 */
  InstCmpsw,       /* a7 */
  InstScasb,       /* ae */
  InstScasd,       /* af */
  InstScasq,       /* af */
  InstScasw,       /* af */
};

/* List of instruction mnemonics that always can have a REPNE/REPNZ (F2) preix
 * associated with the instruction.
 */
static const NaClMnemonic kAllowableRepneMnemonic[] = {
  InstCmpsb,       /* a6 */
  InstCmpsd,       /* a7 */
  InstCmpsq,       /* a7 */
  InstCmpsw,       /* a7 */
  InstScasb,       /* ae */
  InstScasd,       /* af */
  InstScasq,       /* af */
  InstScasw,       /* af */
};

/* List of instruction opcode sequences that can have a REP/REPE/REPZ
 * (F3) prefix associated with the opcode sequence.
 */
static const NaClNameOpcodeSeq kAllowableRepMnemonicOpseq[] = {
  { InstMovsb , { 0xa4 , END_OPCODE_SEQ } },
  { InstMovsd , { 0xa5 , END_OPCODE_SEQ } },
  { InstMovsq , { 0xa5 , END_OPCODE_SEQ } },
  { InstMovsw , { 0xa5 , END_OPCODE_SEQ } },
};

void NaClAddRepPrefixFlagsIfApplicable(void) {
  if (NaClInInstructionSet(kAllowableRepMnemonic,
                           NACL_ARRAY_SIZE(kAllowableRepMnemonic),
                           kAllowableRepMnemonicOpseq,
                           NACL_ARRAY_SIZE(kAllowableRepMnemonicOpseq))) {
    NaClAddIFlags(NACL_IFLAG(OpcodeAllowsRep));
  }
  if (NaClInInstructionSet(kAllowableRepneMnemonic,
                           NACL_ARRAY_SIZE(kAllowableRepneMnemonic),
                           NULL, 0)) {
    NaClAddIFlags(NACL_IFLAG(OpcodeAllowsRepne));
  }
}
