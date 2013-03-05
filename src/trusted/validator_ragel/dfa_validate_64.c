/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Implement the Validator API for the x86-64 architecture. */
#include <errno.h>
#include <string.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/validator_ragel/dfa_validate_common.h"
#include "native_client/src/trusted/validator_ragel/validator.h"

/*
 * Be sure the correct compile flags are defined for this.
 * TODO(khim): Try to figure out if these checks actually make any sense.
 *             I don't foresee any use in cross-environment, but it should work
 *             and may be useful in some case so why not?
 */
#if NACL_ARCH(NACL_TARGET_ARCH) != NACL_x86 || NACL_TARGET_SUBARCH != 64
# error "Can't compile, target is for x86-64"
#endif


static NaClValidationStatus ApplyDfaValidator_x86_64(
    uintptr_t guest_addr,
    uint8_t *data,
    size_t size,
    int stubout_mode,
    int readonly_text,
    const NaClCPUFeatures *f,
    struct NaClValidationCache *cache) {
  /* TODO(jfb) Use a safe cast here. */
  NaClCPUFeaturesX86 *cpu_features = (NaClCPUFeaturesX86 *) f;
  enum NaClValidationStatus status = NaClValidationFailed;
  UNREFERENCED_PARAMETER(guest_addr);
  UNREFERENCED_PARAMETER(cache);

  if (stubout_mode)
    return NaClValidationFailedNotImplemented;
  if (!NaClArchSupportedX86(cpu_features))
    return NaClValidationFailedCpuNotSupported;
  if (size & kBundleMask)
    return NaClValidationFailed;
  if (ValidateChunkAMD64(data, size, 0 /*options*/, cpu_features,
                         readonly_text ?
                           NaClDfaProcessValidationError :
                           NaClDfaStubOutCPUUnsupportedInstruction,
                         &status))
    return NaClValidationSucceeded;
  if (errno == ENOMEM)
    return NaClValidationFailedOutOfMemory;
  return status;
}


static NaClValidationStatus ValidatorCodeCopy_x86_64(
    uintptr_t guest_addr,
    uint8_t *data_existing,
    uint8_t *data_new,
    size_t size,
    const NaClCPUFeatures *f,
    NaClCopyInstructionFunc copy_func) {
  /* TODO(jfb) Use a safe cast here. */
  NaClCPUFeaturesX86 *cpu_features = (NaClCPUFeaturesX86 *) f;
  struct CodeCopyCallbackData callback_data;
  UNREFERENCED_PARAMETER(guest_addr);

  if (size & kBundleMask)
    return NaClValidationFailed;
  callback_data.copy_func = copy_func;
  callback_data.existing_minus_new = data_existing - data_new;
  if (ValidateChunkAMD64(data_new, size, CALL_USER_CALLBACK_ON_EACH_INSTRUCTION,
                         cpu_features, NaClDfaProcessCodeCopyInstruction,
                         &callback_data))
    return NaClValidationSucceeded;
  if (errno == ENOMEM)
    return NaClValidationFailedOutOfMemory;
  return NaClValidationFailed;
}


static Bool ProcessCodeReplacementInstruction(const uint8_t *begin_new,
                                              const uint8_t *end_new,
                                              uint32_t info_new,
                                              void *callback_data) {
  ptrdiff_t existing_minus_new = (ptrdiff_t)callback_data;
  /* TODO(khim): change ABI to pass next_existing instead.  */
  const uint8_t *next_new = end_new + 1;
  size_t instruction_length = next_new - begin_new;
  const uint8_t *begin_existing = begin_new + existing_minus_new;
  const uint8_t *next_existing = next_new + existing_minus_new;

  /* Sanity check: instruction must be no longer than 17 bytes.  */
  CHECK(instruction_length <= MAX_INSTRUCTION_LENGTH);

  /* Unsupported instruction must have been replaced with HLTs.  */
  if ((info_new & VALIDATION_ERRORS_MASK) == CPUID_UNSUPPORTED_INSTRUCTION)
    return NaClDfaCodeReplacementIsStubouted(begin_existing,
                                             instruction_length);

  /* If we have jump which jumps out of it's range...  */
  if (info_new & DIRECT_JUMP_OUT_OF_RANGE) {
    /* then everything is fine if it's the only error and jump is unchanged!  */
    if ((info_new & VALIDATION_ERRORS_MASK) == DIRECT_JUMP_OUT_OF_RANGE &&
        memcmp(begin_new, begin_existing, instruction_length) == 0)
      return TRUE;
    return FALSE;
  }

  /* If instruction is not accepted then we have nothing to do here.  */
  if (info_new & (VALIDATION_ERRORS_MASK | BAD_JUMP_TARGET))
    return FALSE;

  /* Instruction is untouched: we are done.  */
  if (memcmp(begin_new, begin_existing, instruction_length) == 0)
    return TRUE;

  /* Only some instructions can be modified.  */
  if (!(info_new & MODIFIABLE_INSTRUCTION))
    return FALSE;

  /*
   * In order to understand where the following three cases came from you need
   * to understand that there are three kinds of instructions:
   *  - "normal" x86 instructions
   *  - x86 instructions which [ab]use "immediate" field in the instructions
   *     - 3DNow! x86 instructions use it as an opcode extentions
   *     - four-operand instructions encode fourth register in it
   *  - five-operands instructions encode 2bit immediate and fourth register
   *
   * For the last two cases we need to keep either the whole last byte or at
   * least non-immediate part of it intact.
   *
   * See Figure 1-1 "Instruction Encoding Syntax" in AMDs third volume
   * "General-Purpose and System Instructions" for the information about
   * "normal" x86 instructions and 3DNow! instructions.
   *
   * See Figure 1-1 "Typical Descriptive Synopsis - Extended SSE Instructions"
   * in AMDs fourth volume "128-Bit and 256-Bit Media Instructions" and the
   * "Immediate Byte Usage Unique to the SSE instructions" for the information
   * about four-operand and five-operand instructions.
   */

  /*
   * Instruction with two-bit immediate can only change these two bits and
   * immediate/displacement.
   */
  if ((info_new & IMMEDIATE_2BIT) == IMMEDIATE_2BIT)
    return memcmp(begin_new, begin_existing,
                  instruction_length -
                  INFO_ANYFIELDS_SIZE(info_new) - 1) == 0 &&
           (next_new[-1] & 0xfc) == (next_existing[-1] & 0xfc);

  /* Instruction's last byte is not immediate, thus it must be unchanged.  */
  if (info_new & LAST_BYTE_IS_NOT_IMMEDIATE)
    return memcmp(begin_new, begin_existing,
                  instruction_length -
                  INFO_ANYFIELDS_SIZE(info_new) - 1) == 0 &&
           next_new[-1] == next_existing[-1];

  /*
   * Normal instruction can only change an anyfied: immediate, displacement or
   * relative offset.
   */
  return memcmp(begin_new, begin_existing,
                instruction_length - INFO_ANYFIELDS_SIZE(info_new)) == 0;
}

static NaClValidationStatus ValidatorCodeReplacement_x86_64(
    uintptr_t guest_addr,
    uint8_t *data_existing,
    uint8_t *data_new,
    size_t size,
    const NaClCPUFeatures *f) {
  /* TODO(jfb) Use a safe cast here. */
  NaClCPUFeaturesX86 *cpu_features = (NaClCPUFeaturesX86 *) f;
  UNREFERENCED_PARAMETER(guest_addr);

  if (size & kBundleMask)
    return NaClValidationFailed;
  if (ValidateChunkAMD64(data_new, size, CALL_USER_CALLBACK_ON_EACH_INSTRUCTION,
                         cpu_features, ProcessCodeReplacementInstruction,
                         (void *)(data_existing - data_new)))
    return NaClValidationSucceeded;
  if (errno == ENOMEM)
    return NaClValidationFailedOutOfMemory;
  return NaClValidationFailed;
}

static const struct NaClValidatorInterface validator = {
  ApplyDfaValidator_x86_64,
  ValidatorCodeCopy_x86_64,
  ValidatorCodeReplacement_x86_64,
  sizeof(NaClCPUFeaturesX86),
  NaClSetAllCPUFeaturesX86,
  NaClGetCurrentCPUFeaturesX86,
  NaClFixCPUFeaturesX86,
};

const struct NaClValidatorInterface *NaClDfaValidatorCreate_x86_64(void) {
  return &validator;
}
