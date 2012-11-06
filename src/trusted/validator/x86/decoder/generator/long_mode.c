/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Captures instructions that require CPUID bit 29 set, i.e. are
 * a long mode instruction.
 *
 * Extracted from table D-1 in  AMD document 25494 - AMD64 Architecture
 * Programmer's Manual, Volume 3: General-Purpose and System Instructions.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator/x86/decoder/generator/zero_extends.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_forms.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_tablegen.h"

/*
 * List of instruction mnemonics that define long mode instructions.
 */
static const NaClMnemonic kLongModeOp[] = {
  /* Note: Commented out instructions have not yet been implemented. */
  InstCdqe,
  InstCmpsq,
  InstCqo,
  InstIretq,
  InstLodsq,
  InstMovsq,
  InstMovsxd,
  InstPopfq,
  /* InstPrefetchw */
  InstPushfq,
  InstScasq,
  InstStosq,
  InstSwapgs
};

/* Add LongMode instruction flag if applicable. */
void NaClAddLongModeIfApplicable(void) {
  if (NaClInInstructionSet(kLongModeOp, NACL_ARRAY_SIZE(kLongModeOp),
                           NULL, 0)) {
    NaClAddIFlags(NACL_IFLAG(LongMode));
  }
}
