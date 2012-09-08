/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Implement the functions common for ia32 and x86-64 architectures.  */
#include "native_client/src/trusted/validator_ragel/unreviewed/dfa_validate_common.h"

#include <string.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/service_runtime/arch/x86/sel_ldr_x86.h"
#include "native_client/src/trusted/validator_ragel/unreviewed/validator.h"

/* Used as an argument to copy_func when unsupported instruction must be
   replaced with HLTs.  */
static const uint8_t kStubOutMem[MAX_INSTRUCTION_LENGTH] = {
  NACL_HALT_OPCODE, NACL_HALT_OPCODE, NACL_HALT_OPCODE, NACL_HALT_OPCODE,
  NACL_HALT_OPCODE, NACL_HALT_OPCODE, NACL_HALT_OPCODE, NACL_HALT_OPCODE,
  NACL_HALT_OPCODE, NACL_HALT_OPCODE, NACL_HALT_OPCODE, NACL_HALT_OPCODE,
  NACL_HALT_OPCODE, NACL_HALT_OPCODE, NACL_HALT_OPCODE
};

Bool ProcessError(const uint8_t *begin, const uint8_t *end, uint32_t info,
                  void *callback_data) {
  UNREFERENCED_PARAMETER(begin);
  UNREFERENCED_PARAMETER(end);
  UNREFERENCED_PARAMETER(info);

  /* Instruction is unsupported by CPU, but otherwise valid.  */
  if ((info & VALIDATION_ERRORS_MASK) == CPUID_UNSUPPORTED_INSTRUCTION) {
    *((enum NaClValidationStatus*)callback_data) =
        NaClValidationFailedCpuNotSupported;
  }
  return FALSE;
}

Bool StubOutCPUUnsupportedInstruction(const uint8_t *begin,
                                      const uint8_t *end,
                                      uint32_t info,
                                      void *callback_data) {
  UNREFERENCED_PARAMETER(callback_data);

  /* Stub-out instructions unsupported on this CPU, but valid on other CPUs.  */
  if ((info & VALIDATION_ERRORS_MASK) == CPUID_UNSUPPORTED_INSTRUCTION) {
    memset((uint8_t *)begin, NACL_HALT_OPCODE, end - begin + 1);
    return TRUE;
  } else {
    return FALSE;
  }
}

Bool ProcessCodeCopyInstruction(const uint8_t *begin_new,
                                const uint8_t *end_new,
                                uint32_t info,
                                void *callback_data) {
  struct CodeCopyCallbackData *data = callback_data;

  /* Sanity check: instruction must be shorter then 15 bytes.  */
  CHECK(end_new - begin_new < MAX_INSTRUCTION_LENGTH);

  return data->copy_func(
      (uint8_t *)begin_new + data->delta, /* begin_existing */
      (info & VALIDATION_ERRORS_MASK) == CPUID_UNSUPPORTED_INSTRUCTION ?
        (uint8_t *)kStubOutMem :
        (uint8_t *)begin_new,
      (uint8_t)(end_new - begin_new + 1));
}

Bool CodeReplacementIsStubouted(const uint8_t *begin_existing,
                                size_t instruction_length) {

  /* Unsupported instruction must have been replaced with HLTs.  */
  if (memcmp(kStubOutMem, begin_existing, instruction_length) == 0)
    return TRUE;
  else
    return FALSE;
}
