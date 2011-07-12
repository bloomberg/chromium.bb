/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Captures instructions that jump.
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator_x86/nc_def_jumps.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/validator_x86/ncdecode_forms.h"
#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"

/* List of instructions that do unconditional jumps. */
static const NaClMnemonic kDefinesUnconditionalJump[] = {
  InstJmp,
  InstLoop,
  InstCall,
};

/* List of instructions that do conditional jumps. */
static const NaClMnemonic kDefinesConditionalJump[] = {
  InstJb,
  InstJbe,
  InstJcxz,
  InstJecxz,
  InstJrcxz,
  InstJnl,
  InstJnle,
  InstJl,
  InstJle,
  InstJnb,
  InstJnbe,
  InstJno,
  InstJnp,
  InstJns,
  InstJnz,
  InstJo,
  InstJp,
  InstJs,
  InstJz,
  InstLoope,
  InstLoopne,
};

static void NaClAddJumpFlags(NaClIFlag flag, const NaClMnemonic* name,
                             size_t name_size) {
  if (NaClInInstructionSet(name, name_size, NULL, 0)) {
    NaClGetDefInst()->flags |= NACL_IFLAG(flag);
  }
}


void NaClAddJumpFlagsIfApplicable() {
  NaClAddJumpFlags(JumpInstruction, kDefinesUnconditionalJump,
                   NACL_ARRAY_SIZE(kDefinesUnconditionalJump));
  NaClAddJumpFlags(ConditionalJump, kDefinesConditionalJump,
                   NACL_ARRAY_SIZE(kDefinesConditionalJump));
}
