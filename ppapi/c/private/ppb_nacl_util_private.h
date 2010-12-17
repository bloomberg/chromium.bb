// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_PRIVATE_PPB_NACL_UTIL_PRIVATE_H_
#define PPAPI_C_PRIVATE_PPB_NACL_UTIL_PRIVATE_H_

#include "ppapi/c/pp_resource.h"
#include "ppapi/c/pp_stdint.h"

#define PPB_NACLUTIL_PRIVATE_INTERFACE "PPB_NaClUtil(Private);0.1"

struct PPB_NaClUtil_Private {
  int32_t (*LaunchSelLdr)(PP_Resource file_io);
};

#endif  // PPAPI_C_PRIVATE_PPB_NACL_UTIL_PRIVATE_H_
