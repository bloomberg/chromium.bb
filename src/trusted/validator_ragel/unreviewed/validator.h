/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_VALIDATOR_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_VALIDATOR_H_

#include "native_client/src/trusted/validator_ragel/unreviewed/decoder.h"

EXTERN_C_BEGIN

enum validation_callback_info {
  /* validation_error & IMMEDIATES_INFO will give you immediates size.  */
  IMMEDIATES_SIZE               = 0x0000000f,
  /* Immediate sizes (immediates always come at the end of instruction).  */
  IMMEDIATE_8BIT                = 0x00000001,
  IMMEDIATE_16BIT               = 0x00000002,
  IMMEDIATE_32BIT               = 0x00000004,
  IMMEDIATE_64BIT               = 0x00000008,
  /* Relative size (relative fields always come at the end if instriction).  */
  RELATIVE_8BIT                 = 0x00000011,
  RELATIVE_16BIT                = 0x00000012,
  RELATIVE_32BIT                = 0x00000014,
  /* Not a normal immediate: only two bits can be changed.  */
  IMMEDIATE_2BIT                = 0x00000080,
  /* Mask to select all validation errors.  */
  VALIDATION_ERRORS             = 0x003fff00,
  /* Unrecognized instruction: fatal error, processing stops here.  */
  UNRECOGNIZED_INSTRUCTION      = 0x00000100,
  /* Direct jump to unaligned address outside of given region.  */
  DIRECT_JUMP_OUT_OF_RANGE      = 0x00000200,
  /* Instruction is not allowed on current CPU.  */
  CPUID_UNSUPPORTED_INSTRUCTION = 0x00000400,
  /* Base register can be one of: %r15, %rbp, %rip, %rsp.  */
  FORBIDDEN_BASE_REGISTER       = 0x00000800,
  /* Index must be restricted if present.  */
  UNRESTRICTED_INDEX_REGISTER   = 0x00001000,
  /* Operations with %ebp must be followed with sandboxing immediately.  */
  RESTRICTED_RBP_UNPROCESSED    = 0x00002000,
  /* Attemp to "sandbox" %rbp without restricting it first.  */
  UNRESTRICTED_RBP_PROCESSED    = 0x00004000,
  /* Operations with %esp must be followed with sandboxing immediately.  */
  RESTRICTED_RSP_UNPROCESSED    = 0x00008000,
  /* Attemp to "sandbox" %rsp without restricting it first.  */
  UNRESTRICTED_RSP_PROCESSED    = 0x00010000,
  /* Operations with %r15 are forbidden.  */
  R15_MODIFIED                  = 0x00020000,
  /* Operations with SPL are forbidden for compatibility with old validator.  */
  BPL_MODIFIED                  = 0x00040000,
  /* Operations with SPL are forbidden for compatibility with old validator.  */
  SPL_MODIFIED                  = 0x00080000,
  /* %rsi must be sandboxed in instructions cmpsb, lods, and movs.  */
  RSI_UNSANDBOXDED              = 0x00100000,
  /* %rdi must be sandboxed in instructions cmpsb, movs, scas, and stos.  */
  RDI_UNSANDBOXDED              = 0x00200000,
  /* Bad jump target.  Note: in this case ptr points to jump target!  */
  BAD_JUMP_TARGET               = 0x10000000
};

#define kBundleSize 32
#define kBundleMask 31

enum validation_options {
  /* Call process_error function on instruction.  */
  CALL_USER_FUNCTION_ON_EACH_INSTRUCTION = 0x00000001,
  /* Process all instruction as a contiguous stream.  */
  PROCESS_CHUNK_AS_A_CONTIGUOUS_STREAM   = 0x00000002
};

/*
 * If CALL_USER_FUNCTION_ON_EACH_INSTRUCTION option is used then callback
 * is called on each instruction, otherwise only errorneous instructions
 * are inspected via callback.  If callback returns FALSE at least once
 * then ValidateChunkXXX returns false, if callback marks all the error
 * as unimportant by always returning TRUE then ValidateChunkXXX returns
 * TRUE as well.
 */
typedef Bool (*validation_callback_func) (const uint8_t *instruction_start,
                                          const uint8_t *instruction_end,
                                          uint32_t validation_info,
                                          void *userdata);

Bool ValidateChunkAMD64(const uint8_t *data, size_t size,
                        enum validation_options options,
                        const NaClCPUFeaturesX86 *cpu_features,
                        validation_callback_func user_callback,
                        void *callback_data);

Bool ValidateChunkIA32(const uint8_t *data, size_t size,
                       enum validation_options options,
                       const NaClCPUFeaturesX86 *cpu_features,
                       validation_callback_func user_callback,
                       void *callback_data);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_VALIDATOR_H_ */
