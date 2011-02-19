/*
  Copyright (c) 2011 The Native Client Authors. All rights reserved.
  Use of this source code is governed by a BSD-style license that can be
  found in the LICENSE file.
*/

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_PPAPI_NACL_FILE_H
#define NATIVE_CLIENT_SRC_UNTRUSTED_PPAPI_NACL_FILE_H

#include "ppapi/c/pp_instance.h"

// TODO(nfullagar, polina): enable extern "C"
//#ifdef __cplusplus
//extern "C" {
//#endif  /* __cplusplus */

// Returns PP_ERROR.
int32_t LoadUrl(PP_Instance instance, const char* url);

// TODO(nfullagar,polina): implement POSIX-like, thread-safe, open, close, read.

//#ifdef __cplusplus
//}
//#endif  /* __cplusplus */

#endif  /*  NATIVE_CLIENT_SRC_UNTRUSTED_PPAPI_NACL_FILE_H */
