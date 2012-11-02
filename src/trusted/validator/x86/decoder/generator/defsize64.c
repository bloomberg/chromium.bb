/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * Captures instructions that assumes the default size is 64 bits,
 * instead of 32 bits. That is, effective operand size is 64 bits,
 * even without a rex prefix.
 *
 * The tables below are from Table B-5 of Appendex B.4 in AMD document
 * 24594-Rev.3.14-September 2007, "AMD64 Architecture Programmer's manual
 * Volume 3: General-Purpose and System Instructions".
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/trusted/validator/x86/decoder/generator/defsize64.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_forms.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_tablegen.h"

/* List of instruction mnemonics that assumes the default size is 64 bits,
 * and the mnemonic is sufficient to disambiguate the case.
 */
static const NaClMnemonic kNameSizeDefaultIs64[] = {
  InstEnter,          /* c8 */
  InstJo,             /* 70, 0f 80 */
  InstJno,            /* 71, 0f 81 */
  InstJb,             /* 72, 0f 82 */
  InstJnb,            /* 73, 0f 83 */
  InstJz,             /* 74, 0f 84 */
  InstJnz,            /* 75, 0f 85 */
  InstJbe,            /* 76, 0f 86 */
  InstJnbe,           /* 77, 0f 87 */
  InstJs,             /* 78, 0f 88 */
  InstJns,            /* 79, 0f 89 */
  InstJp,             /* 7a, 0f 8a */
  InstJnp,            /* 7b, 0f 8b */
  InstJl,             /* 7c, 0f 8c */
  InstJnl,            /* 7d, 0f 8d */
  InstJle,            /* 7e, 0f 8e */
  InstJnle,           /* 7f, 0f 8f */
  InstJcxz,           /* e3 */
  InstJecxz,          /* e3 */
  InstJrcxz,          /* e3 */
  InstLeave,          /* c9 */
  InstLoop,           /* e2 */
  InstLoopne,         /* e0 */
  InstLoope,          /* e1 */
  /*
  InstMovmskpd,       ** 66 0f 50 - Intel(tm) claims 64-bit size assumption,
                       * while AMD does not. If we go ahead and assume that
                       * all 64-bits can be effected, we do not break anything.
                       * Hence, we generalize to 64-bit default.
                       */
  InstPopf,           /* 9d */
  InstPopfd,          /* 9d */
  InstPopfq,          /* 9d */
  InstPushf,          /* 9c */
  InstPushfd,         /* 9c */
  InstPushfq          /* 9c */

};

static const NaClNameOpcodeSeq kNameSeqSizeDefaultIs64[] = {
  { InstCall , { 0xe8 , END_OPCODE_SEQ } },
  { InstCall , { 0xff , SL(2) , END_OPCODE_SEQ } },
  { InstJmp , { 0xe9 , END_OPCODE_SEQ } },
  { InstJmp , { 0xeb , END_OPCODE_SEQ } },
  { InstJmp , { 0xff , SL(4) , END_OPCODE_SEQ } },
  { InstPop , { 0x58 , END_OPCODE_SEQ } },
  { InstPop , { 0x59 , END_OPCODE_SEQ } },
  { InstPop , { 0x5a , END_OPCODE_SEQ } },
  { InstPop , { 0x5b , END_OPCODE_SEQ } },
  { InstPop , { 0x5c , END_OPCODE_SEQ } },
  { InstPop , { 0x5d , END_OPCODE_SEQ } },
  { InstPop , { 0x5e , END_OPCODE_SEQ } },
  { InstPop , { 0x5f , END_OPCODE_SEQ } },
  { InstPop , { 0x8f , SL(0) , END_OPCODE_SEQ } },
  { InstPop , { 0x0f , 0xa1 , END_OPCODE_SEQ } } ,
  { InstPop , { 0x0f , 0xa9 , END_OPCODE_SEQ } },
  { InstPush , { 0x50 , END_OPCODE_SEQ } },
  { InstPush , { 0x51 , END_OPCODE_SEQ } },
  { InstPush , { 0x52 , END_OPCODE_SEQ } },
  { InstPush , { 0x53 , END_OPCODE_SEQ } },
  { InstPush , { 0x54 , END_OPCODE_SEQ } },
  { InstPush , { 0x55 , END_OPCODE_SEQ } },
  { InstPush , { 0x56 , END_OPCODE_SEQ } },
  { InstPush , { 0x57 , END_OPCODE_SEQ } },
  { InstPush , { 0x6a , END_OPCODE_SEQ } },
  { InstPush , { 0x68 , END_OPCODE_SEQ } },
  { InstPush , { 0xff , SL(6) , END_OPCODE_SEQ } },
  { InstPush , { 0x0f , 0xa0 , END_OPCODE_SEQ } },
  { InstPush , { 0x0f , 0xa8 , END_OPCODE_SEQ } },
  { InstRet , { 0xc2 , END_OPCODE_SEQ } },
  { InstRet , { 0xc3 , END_OPCODE_SEQ } },
};

void NaClAddSizeDefaultIs64(void) {
  NaClModeledInst* inst = NaClGetDefInst();
  if ((X86_64 == NACL_FLAGS_run_mode) &&
      (NaClOperandSizes(inst) & NACL_IFLAG(OperandSize_o)) &&
      NaClInInstructionSet(kNameSizeDefaultIs64,
                           NACL_ARRAY_SIZE(kNameSizeDefaultIs64),
                           kNameSeqSizeDefaultIs64,
                           NACL_ARRAY_SIZE(kNameSeqSizeDefaultIs64))) {
    NaClAddIFlags(NACL_IFLAG(OperandSizeDefaultIs64));
  }
}
