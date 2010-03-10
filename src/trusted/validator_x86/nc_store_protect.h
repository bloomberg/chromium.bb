/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_STORE_PROTECT_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_STORE_PROTECT_H__

/* nc_store_protect.h - For 64-bit mode, verifies that we don't store out
 * of range.
 */

/*
 * Note: The function NcStoreValidator is used as a validator
 * function to be applied to a validated segment, as defined in
 * ncvalidate_iter.h.
 */

struct NaClValidatorState;
struct NaClInstIter;

/*
 * Verifies that we don't store out of range. That implies that when storing
 * in a memory offset of the form:
 *
 *     [base + index * scale + displacement]
 *
 * (1) base is either the reserved base register (r15), or rsp, or rbp.
 *
 * (2) Either the index isn't defined, or the index is a 32-bit register and
 *     the previous instruction must assign a value to index that is 32-bits
 *     with zero extension.
 *
 * (3) The displacement can't be larger than 32 bits.
 *
 * SPECIAL CASE: We allow all stores of the form [%rip + displacement].
 *
 * NOTE: in x86 code, displacements can't be larger than 32 bits.
 */
void NaClStoreValidator(struct NaClValidatorState* state,
                        struct NaClInstIter* iter,
                        void* ignore);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NC_STORE_PROTECT_H__ */
