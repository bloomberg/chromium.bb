/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <stdio.h>
#include <string.h>
#include <nacl/npupp.h>
#include <new>
#include "native_client/tests/npapi_pi/plugin.h"

// Please refer to the Gecko Plugin API Reference for the description of
// NPP_New.
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

  Plugin* plugin = new(std::nothrow) Plugin(instance);
  if (plugin == NULL) {
    return NPERR_OUT_OF_MEMORY_ERROR;
  }

  instance->pdata = plugin;
  return NPERR_NO_ERROR;
}

// Please refer to the Gecko Plugin API Reference for the description of
// NPP_Destroy.
// In the NaCl module, NPP_Destroy is called from NaClNP_MainLoop().
NPError NPP_Destroy(NPP instance, NPSavedData** save) {
  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  Plugin* plugin = static_cast<Plugin*>(instance->pdata);
  if (plugin != NULL) {
    delete plugin;
  }
  return NPERR_NO_ERROR;
}

// NPP_GetScriptableInstance retruns the NPObject pointer that corresponds to
// NPPVpluginScriptableNPObject queried by NPP_GetValue() from the browser.
NPObject* NPP_GetScriptableInstance(NPP instance) {
  if (instance == NULL) {
    return NULL;
  }

  NPObject* object = NULL;
  Plugin* plugin = static_cast<Plugin*>(instance->pdata);
  if (plugin) {
    object = plugin->GetScriptableObject();
  }
  return object;
}

int16_t NPP_HandleEvent(NPP instance, void* event) {
  return 0;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window) {
  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }
  if (window == NULL) {
    return NPERR_GENERIC_ERROR;
  }
  Plugin* plugin = static_cast<Plugin*>(instance->pdata);
  if (plugin != NULL) {
    return plugin->SetWindow(window);
  }
  return NPERR_GENERIC_ERROR;
}

NPError NP_Initialize(NPNetscapeFuncs* browser_funcs,
                      NPPluginFuncs* plugin_funcs) {
  plugin_funcs->newp = NPP_New;
  plugin_funcs->destroy = NPP_Destroy;
  plugin_funcs->setwindow = NPP_SetWindow;
  plugin_funcs->event = NPP_HandleEvent;
  return NPERR_NO_ERROR;
}
