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

/*
 * Returns whether a validation error should be ignored by
 * RewriteAndRevalidateBundle()'s two validation passes, which
 * validate individual instruction bundles.
 */
static Bool AllowErrorDuringBundleValidation(
    uint32_t info, struct StubOutCallbackData *data) {
  if ((info & VALIDATION_ERRORS_MASK) == DIRECT_JUMP_OUT_OF_RANGE) {
    /*
     * This error can occur on valid jumps because we are validating
     * an instruction bundle that is a subset of a code chunk.
     */
    return TRUE;
  }
  if ((info & VALIDATION_ERRORS_MASK) == UNSUPPORTED_INSTRUCTION) {
    return (data->flags & NACL_DISABLE_NONTEMPORALS_X86) == 0;
  }
  return FALSE;
}

/*
 * First pass of RewriteAndRevalidateBundle(): Rewrite any
 * instructions that need rewriting.
 */
static Bool BundleValidationApplyRewrite(const uint8_t *begin,
                                         const uint8_t *end,
                                         uint32_t info,
                                         void *callback_data) {
  struct StubOutCallbackData *data = callback_data;

  if ((info & VALIDATION_ERRORS_MASK) == CPUID_UNSUPPORTED_INSTRUCTION) {
    /* Stub-out instructions unsupported on this CPU, but valid on other CPUs.*/
    data->did_rewrite = 1;
    memset((uint8_t *) begin, NACL_HALT_OPCODE, end - begin);
    return TRUE;
  }
  return AllowErrorDuringBundleValidation(info, data);
}

/*
 * Second pass of RewriteAndRevalidateBundle(): Revalidate, checking
 * that no further instruction rewrites are needed.
 */
static Bool BundleValidationCheckAfterRewrite(const uint8_t *begin,
                                              const uint8_t *end,
                                              uint32_t info,
                                              void *callback_data) {
  struct StubOutCallbackData *data = callback_data;
  UNREFERENCED_PARAMETER(begin);
  UNREFERENCED_PARAMETER(end);

  return AllowErrorDuringBundleValidation(info, data);
}

/*
 * As an extra safety check, when the validator modifies an
 * instruction, we want to revalidate the rewritten code.
 *
 * For performance, we don't want to revalidate the whole code chunk,
 * because that would double the overall validation time.  However,
 * it's not practical to revalidate the individual rewritten
 * instruction, because for x86-64 we need to account for previous
 * instructions that can truncate registers to 32 bits (otherwise we'd
 * get an UNRESTRICTED_INDEX_REGISTER error).
 *
 * Therefore, we revalidate the whole bundle instead.  This means we
 * must apply all rewrites to the bundle first.
 *
 * So, when we hit an instruction that needs rewriting, we recursively
 * invoke the validator two times on the instruction bundle:
 *  * First pass: Rewrite all instructions within the bundle that need
 *    rewriting.
 *  * Second pass: Ensure that the rewritten bundle passes the
 *    validator, with no instructions requiring rewrites.
 */
static Bool RewriteAndRevalidateBundle(const uint8_t *instruction_begin,
                                       const uint8_t *instruction_end,
                                       struct StubOutCallbackData *data) {
  uint8_t *bundle_begin;

  /* Find the start of the bundle that this instruction is part of. */
  CHECK(instruction_begin >= data->chunk_begin);
  bundle_begin = data->chunk_begin + ((instruction_begin - data->chunk_begin)
                                      & ~kBundleMask);
  CHECK(instruction_end <= bundle_begin + kBundleSize);
  CHECK(bundle_begin + kBundleSize <= data->chunk_end);

  /* First pass: Rewrite instructions within bundle that need rewriting. */
  if (!data->validate_chunk_func(bundle_begin, kBundleSize, 0 /*options*/,
                                 data->cpu_features,
                                 BundleValidationApplyRewrite,
                                 data)) {
    return FALSE;
  }

  /* Second pass: Revalidate the bundle. */
  return data->validate_chunk_func(bundle_begin, kBundleSize, 0 /*options*/,
                                   data->cpu_features,
                                   BundleValidationCheckAfterRewrite,
                                   data);
}

Bool NaClDfaStubOutUnsupportedInstruction(const uint8_t *begin,
                                          const uint8_t *end,
                                          uint32_t info,
                                          void *callback_data) {
  struct StubOutCallbackData *data = callback_data;
  /* Stub-out instructions unsupported on this CPU, but valid on other CPUs.  */
  if ((info & VALIDATION_ERRORS_MASK) == CPUID_UNSUPPORTED_INSTRUCTION) {
    return RewriteAndRevalidateBundle(begin, end, data);
  } else if ((info & VALIDATION_ERRORS_MASK) == UNSUPPORTED_INSTRUCTION) {
    if (data->flags & NACL_DISABLE_NONTEMPORALS_X86) {
      return FALSE;
    } else {
      /* TODO(ruiq): rewrite instruction. For now, we keep the original
       * instruction and indicate validation success, which is consistent
       * with current validation results. */
      return TRUE;
    }
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
