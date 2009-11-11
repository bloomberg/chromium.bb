// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/gpu_plugin/gpu_plugin.h"
#include "gpu/gpu_plugin/gpu_plugin_object_factory.h"
#include "gpu/np_utils/np_browser.h"
#include "gpu/np_utils/np_plugin_object.h"
#include "gpu/np_utils/np_plugin_object_factory.h"

#if defined(O3D_IN_CHROME)
#include "webkit/glue/plugins/nphostapi.h"
#else
#include "o3d/third_party/npapi/include/npfunctions.h"
#endif

namespace gpu_plugin {

// Definitions of NPAPI plugin entry points.

namespace {
NPBrowser* g_browser;
GPUPluginObjectFactory g_plugin_object_factory;

NPError NPP_New(NPMIMEType plugin_type, NPP instance,
                uint16 mode, int16 argc, char* argn[],
                char* argv[], NPSavedData* saved) {
  if (!instance)
    return NPERR_INVALID_INSTANCE_ERROR;

  PluginObject* plugin_object =
      NPPluginObjectFactory::get()->CreatePluginObject(instance, plugin_type);
  if (!plugin_object)
    return NPERR_GENERIC_ERROR;

  instance->pdata = plugin_object;

  NPError error = plugin_object->New(plugin_type, argc, argn, argv, saved);
  if (error != NPERR_NO_ERROR) {
    plugin_object->Release();
  }

  return error;
}

NPError NPP_Destroy(NPP instance, NPSavedData** saved) {
  if (!instance)
    return NPERR_INVALID_INSTANCE_ERROR;

  PluginObject* plugin_object = static_cast<PluginObject*>(instance->pdata);
  NPError error = plugin_object->Destroy(saved);

  if (error == NPERR_NO_ERROR) {
    plugin_object->Release();
  }

  return error;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window) {
  if (!instance)
    return NPERR_INVALID_INSTANCE_ERROR;

  PluginObject* plugin_object = static_cast<PluginObject*>(instance->pdata);
  return plugin_object->SetWindow(window);
}

int16 NPP_HandleEvent(NPP instance, void* event) {
  if (!instance)
    return 0;

  PluginObject* plugin_object = static_cast<PluginObject*>(instance->pdata);
  return plugin_object->HandleEvent(static_cast<NPEvent*>(event));
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value) {
  if (!instance)
    return NPERR_INVALID_INSTANCE_ERROR;

  PluginObject* plugin_object = static_cast<PluginObject*>(instance->pdata);
  switch (variable) {
    case NPPVpluginScriptableNPObject:
      *reinterpret_cast<NPObject**>(value) =
          plugin_object->GetScriptableNPObject();
      return NPERR_NO_ERROR;
    default:
      return NPERR_GENERIC_ERROR;
  }
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value) {
  return NPERR_NO_ERROR;
}
}

NPError NP_GetEntryPoints(NPPluginFuncs* funcs) {
  funcs->newp = NPP_New;
  funcs->destroy = NPP_Destroy;
  funcs->setwindow = NPP_SetWindow;
  funcs->event = NPP_HandleEvent;
  funcs->getvalue = NPP_GetValue;
  funcs->setvalue = NPP_SetValue;
  return NPERR_NO_ERROR;
}

#if defined(OS_LINUX)
NPError API_CALL NP_Initialize(NPNetscapeFuncs *browser_funcs,
                               NPPluginFuncs* plugin_funcs) {
#else
NPError NP_Initialize(NPNetscapeFuncs *browser_funcs) {
#endif
  if (!browser_funcs)
    return NPERR_INVALID_FUNCTABLE_ERROR;

  if (g_browser)
    return NPERR_GENERIC_ERROR;

#if defined(OS_LINUX)
  NP_GetEntryPoints(plugin_funcs);
#endif

  g_browser = new NPBrowser(browser_funcs);

  return NPERR_NO_ERROR;
}

NPError NP_Shutdown() {
  if (!g_browser)
    return NPERR_GENERIC_ERROR;

  delete g_browser;
  g_browser = NULL;

  return NPERR_NO_ERROR;
}
}  // namespace gpu_plugin
