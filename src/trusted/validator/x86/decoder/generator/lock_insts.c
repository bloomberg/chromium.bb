/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Captures instructions that can be locked.
 *
 * Extracted from zection '1.2.5 Lock Prefix' in AMD Document 25494 - AMD64
 * Architecture Programmer's Manual, Volume 3: General-Purpose and System
 * Instructions.
 *
 * Note: This is used by the x86-64 validator to decide what operations can
 * be locked.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator/x86/decoder/generator/lock_insts.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_forms.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_tablegen.h"

/* List of instruction nmemonics that can be locked. */
static const NaClMnemonic kLockableOp[] = {
  InstAdc,
  InstAdd,
  InstAnd,
  InstBtc,
  InstBtr,
  InstBts,
  InstCmpxchg,
  /* Note: The following two instructions are not implemented as separate
   * instructions from Cmpxchg, but are separated in the AMD manual.
   */
  InstCmpxchg8b,
  InstCmpxchg16b,
  InstDec,
  InstInc,
  InstNeg,
  InstNot,
  InstOr,
  InstSbb,
  InstSub,
  InstXadd,
  InstXchg,
  InstXor
};

void NaClLockableFlagIfApplicable(void) {
  if (NaClInInstructionSet(kLockableOp, NACL_ARRAY_SIZE(kLockableOp),
                           NULL, 0)) {
    NaClAddIFlags(NACL_IFLAG(OpcodeLockable));
  }
}
