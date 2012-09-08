/*
 * Copyright (c) 2011 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_PNACL_IRT_SHIM_SHIM_PPAPI_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_PNACL_IRT_SHIM_SHIM_PPAPI_H_ 1

#include <stddef.h>
#include "native_client/src/untrusted/irt/irt.h"

/*
 * Remembers the IRT's true interface query function.
 */
extern TYPE_nacl_irt_query __pnacl_real_irt_interface;

size_t __pnacl_irt_interface_wrapper(const char *interface_ident,
                                     void *table, size_t tablesize);

#endif  /* NATIVE_CLIENT_SRC_UNTRUSTED_PNACL_IRT_SHIM_SHIM_PPAPI_H_ */
