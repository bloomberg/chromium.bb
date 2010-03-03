// Copyright (c) 2010 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <stdlib.h>

#include <nacl/nacl_npapi.h>
#include <nacl/npapi.h>
#include <nacl/npruntime.h>
#include <nacl/npupp.h>
#include "native_client/tests/npapi_runtime/plugin.h"

NPNetscapeFuncs* browser;

// Create a new instance of a plugin.
NPError NPP_New(NPMIMEType mime_type,
                NPP instance,
                uint16_t mode,
                int16_t argc,
                char* argn[],
                char* argv[],
                NPSavedData* saved) {
  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  Plugin *plugin = Plugin::New(instance, mime_type, argc, argn, argv);
  if (NULL == plugin) {
    return NPERR_GENERIC_ERROR;
  }
  instance->pdata = reinterpret_cast<void*>(plugin);
  return NPERR_NO_ERROR;
}

// Destroy an instance of a plugin.
NPError NPP_Destroy(NPP instance,
                    NPSavedData** save) {
  if (NULL == instance) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  delete reinterpret_cast<Plugin*>(instance->pdata);
  return NPERR_NO_ERROR;
}

NPError NPP_GetValue(NPP instance,
                     NPPVariable variable,
                     void* ret_value) {
  if (NPPVpluginScriptableNPObject == variable) {
    if (NULL == instance || NULL == instance->pdata) {
      return NPERR_GENERIC_ERROR;
    }
    *reinterpret_cast<NPObject**>(ret_value) =
        NPN_RetainObject(reinterpret_cast<NPObject*>(instance->pdata));
    return NPERR_NO_ERROR;
  } else {
    return NPERR_GENERIC_ERROR;
  }
}

NPError NPP_SetWindow(NPP instance,
                      NPWindow* window) {
  return NPERR_NO_ERROR;
}

// NP_Initialize
NPError NP_Initialize(NPNetscapeFuncs* browser_funcs,
                      NPPluginFuncs* plugin_funcs) {
  // Imported APIs from the browser.
  browser = browser_funcs;
  // Exported APIs from the plugin.
  plugin_funcs->newp = NPP_New;
  plugin_funcs->destroy = NPP_Destroy;
  plugin_funcs->setwindow = NPP_SetWindow;
  plugin_funcs->getvalue = NPP_GetValue;
  return NPERR_NO_ERROR;
}
