/*
 * Copyright 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_SHARED_PLATFORM_NACL_EXIT_H_
#define NATIVE_CLIENT_SRC_SHARED_PLATFORM_NACL_EXIT_H_ 1

#include "native_client/src/include/nacl_base.h"

EXTERN_C_BEGIN

/*
 * NaClExit() is for doing a graceful exit, when no internal errors
 * have been detected, when the caller wants to return a well-defined
 * exit status.
 */
void NaClExit(int code);

/*
 * NaClAbort() is for doing a non-graceful exit, when an internal
 * error has been detected and we want to exit as quickly as possible
 * without running any shutdown code that might cause further
 * problems.
 */
void NaClAbort(void);

EXTERN_C_END

#endif /* NATIVE_CLIENT_SRC_SHARED_PLATFORM_NACL_EXIT_H_ */
