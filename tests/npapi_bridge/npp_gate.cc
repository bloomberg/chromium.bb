/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <stdio.h>
#include <string.h>
#include <new>
#include <nacl/npupp.h>
#include "native_client/tests/npapi_bridge/plugin.h"

// Please refer to the Gecko Plugin API Reference for the description of
// NPP_New.
NPError NPP_New(NPMIMEType mime_type,
                NPP instance,
                uint16_t mode,
                int16_t argc,
                char* argn[],
                char* argv[],
                NPSavedData* saved) {
  printf("NPP_New: instance %p\n", reinterpret_cast<void*>(instance));

  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  bool cycle = false;
  const char* canvas = "unknown";
  for (uint16_t i = 0; i < argc; ++i) {
    printf("%u: '%s' '%s'\n", i, argn[i], argv[i]);
    if (strcmp(argn[i], "canvas") == 0) {
      canvas = argv[i];
      printf("CANVAS: %s\n", canvas);
    } else if ((strcmp(argn[i], "cycle") == 0) &&
               (strcmp(argv[i], "auto") == 0)) {
      cycle = true;
      printf("CYCLE: %s\n", cycle ? "true" : "false");
    }
  }

  Plugin* plugin = new(std::nothrow) Plugin(instance, canvas, cycle);
  if (plugin == NULL) {
    return NPERR_OUT_OF_MEMORY_ERROR;
  }

  instance->pdata = plugin;
  return NPERR_NO_ERROR;
}

// Please refer to the Gecko Plugin API Reference for the description of
// NPP_Destroy.
NPError NPP_Destroy(NPP instance, NPSavedData** save) {
  printf("NPP_Destroy\n");

  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  Plugin* plugin = static_cast<Plugin*>(instance->pdata);
  if (plugin != NULL) {
    delete plugin;
  }
  return NPERR_NO_ERROR;
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value) {
  if (NPPVpluginScriptableNPObject == variable) {
    if (instance == NULL) {
      return NPERR_INVALID_INSTANCE_ERROR;
    }
    NPObject* object = NULL;
    Plugin* plugin = static_cast<Plugin*>(instance->pdata);
    if (plugin != NULL) {
      object = plugin->GetScriptableObject();
    }
    *reinterpret_cast<NPObject**>(value) = object;
    return NPERR_NO_ERROR;
  }
  return NPERR_INVALID_PARAM;
}

int16_t NPP_HandleEvent(NPP instance, void* event) {
  return 0;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window) {
  printf("NPP_SetWindow\n");
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

extern "C" NPError NP_Initialize(NPNetscapeFuncs* browser_funcs,
                                 NPPluginFuncs* plugin_funcs) {
  plugin_funcs->newp = NPP_New;
  plugin_funcs->destroy = NPP_Destroy;
  plugin_funcs->setwindow = NPP_SetWindow;
  plugin_funcs->event = NPP_HandleEvent;
  plugin_funcs->getvalue = NPP_GetValue;
  return NPERR_NO_ERROR;
}
