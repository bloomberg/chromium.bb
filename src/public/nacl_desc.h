/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_PUBLIC_NACL_DESC_H_
#define NATIVE_CLIENT_SRC_PUBLIC_NACL_DESC_H_ 1

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/public/imc_types.h"

EXTERN_C_BEGIN

struct NaClDesc;

/*
 * Create a NaClDesc for a NaClHandle which has reliable identity information.
 * That identity can be used for future validation caching.
 *
 * If the file_path string is empty, this returns a NaClDesc that is not marked
 * as validation-cacheable.
 *
 * On success, returns a new read-only NaClDesc that uses the passed handle,
 * setting file path information internally.
 * On failure, returns NULL.
 */
struct NaClDesc *NaClDescCreateWithFilePathMetadata(NaClHandle handle,
                                                    const char *file_path);

EXTERN_C_END

#endif
