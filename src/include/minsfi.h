/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_INCLUDE_MINSFI_H_
#define NATIVE_CLIENT_SRC_INCLUDE_MINSFI_H_

#include <stdbool.h>
#include <stdint.h>

#include "native_client/src/include/nacl_base.h"

EXTERN_C_BEGIN

/*
 * Allocates a memory region for the sandbox and initializes it. Returns TRUE
 * if it was successful or if the sandbox has already been initialized.
 */
bool MinsfiInitializeSandbox(void);

/*
 * Invokes the entry function of the sandbox and returns the exit value
 * returned by the sandbox. Returns EXIT_FAILURE if sandbox cannot be invoked,
 * e.g. because it has not been initialized.
 */
int MinsfiInvokeSandbox(int argc, char **argv);

/*
 * Destroys the MinSFI address subspace if there is one. Returns FALSE if
 * a subspace exists but could not be destroyed.
 */
bool MinsfiDestroySandbox(void);

EXTERN_C_END

#endif  // NATIVE_CLIENT_SRC_INCLUDE_MINSFI_H_
