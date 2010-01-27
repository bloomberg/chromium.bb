
/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl service runtime.  NaClDescInvalid subclass of NaClDesc.
 * This is a singleton subclass.
 */
#ifndef NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_INVALID_H_
#define NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_INVALID_H_

#include "native_client/src/include/nacl_base.h"
#include "native_client/src/include/portability.h"

EXTERN_C_BEGIN

struct NaClDesc;
struct NaClDescXferState;
struct NaClMessageHeader;

/* Initialize and tear down the state for maintaining the singleton. */
extern void NaClDescInvalidInit();
extern void NaClDescInvalidFini();

/* A factory method for returning the singleton. */
struct NaClDescInvalid const *NaClDescInvalidMake();

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_DESC_NACL_DESC_INVALID_H_ */
