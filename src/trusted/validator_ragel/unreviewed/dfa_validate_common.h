/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This file contains common parts of x86-32 and x86-64 internals (inline
 * and static functions and defines).
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_DFA_VALIDATE_COMMON_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_DFA_VALIDATE_COMMON_H_

#include <stddef.h>

#include "native_client/src/shared/utils/types.h"
#include "native_client/src/trusted/validator/ncvalidate.h"

#define MAX_INSTRUCTION_LENGTH 15

Bool ProcessError(const uint8_t *begin, const uint8_t *end, uint32_t info,
                  void *callback_data);

Bool StubOutCPUUnsupportedInstruction(const uint8_t *begin,
                                      const uint8_t *end,
                                      uint32_t info,
                                      void *callback_data);
struct CodeCopyCallbackData {
  NaClCopyInstructionFunc copy_func;
  /* Difference between addresses: dest - src.  */
  ptrdiff_t delta;
};

Bool ProcessCodeCopyInstruction(const uint8_t *begin_new,
                                const uint8_t *end_new,
                                uint32_t info,
                                void *callback_data);

/* Check whether instruction is stubouted because it is not supported by current
   CPU.  */
Bool CodeReplacementIsStubouted(const uint8_t *begin_existing,
                                size_t instruction_length);

#endif /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_RAGEL_DFA_VALIDATE_COMMON_H_ */
