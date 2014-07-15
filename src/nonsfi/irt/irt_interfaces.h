/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_NONSFI_IRT_IRT_INTERFACES_H_
#define NATIVE_CLIENT_SRC_NONSFI_IRT_IRT_INTERFACES_H_ 1

#include "native_client/src/include/nacl_base.h"

EXTERN_C_BEGIN

typedef void (*nacl_entry_func_t)(void *args);

int nacl_irt_nonsfi_entry(int argc, char **argv, char **environ,
                          nacl_entry_func_t entry_func);

EXTERN_C_END

#endif
