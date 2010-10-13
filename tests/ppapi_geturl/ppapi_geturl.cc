// Copyright 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can
// be found in the LICENSE file.

#include <unistd.h>
#include <nacl/nacl_inttypes.h>
#include <ppapi/c/pp_errors.h>
#include <ppapi/c/pp_resource.h>
#include <ppapi/c/pp_var.h>
#include <ppapi/c/ppb.h>
#include <ppapi/c/ppb_instance.h>
#include <ppapi/c/ppp.h>
#include <ppapi/c/ppp_instance.h>
#include <cstdio>
#include <cstdlib>
#include "native_client/tests/ppapi_geturl/module.h"

// Global PPP functions --------------------------------------------------------

PP_EXPORT int32_t PPP_InitializeModule(PP_Module module_id,
                                       PPB_GetInterface get_browser_interface) {
  printf("--- PPP_InitializeModule\n");
  fflush(stdout);
  const void* ppi = get_browser_interface(PPB_INSTANCE_INTERFACE);
  printf("--- get_browser_interface(PPB_INSTANCE_INTERFACE) -> %p\n", ppi);

  Module* module = Module::Create(module_id, get_browser_interface);
  if (NULL == module)
    return PP_ERROR_FAILED;
  return PP_OK;
}

PP_EXPORT void PPP_ShutdownModule() {
  printf("--- PPP_ShutdownModule\n");
  fflush(stdout);
  Module::Free();
}

PP_EXPORT const void* PPP_GetInterface(const char* interface_name) {
  printf("--- PPP_GetInterface\n");
  fflush(stdout);
  Module* module = Module::Get();
  if (NULL == module)
    return NULL;
  return module->GetPluginInterface(interface_name);
}

