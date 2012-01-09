/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Define postcondition testers for testing the validator. */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_REG_SFI_NC_POSTCONDS_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_REG_SFI_NC_POSTCONDS_H__

#ifdef NCVAL_TESTING

struct NaClInstIter;
struct NaClValidatorState;

/* Checks if the postcondition "ZeroExtends(reg)" is generated for the
 * current instruction. If so, a corresponding postcondition is added
 * to the instruction.
 */
void NaClAddAssignsRegisterWithZeroExtendsPostconds(
    struct NaClValidatorState* state);

/* Checks if the postcondition "SafeAddress(reg)" is generated for the
 * current instruction. If so, a corresponding postcondition is
 * added to the instruction.
 */
void NaClAddLeaSafeAddressPostconds(
    struct NaClValidatorState* state);

#endif

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVAL_REG_SFI_NC_POSTCONDS_H__ */
