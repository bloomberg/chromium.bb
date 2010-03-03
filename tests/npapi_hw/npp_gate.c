/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include <nacl/nacl_npapi.h>
#include <nacl/npupp.h>

extern NPClass* GetNPSimpleClass();

struct PlugIn {
  NPP npp;
  NPObject *npobject;
};

/*
 * Please refer to the Gecko Plugin API Reference for the description of
 * NPP_New.
 */
NPError NPP_New(NPMIMEType mime_type,
                NPP instance,
                uint16_t mode,
                int16_t argc,
                char* argn[],
                char* argv[],
                NPSavedData* saved) {
  int i;
  struct PlugIn *plugin;

  printf("*** NPP_New\n");
  if (instance == NULL) return NPERR_INVALID_INSTANCE_ERROR;

  for (i = 0; i < argc; ++i) {
    printf("%u: '%s' '%s'\n", i, argn[i], argv[i]);
  }

  /* avoid malloc */
  plugin = (struct PlugIn *) malloc(sizeof(*plugin));
  plugin->npp = instance;
  plugin->npobject = NULL;

  instance->pdata = plugin;
  return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData** save) {
  printf("*** NPP_Destroy\n");

  if (NULL == instance)
    return NPERR_INVALID_INSTANCE_ERROR;

  /* free plugin */
  if (NULL != instance->pdata) free(instance->pdata);
  instance->pdata = NULL;
  return NPERR_NO_ERROR;
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void* ret_value) {
  printf("*** NPP_GetValue\n");
  if (NPPVpluginScriptableNPObject == variable) {
    if (NULL == instance) {
      return NPERR_INVALID_INSTANCE_ERROR;
    }
    struct PlugIn* plugin = (struct PlugIn*) instance->pdata;
    if (NULL == plugin->npobject) {
      plugin->npobject = NPN_CreateObject(instance, GetNPSimpleClass());
    } else {
      NPN_RetainObject(plugin->npobject);
    }
    *((NPObject**) ret_value) = plugin->npobject;
    return NPERR_NO_ERROR;
  }
  return NPERR_INVALID_PARAM;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window) {
  printf("*** NPP_SetWindow\n");
  return NPERR_NO_ERROR;
}

/*
 * NP_Initialize
 */

NPError NP_Initialize(NPNetscapeFuncs* browser_funcs,
                      NPPluginFuncs* plugin_funcs) {
  plugin_funcs->newp = NPP_New;
  plugin_funcs->destroy = NPP_Destroy;
  plugin_funcs->setwindow = NPP_SetWindow;
  plugin_funcs->getvalue = NPP_GetValue;
  return NPERR_NO_ERROR;
}
