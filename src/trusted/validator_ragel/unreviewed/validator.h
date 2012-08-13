/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_VALIDATOR_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_VALIDATOR_H_

#include "native_client/src/trusted/validator_ragel/unreviewed/decoder.h"

EXTERN_C_BEGIN

enum validation_errors {
  /* Unrecognized instruction: fatal error, processing stops here.  */
  UNRECOGNIZED_INSTRUCTION      = 0x00000001,
  /* Direct jump to unaligned address outside of given region.  */
  DIRECT_JUMP_OUT_OF_RANGE      = 0x00000002,
  /* Instruction is not allowed on current CPU.  */
  CPUID_UNSUPPORTED_INSTRUCTION = 0x00000004,
  /* Base register can be one of: %r15, %rbp, %rip, %rsp.  */
  FORBIDDEN_BASE_REGISTER       = 0x00000008,
  /* Index must be restricted if present.  */
  UNRESTRICTED_INDEX_REGISTER   = 0x00000010,
  /* Operations with %ebp must be followed with sandboxing immediately.  */
  RESTRICTED_RBP_UNPROCESSED    = 0x00000020,
  /* Attemp to "sandbox" %rbp without restricting it first.  */
  UNRESTRICTED_RBP_PROCESSED    = 0x00000040,
  /* Operations with %esp must be followed with sandboxing immediately.  */
  RESTRICTED_RSP_UNPROCESSED    = 0x00000080,
  /* Attemp to "sandbox" %rsp without restricting it first.  */
  UNRESTRICTED_RSP_PROCESSED    = 0x00000100,
  /* Operations with %r15 are forbidden.  */
  R15_MODIFIED                  = 0x00000200,
  /* Operations with SPL are forbidden for compatibility with old validator.  */
  BPL_MODIFIED                  = 0x00000400,
  /* Operations with SPL are forbidden for compatibility with old validator.  */
  SPL_MODIFIED                  = 0x00000800,
  /* %rsi must be sandboxed in instructions cmpsb, lods, and movs.  */
  RSI_UNSANDBOXDED              = 0x00001000,
  /* %rdi must be sandboxed in instructions cmpsb, movs, scas, and stos.  */
  RDI_UNSANDBOXDED              = 0x00002000,
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

typedef void (*process_validation_error_func) (const uint8_t *ptr,
                                               uint32_t validation_error,
                                               void *userdata);

int ValidateChunkAMD64(const uint8_t *data, size_t size,
                       enum validation_options options,
                       const NaClCPUFeaturesX86 *cpu_features,
                       process_validation_error_func process_error,
                       void *userdata);

int ValidateChunkIA32(const uint8_t *data, size_t size,
                      enum validation_options options,
                      const NaClCPUFeaturesX86 *cpu_features,
                      process_validation_error_func process_error,
                      void *userdata);

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_VALIDATOR_H_ */
