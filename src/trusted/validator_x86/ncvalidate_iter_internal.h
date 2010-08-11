/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_ITER_INTERNAL_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_ITER_INTERNAL_H__

/* Defines the internal data structure for the validator state. */

#include "native_client/src/shared/utils/types.h"
#include "native_client/src/shared/gio/gio.h"
#include "native_client/src/trusted/validator_x86/nacl_cpuid.h"

/* Defines the maximum number of validators that can be registered. */
#define NACL_MAX_NCVALIDATORS 20

struct NaClValidatorState {
  /* Holds the vbase value passed to NaClValidatorStateCreate. */
  NaClPcAddress vbase;
  /* Holds the sz value passed to NaClValidatorStateCreate. */
  NaClMemorySize sz;
  /* Holds the alignment value passed to NaClValidatorStateCreate. */
  uint8_t alignment;
  /* Holds the upper limit of all possible addresses */
  NaClPcAddress vlimit;
  /* Holds the alignment mask, which when applied, catches any lower
   * bits in an address that violate alignment.
   */
  NaClPcAddress alignment_mask;
  /* Holds the value for the base register, or RegUnknown if undefined. */
  NaClOpKind base_register;
  /* Holds if the validation is still valid. */
  Bool validates_ok;
  /* If >= 0, holds how many errors can be reported. If negative,
   * reports all errors.
   */
  int quit_after_error_count;
  /* Holds the local memory associated with validators to be applied to this
   * state.
   */
  void* local_memory[NACL_MAX_NCVALIDATORS];
  /* Defines how many validators are defined for this state. */
  int number_validators;
  /* Holds the cpu features of the machine it is running on. */
  CPUFeatures cpu_features;
};

#endif
  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_ITER_INTERNAL_H__ */
