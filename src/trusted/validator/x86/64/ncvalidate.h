/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_64_NCVALIDATE_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_64_NCVALIDATE_H_

/* Defines helper functions for implementing the ApplyValidator API
 * for the x86-64 architecture.
 */

/* Runs the validator to stubout instructions in the code segment
 * that may cause validation errors. Note: Not all validation erros
 * are caught by stubout. After code has been stubbed out, it must
 * still be validated to verify that there aren't validation errors.
 * Paramters are:
 *    guest_addr: The pc address to use.
 *    data - The contents of the code segment to be validated.
 *    size - The size of the code segment to be validated.
 *    bundle_size - The number of bytes in a code bundle.
 *    local_cpu: True if local cpu rules should be applied.
 *           Otherwise, assume no cpu specific rules.
 */
NaClValidationStatus NaClApplyValidatorStubout_x86_64(
    uintptr_t guest_addr,
    uint8_t *data,
    size_t size,
    int bundle_size,
    NaClCPUFeaturesX86 *cpu_features);

/* Creates a validator state and initializes it. Returns
 * NaClValidationSucceeded if successful. Otherwise, it returns
 * status describing reason for failure.
 * Paramters are:
 *    guest_addr: The pc address to use.
 *    data - The contents of the code segment to be validated.
 *    size - The size of the code segment to be validated.
 *    bundle_size - The number of bytes in a code bundle.
 *    local_cpu: True if local cpu rules should be applied.
 *           Otherwise, assume no cpu specific rules.
 *    vstate_ptr - Pointer to be set to allocated validator
 *           state if succeeded (NULL otherwise).o
 */
NaClValidationStatus NaClValidatorSetup_x86_64(
    uintptr_t guest_addr,
    size_t size,
    int bundle_size,
    NaClCPUFeaturesX86 *cpu_features,
    struct NaClValidatorState** vstate_ptr);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_64_NCVALIDATE_H_ */
