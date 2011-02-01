/*
 * Copyright 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/pp_var.h"

/* Function prototypes */
static PP_Bool DidCreate(PP_Instance instance,
                         uint32_t argc,
                         const char* argn[],
                         const char* argv[]);
static void DidDestroy(PP_Instance instance);
static void DidChangeView(PP_Instance instance,
                          const struct PP_Rect* position,
                          const struct PP_Rect* clip);
static void DidChangeFocus(PP_Instance instance,
                           PP_Bool has_focus);
static PP_Bool HandleInputEvent(PP_Instance instance,
                               const struct PP_InputEvent* event);
static PP_Bool HandleDocumentLoad(PP_Instance instance,
                                  PP_Resource url_loader);
static struct PP_Var GetInstanceObject(PP_Instance instance);
extern struct PP_Var GetScriptableObject(
    PP_Instance instance);


/* Global variables */
extern PPB_GetInterface get_browser_interface_func;
extern PP_Module module;
struct PPP_Instance instance_interface = {
  DidCreate,
  DidDestroy,
  DidChangeView,
  DidChangeFocus,
  HandleInputEvent,
  HandleDocumentLoad,
  GetInstanceObject
};

static PP_Bool DidCreate(PP_Instance instance,
                         uint32_t argc,
                         const char* argn[],
                         const char* argv[]) {
  return PP_TRUE;
}

static void DidDestroy(PP_Instance instance) {
}

static void DidChangeView(PP_Instance instance,
                          const struct PP_Rect* position,
                          const struct PP_Rect* clip) {
}

static void DidChangeFocus(PP_Instance instance,
                           PP_Bool has_focus) {
}

static PP_Bool HandleInputEvent(PP_Instance instance,
                                const struct PP_InputEvent* event) {
  return PP_TRUE;
}

static PP_Bool HandleDocumentLoad(PP_Instance instance,
                                  PP_Resource url_loader) {
  return PP_TRUE;
}

struct PP_Var GetInstanceObject(PP_Instance instance) {
  return GetScriptableObject(instance);
}

int32_t PPP_InitializeModule(PP_Module module_id,
                             PPB_GetInterface get_browser_interface) {
  module = module_id;
  get_browser_interface_func = get_browser_interface;
  return PP_OK;
}

void PPP_ShutdownModule() {
}

const void* PPP_GetInterface(const char* interface_name) {
  if (0 == strncmp(PPP_INSTANCE_INTERFACE, interface_name,
                   strlen(PPP_INSTANCE_INTERFACE))) {
    return &instance_interface;
  }
  return NULL;
}
