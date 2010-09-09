/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Captures instructions that assumes the default size is 64 bits,
 * instead of 32 bits. That is, effective operand size is 64 bits,
 * even without a rex prefix.
 *
 * TODO(karl) Find a manual reference for how entries are defined
 * in this table.
 */

#include "native_client/src/trusted/validator_x86/zero_extends.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/validator_x86/ncdecode_forms.h"
#include "native_client/src/trusted/validator_x86/ncdecode_tablegen.h"

static const NaClNameOpcodeSeq kSizeDefaultIs64[] = {
  { InstPush , { 0x50 , END_OPCODE_SEQ } },
  { InstPush , { 0x51 , END_OPCODE_SEQ } },
  { InstPush , { 0x52 , END_OPCODE_SEQ } },
  { InstPush , { 0x53 , END_OPCODE_SEQ } },
  { InstPush , { 0x54 , END_OPCODE_SEQ } },
  { InstPush , { 0x55 , END_OPCODE_SEQ } },
  { InstPush , { 0x56 , END_OPCODE_SEQ } },
  { InstPush , { 0x57 , END_OPCODE_SEQ } },
  { InstPop  , { 0x58 , END_OPCODE_SEQ } },
  { InstPop  , { 0x59 , END_OPCODE_SEQ } },
  { InstPop  , { 0x5a , END_OPCODE_SEQ } },
  { InstPop  , { 0x5b , END_OPCODE_SEQ } },
  { InstPop  , { 0x5c , END_OPCODE_SEQ } },
  { InstPop  , { 0x5d , END_OPCODE_SEQ } },
  { InstPop  , { 0x5e , END_OPCODE_SEQ } },
  { InstPop  , { 0x5f , END_OPCODE_SEQ } },
  { InstPushfq , { 0x9c , END_OPCODE_SEQ } },
  { InstPush , { 0xff , SL(6), END_OPCODE_SEQ } },
};

void NaClAddSizeDefaultIs64() {
  NaClInst* inst = NaClGetDefInst();
  if ((X86_64 == NACL_FLAGS_run_mode) &&
      (NaClOperandSizes(inst) & NACL_IFLAG(OperandSize_o)) &&
      NaClInInstructionSet(NULL, 0,
                           kSizeDefaultIs64,
                           NACL_ARRAY_SIZE(kSizeDefaultIs64))) {
    NaClAddIFlags(NACL_IFLAG(OperandSizeDefaultIs64));
  }
}
