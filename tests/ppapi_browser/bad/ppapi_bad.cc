// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "native_client/src/shared/platform/nacl_check.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_var.h"

#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/dev/ppb_widget_dev.h"

#include "ppapi/c/ppp.h"

PP_EXPORT int32_t PPP_InitializeModule(PP_Module module_id,
                                       PPB_GetInterface get_browser_interface) {
  printf("PPP_InitializeModule\n");

  // Request an unsupported interface.
  CHECK(NULL == get_browser_interface(PPB_WIDGET_DEV_INTERFACE));
  // Request a supported interface with a bad revision number.
  CHECK(NULL == get_browser_interface("PPB_Instance;0.0"));

  return PP_ERROR_FAILED;
}

PP_EXPORT void PPP_ShutdownModule() {
  printf("PPP_ShutdownModule\n");
  fflush(stdout);
}

namespace {

PP_Bool DidCreate(PP_Instance instance,
                  uint32_t argc,
                  const char* argn[],
                  const char* argv[]) {
  return PP_FALSE;
}

void DidDestroy(PP_Instance instance) {
}

void DidChangeView(PP_Instance instance,
                   const struct PP_Rect* position,
                   const struct PP_Rect* clip) {
}

void DidChangeFocus(PP_Instance instance, PP_Bool has_focus) {
}

PP_Bool HandleInputEvent(PP_Instance instance,
                         const struct PP_InputEvent* event) {
  return PP_FALSE;
}

PP_Bool HandleDocumentLoad(PP_Instance instance, PP_Resource url_loader) {
  return PP_FALSE;
}

struct PP_Var GetInstance(PP_Instance instance) {
  return PP_MakeNull();
}

const struct PPP_Instance instance_interface = {
  DidCreate,
  DidDestroy,
  DidChangeView,
  DidChangeFocus,
  HandleInputEvent,
  HandleDocumentLoad,
  GetInstance
};

}  // namespace

PP_EXPORT const void* PPP_GetInterface(const char* interface_name) {
  printf("PPP_GetInterface(%s)\n", interface_name);
  if (0 != std::strcmp(interface_name, PPP_INSTANCE_INTERFACE)) {
    return NULL;
  }
  return reinterpret_cast<const void*>(&instance_interface);
}
