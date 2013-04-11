/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Implement the functions common for ia32 and x86-64 architectures.  */
#include "native_client/src/trusted/validator_ragel/dfa_validate_common.h"

#include <string.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/validator_ragel/validator.h"

/* Used as an argument to copy_func when unsupported instruction must be
   replaced with HLTs.  */
static const uint8_t kStubOutMem[MAX_INSTRUCTION_LENGTH] = {
  NACL_HALT_OPCODE, NACL_HALT_OPCODE, NACL_HALT_OPCODE, NACL_HALT_OPCODE,
  NACL_HALT_OPCODE, NACL_HALT_OPCODE, NACL_HALT_OPCODE, NACL_HALT_OPCODE,
  NACL_HALT_OPCODE, NACL_HALT_OPCODE, NACL_HALT_OPCODE, NACL_HALT_OPCODE,
  NACL_HALT_OPCODE, NACL_HALT_OPCODE, NACL_HALT_OPCODE, NACL_HALT_OPCODE,
  NACL_HALT_OPCODE
};

Bool NaClDfaProcessValidationError(const uint8_t *begin, const uint8_t *end,
                                   uint32_t info, void *callback_data) {
  UNREFERENCED_PARAMETER(begin);
  UNREFERENCED_PARAMETER(end);
  UNREFERENCED_PARAMETER(info);
  UNREFERENCED_PARAMETER(callback_data);

  return FALSE;
}

Bool NaClDfaStubOutCPUUnsupportedInstruction(const uint8_t *begin,
                                             const uint8_t *end,
                                             uint32_t info,
                                             void *callback_data) {
  /* Stub-out instructions unsupported on this CPU, but valid on other CPUs.  */
  if ((info & VALIDATION_ERRORS_MASK) == CPUID_UNSUPPORTED_INSTRUCTION) {
    int *did_stubout = callback_data;
    *did_stubout = 1;
    memset((uint8_t *)begin, NACL_HALT_OPCODE, end - begin);
    return TRUE;
  } else {
    return FALSE;
  }
}

Bool NaClDfaProcessCodeCopyInstruction(const uint8_t *begin_new,
                                       const uint8_t *end_new,
                                       uint32_t info_new,
                                       void *callback_data) {
  struct CodeCopyCallbackData *data = callback_data;
  size_t instruction_length = end_new - begin_new;

  /* Sanity check: instruction must be no longer than 17 bytes.  */
  CHECK(instruction_length <= MAX_INSTRUCTION_LENGTH);

  return data->copy_func(
      (uint8_t *)begin_new + data->existing_minus_new, /* begin_existing */
      (info_new & VALIDATION_ERRORS_MASK) == CPUID_UNSUPPORTED_INSTRUCTION ?
        (uint8_t *)kStubOutMem :
        (uint8_t *)begin_new,
      (uint8_t)instruction_length);
}

Bool NaClDfaCodeReplacementIsStubouted(const uint8_t *begin_existing,
                                       size_t instruction_length) {

  /* Unsupported instruction must have been replaced with HLTs.  */
  if (memcmp(kStubOutMem, begin_existing, instruction_length) == 0)
    return TRUE;
  else
    return FALSE;
}
