/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#ifndef NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_QUALIFY_NACL_OS_QUALIFY_H_
#define NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_QUALIFY_NACL_OS_QUALIFY_H_

#include "native_client/src/include/nacl_base.h"

EXTERN_C_BEGIN

/*
 * Returns 1 if the operating system can run Native Client modules.
 */
int NaClOsIsSupported();

/*
 * Returns 1 if the operating system is a 64-bit version of
 * Windows.  For now, all of these versions are not supported.
 */
int NaClOsIs64BitWindows();

/*
 * Returns 1 if the operating system is a version that restores the
 * local descriptor table of processes.  This determines which 64-bit
 * solution is required to execute Native Client modules.
 */
int NaClOsRestoresLdt();

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_PLATFORM_QUALIFY_NACL_OS_QUALIFY_H_ */
