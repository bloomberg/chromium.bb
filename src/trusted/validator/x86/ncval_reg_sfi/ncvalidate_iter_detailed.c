/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter_detailed.h"
#include "native_client/src/trusted/validator/x86/ncval_reg_sfi/ncvalidate_iter_internal.h"

NaClValidatorState* NaClValidatorStateCreateDetailed(
    const NaClPcAddress vbase,
    const NaClMemorySize sz,
    const uint8_t alignment,
    const NaClOpKind base_register,
    const CPUFeatures* features) {
  NaClValidatorState* state =
      NaClValidatorStateCreate(vbase, sz, alignment, base_register, features);
  if (state != NULL) {
    state->do_detailed = TRUE;
  }
  return state;
}
