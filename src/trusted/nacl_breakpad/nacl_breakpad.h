/*
 * Copyright 2010 The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_NACL_BREAKPAD_NACL_BREAKPAD_H_
#define NATIVE_CLIENT_SRC_TRUSTED_NACL_BREAKPAD_NACL_BREAKPAD_H_ 1

#include "native_client/src/include/nacl_base.h"

EXTERN_C_BEGIN

/*
 * Initialize the breakpad crash handler
 */
void NaClBreakpadInit();

/*
 * Terminate and clean up the breakpad library
 */
void NaClBreakpadFini();

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_NACL_BREAKPAD_NACL_BREAKPAD_H_ */

