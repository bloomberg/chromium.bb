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
  plugin = (struct PlugIn *)malloc(sizeof(*plugin));
  plugin->npp = instance;
  plugin->npobject = NULL;

  instance->pdata = plugin;
  return NPERR_NO_ERROR;
}

/*
 * Please refer to the Gecko Plugin API Reference for the description of
 * NPP_Destroy.
 * In the NaCl module, NPP_Destroy is called from NaClNP_MainLoop().
 */
NPError NPP_Destroy(NPP instance, NPSavedData** save) {
  printf("*** NPP_Destroy\n");

  if (NULL == instance)
    return NPERR_INVALID_INSTANCE_ERROR;

  /* free plugin */
  if (NULL != instance->pdata) free(instance->pdata);
  instance->pdata = NULL;
  return NPERR_NO_ERROR;
}

NPObject *NPP_GetScriptableInstance(NPP instance) {
  struct PlugIn* plugin;

  extern NPClass *GetNPSimpleClass();
  printf("*** NPP_GetScriptableInstance\n");

  if (NULL == instance) {
    printf("NULL NPP\n");
    return NULL;
  }
  plugin = (struct PlugIn *)instance->pdata;
  if (NULL == plugin->npobject) {
    printf("Creating the plugin object\n");
    plugin->npobject = NPN_CreateObject(instance, GetNPSimpleClass());
  }
  if (NULL != plugin->npobject) {
    printf("Retaining the plugin object\n");
    NPN_RetainObject(plugin->npobject);
  }
  printf("The plugin object %p\n", (void*) plugin->npobject);
  return plugin->npobject;
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void* ret_value) {
  if (NPPVpluginScriptableNPObject == variable) {
    *((NPObject**) ret_value) = NPP_GetScriptableInstance(instance);
    return NPERR_NO_ERROR;
  } else {
    return NPERR_GENERIC_ERROR;
  }
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
