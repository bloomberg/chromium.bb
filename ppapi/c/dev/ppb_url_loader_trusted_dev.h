// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_C_DEV_PPB_URL_LOADER_TRUSTED_DEV_H_
#define PPAPI_C_DEV_PPB_URL_LOADER_TRUSTED_DEV_H_

#include "ppapi/c/pp_resource.h"

#define PPB_URLLOADERTRUSTED_DEV_INTERFACE "PPB_URLLoaderTrusted(Dev);0.1"

// Available only to trusted implementations.
struct PPB_URLLoaderTrusted_Dev {
  // Grant this URLLoader the capability to make unrestricted cross-origin
  // requests.
  void (*GrantUniversalAccess)(PP_Resource loader);
};

#endif  // PPAPI_C_DEV_PPB_URL_LOADER_DEV_H_
