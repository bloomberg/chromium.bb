/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_MINIDUMP_GENERATOR_BUILD_ID_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_MINIDUMP_GENERATOR_BUILD_ID_H_ 1

#include <stdlib.h>

#include "native_client/src/include/nacl_base.h"

EXTERN_C_BEGIN

/*
 * Get the build ID for the nexe.  This is a byte string that uniquely
 * identifies the executable, typically a hash of its contents.
 *
 * On success, this returns 1 and fills out *data and *size.  If the
 * build ID cannot be found, this returns 0.
 */
int nacl_get_build_id(const char **data, size_t *size);

EXTERN_C_END

#endif
