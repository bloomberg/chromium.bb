/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Routines for checking address-sandboxing masks.
 */

#ifndef NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_MASKS_H__
#define NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_MASKS_H__

#include "native_client/src/trusted/validator_arm/ncdecode.h"

/*
 * Verifies that state's current instruction produces a correctly masked
 * data-region address in register 'reg' when 'condition' is true.
 *
 * This function always checks the condition, because we have no use case yet
 * where it shouldn't.
 */
bool CheckDataMask(const NcDecodeState &state, int32_t reg, int32_t condition);

/*
 * Verifies that state's current instruction produces a correctly masked
 * indirect branch target in register 'reg' when 'condition' is true.
 *
 * Because we allow a single-instruction atomic mask-branch with bic, some
 * uses don't require that 'condition' be checked.  For these cases, pass
 * kIgnoreCondition.
 */
bool CheckControlMask(const NcDecodeState &state, int32_t reg,
    int32_t condition);
static const int kIgnoreCondition = -1;

#endif  // NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_MASKS_H__
