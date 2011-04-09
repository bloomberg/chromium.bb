/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_PPAPI_H_
#define NATIVE_CLIENT_SRC_UNTRUSTED_IRT_IRT_PPAPI_H_ 1

#include "ppapi/c/ppp.h"

struct PP_StartFunctions {
  int32_t (*PPP_InitializeModule)(PP_Module module_id,
                                  PPB_GetInterface get_browser_interface);
  void (*PPP_ShutdownModule)();
  const void *(*PPP_GetInterface)(const char *interface_name);
};

typedef void (*PP_StartFunc)(const struct PP_StartFunctions *funcs);

#endif
