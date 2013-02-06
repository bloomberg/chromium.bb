/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Implement the Validator API for the x86-32 architecture. */
#include <errno.h>
#include <string.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/validator_ragel/bitmap.h"
#include "native_client/src/trusted/validator_ragel/unreviewed/dfa_validate_common.h"
#include "native_client/src/trusted/validator_ragel/validator.h"

/* Be sure the correct compile flags are defined for this. */
#if NACL_ARCH(NACL_TARGET_ARCH) != NACL_x86
# error("Can't compile, target is for x86-32")
#else
# if NACL_TARGET_SUBARCH != 32
#  error("Can't compile, target is for x86-32")
# endif
#endif

NaClValidationStatus ApplyDfaValidator_x86_32(
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
  if (ValidateChunkIA32(data, size, 0 /*options*/, cpu_features,
                        readonly_text ?
                          ProcessError :
                          StubOutCPUUnsupportedInstruction,
                        &status))
    return NaClValidationSucceeded;
  if (errno == ENOMEM)
    return NaClValidationFailedOutOfMemory;
  return status;
}


static NaClValidationStatus ValidatorCopy_x86_32(
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
  callback_data.delta = data_existing - data_new;
  if (ValidateChunkIA32(data_new, size, CALL_USER_CALLBACK_ON_EACH_INSTRUCTION,
                        cpu_features, ProcessCodeCopyInstruction,
                        &callback_data))
    return NaClValidationSucceeded;
  if (errno == ENOMEM)
    return NaClValidationFailedOutOfMemory;
  return NaClValidationFailed;
}

/* This structure is used by callbacks ProcessCodeReplacementInstruction
   and ProcessOriginalCodeInstruction during code replacement validation
   in ValidatorCodeReplacement_x86_32.  */
struct CodeReplacementCallbackData {
  /* Bitmap with boundaries of the instructions in the old code bundle.  */
  bitmap_word instruction_boundaries_existing;
  /* Bitmap with boundaries of the instructions in the new code bundle.  */
  bitmap_word instruction_boundaries_new;
  /* cpu_features - needed for the call to ValidateChunkIA32.  */
  const NaClCPUFeaturesX86 *cpu_features;
  /* Pointer to the start of the current bundle in the old code.  */
  const uint8_t *bundle_existing;
  /* Pointer to the start of the new code.  */
  const uint8_t *data_new;
  /* Difference between addresses: data_existing - data_new. This is needed for
     fast comparison between existing and new code.  */
  ptrdiff_t delta;
};

static INLINE Bool ProcessCodeReplacementInstructionInfoFlags(
    const uint8_t *begin_existing,
    const uint8_t *begin_new,
    size_t instruction_length,
    uint32_t info,
    struct CodeReplacementCallbackData *data) {

  /* Unsupported instruction must have been replaced with HLTs.  */
  if ((info & VALIDATION_ERRORS_MASK) == CPUID_UNSUPPORTED_INSTRUCTION) {
    /* This instruction is replaced with a bunch of HLTs and they are
       single-byte instructions.  Mark all the bytes as instruction
       bundaries.  */
    if (CodeReplacementIsStubouted(begin_existing, instruction_length)) {
      BitmapSetBits(&(data->instruction_boundaries_new),
                    (begin_new - data->data_new) & kBundleMask,
                    instruction_length);
      return TRUE;
    }
    return FALSE;
  }

  /* If we have jump which jumps out of it's range...  */
  if (info & DIRECT_JUMP_OUT_OF_RANGE) {
    /* then everything is fine if it's the only error and jump is unchanged!  */
    if ((info & VALIDATION_ERRORS_MASK) == DIRECT_JUMP_OUT_OF_RANGE &&
        memcmp(begin_new, begin_existing, instruction_length) == 0)
      return TRUE;
    return FALSE;
  }

  /* If instruction is not accepted then we have nothing to do here.  */
  if (info & (VALIDATION_ERRORS_MASK | BAD_JUMP_TARGET))
    return FALSE;

  /* Instruction is untouched: we are done.  */
  if (memcmp(begin_new, begin_existing, instruction_length) == 0)
    return TRUE;

  /*
   * Return failure if code replacement attempts to modify a special instruction
   * such as naclcall or nacljmp.
   */
  if (info & SPECIAL_INSTRUCTION)
    return FALSE;

  /* No problems found.  */
  return TRUE;
}

static Bool ProcessOriginalCodeInstruction(const uint8_t *begin_existing,
                                           const uint8_t *end_existing,
                                           uint32_t info,
                                           void *callback_data) {
  struct CodeReplacementCallbackData *data = callback_data;

  /* Sanity check: instruction must be shorter then 15 bytes.  */
  CHECK(end_existing - begin_existing < MAX_INSTRUCTION_LENGTH);

  /* Sanity check: old code must be valid... except for jumps.  */
  CHECK(!(info & (VALIDATION_ERRORS_MASK & ~DIRECT_JUMP_OUT_OF_RANGE)));

  /* Don't mask the end of bundle: may lead to overflow  */
  if (end_existing - data->bundle_existing == kBundleMask)
    return TRUE;

  BitmapSetBit(&data->instruction_boundaries_existing,
               (end_existing + 1) - data->bundle_existing);
  return TRUE;
}

static Bool ProcessCodeReplacementInstruction(const uint8_t *begin_new,
                                              const uint8_t *end_new,
                                              uint32_t info,
                                              void *callback_data) {
  struct CodeReplacementCallbackData *data = callback_data;
  size_t instruction_length = end_new - begin_new + 1;
  const uint8_t *begin_existing = begin_new + data->delta;

  /* Sanity check: instruction must be shorter then 15 bytes.  */
  CHECK(instruction_length <= MAX_INSTRUCTION_LENGTH);

  if (ProcessCodeReplacementInstructionInfoFlags(begin_existing, begin_new,
                                                 instruction_length,
                                                 info, data)) {
    /* If we are in the middle of a bundle then collect bondaries.  */
    if (((end_new - data->data_new) & kBundleMask) != kBundleMask) {
      /* We mark the end of the instruction as valid jump target here.  */
      BitmapSetBit(&(data->instruction_boundaries_new),
                   ((end_new + 1) - data->data_new) & kBundleMask);
    /* If we at the end of a bundle then check bondaries.  */
    } else {
      Bool rc;
      data->bundle_existing = end_new - kBundleMask + data->delta;

      /* If old piece of code is not valid then something is VERY wrong.  */
      rc = ValidateChunkIA32(data->bundle_existing, kBundleSize,
                             CALL_USER_CALLBACK_ON_EACH_INSTRUCTION,
                             data->cpu_features, ProcessOriginalCodeInstruction,
                             callback_data);
      CHECK(rc);

      if (data->instruction_boundaries_existing !=
          data->instruction_boundaries_new)
        return FALSE;
      /* Pre-mark first boundaries. */
      data->instruction_boundaries_existing = 1;
      data->instruction_boundaries_new = 1;
    }

    return TRUE;
  }

  return FALSE;
}

static NaClValidationStatus ValidatorCodeReplacement_x86_32(
    uintptr_t guest_addr,
    uint8_t *data_existing,
    uint8_t *data_new,
    size_t size,
    const NaClCPUFeatures *f) {
  /* TODO(jfb) Use a safe cast here. */
  NaClCPUFeaturesX86 *cpu_features = (NaClCPUFeaturesX86 *) f;
  struct CodeReplacementCallbackData callback_data;
  UNREFERENCED_PARAMETER(guest_addr);

  if (size & kBundleMask)
    return NaClValidationFailed;
   /* Pre-mark first boundaries. */
  callback_data.instruction_boundaries_existing = 1;
  callback_data.instruction_boundaries_new = 1;
  callback_data.cpu_features = cpu_features;
  /* Note: bundle_existing is used when we call second validator.  */
  callback_data.data_new = data_new;
  callback_data.delta = data_existing - data_new;
  if (ValidateChunkIA32(data_new, size, CALL_USER_CALLBACK_ON_EACH_INSTRUCTION,
                        cpu_features, ProcessCodeReplacementInstruction,
                        &callback_data))
    return NaClValidationSucceeded;
  if (errno == ENOMEM)
    return NaClValidationFailedOutOfMemory;
  return NaClValidationFailed;
}

static const struct NaClValidatorInterface validator = {
  ApplyDfaValidator_x86_32,
  ValidatorCopy_x86_32,
  ValidatorCodeReplacement_x86_32,
  sizeof(NaClCPUFeaturesX86),
  NaClSetAllCPUFeaturesX86,
  NaClGetCurrentCPUFeaturesX86,
  NaClFixCPUFeaturesX86,
};

const struct NaClValidatorInterface *NaClDfaValidatorCreate_x86_32(void) {
  return &validator;
}
