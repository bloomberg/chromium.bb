/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdio.h>
#include <string.h>
#include <new>
#include <nacl/npupp.h>
#include "native_client/tests/npapi_bridge/plugin.h"

// Please refer to the Gecko Plugin API Reference for the description of
// NPP_New.
NPError NPP_New(NPMIMEType mime_type, NPP instance, uint16_t mode,
                int16_t argc, char* argn[], char* argv[],
                NPSavedData* saved) {
  printf("NPP_New\n");

  if (instance == NULL) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  const char* canvas = "tutorial";
  for (uint16_t i = 0; i < argc; ++i) {
    printf("%u: '%s' '%s'\n", i, argn[i], argv[i]);
    if (strcmp(argn[i], "canvas") == 0) {
      canvas = argv[i];
      printf("CANVAS: %s\n", canvas);
    }
  }

  Plugin* plugin = new(std::nothrow) Plugin(instance, canvas);
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

// NPP_GetScriptableInstance retruns the NPObject pointer that corresponds to
// NPPVpluginScriptableNPObject queried by NPP_GetValue() from the browser.
NPObject* NPP_GetScriptableInstance(NPP instance) {
  printf("NPP_GetScriptableInstance\n");

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
  return NPERR_NO_ERROR;
}
