/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * TODO(mcgrathr): This whole file should go away as soon as the
 * LLVM intrinsics are really done right.  See ../nacl/tls_params.h
 * for details.
 */

#ifdef __pnacl__
#undef __pnacl__
#define NACL_UNTRUSTED_INLINE /* empty */
#else
#error "this file is a kludge meant only for the pnacl build"
#endif

#include "native_client/src/untrusted/nacl/tls_params.h"
