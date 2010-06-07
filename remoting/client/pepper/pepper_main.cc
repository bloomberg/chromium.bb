// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/client/pepper/pepper_plugin.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/nphostapi.h"

#if __GNUC__ >= 4
# define EXPORT  __attribute__ ((visibility("default")))
# define PRIVATE __attribute__ ((visibility("hidden")))
#else
# define EXPORT
# define PRIVATE
#endif

//
// External Plugin Implementation
//

// Plugin info.
// These routines are defined externally and provide the code that is specific
// to this particular plugin.

// Initialize general plugin information.
extern void InitializePluginInfo(pepper::PepperPlugin::Info* plugin_info);

// Routine to create the PepperPlugin subclass that implements all of the
// plugin-specific functionality.
extern pepper::PepperPlugin* CreatePlugin(NPNetscapeFuncs* browser_funcs,
                                          NPP instance);

namespace pepper {

//
// Globals
//

// Pointer to struct containing all the callbacks provided by the browser
// to the plugin.
NPNetscapeFuncs* g_browser_funcs = NULL;

// General information (name/description) about this plugin.
PepperPlugin::Info g_plugin_info = { false, NULL, NULL, NULL };


//
// Internal setup routines
//

PRIVATE void Initialize(NPNetscapeFuncs* browser_funcs) {
  g_browser_funcs = browser_funcs;
  if (!g_plugin_info.initialized) {
    InitializePluginInfo(&g_plugin_info);
  }
}

// Populate the NPPluginFuncs struct so that the browser knows how to find
// each entry point for the plugin.
PRIVATE void SetupEntryPoints(NPPluginFuncs* plugin_funcs) {
  plugin_funcs->version       = ((NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR);
  plugin_funcs->size          = sizeof(NPPluginFuncs);
  plugin_funcs->newp          = NPP_New;
  plugin_funcs->destroy       = NPP_Destroy;
  plugin_funcs->setwindow     = NPP_SetWindow;
  plugin_funcs->newstream     = NPP_NewStream;
  plugin_funcs->destroystream = NPP_DestroyStream;
  plugin_funcs->asfile        = NPP_StreamAsFile;
  plugin_funcs->writeready    = NPP_WriteReady;
  plugin_funcs->write         = NPP_Write;
  plugin_funcs->print         = NPP_Print;
  plugin_funcs->event         = NPP_HandleEvent;
  plugin_funcs->urlnotify     = NPP_URLNotify;
  plugin_funcs->javaClass     = NULL;
  plugin_funcs->getvalue      = NPP_GetValue;
  plugin_funcs->setvalue      = NPP_SetValue;
}

// Get the PepperPlugin from the private data storage in the instance.
PRIVATE PepperPlugin* GetPlugin(NPP instance) {
  return static_cast<PepperPlugin*>(instance->pdata);
}

}  // namespace pepper


//
// Exported interfaces
// Routines to initialize/shutdown the plugin.
//

extern "C" {

#if defined(OS_POSIX) && !defined(OS_MACOSX)

// Get the MIME-type associated with this plugin.
// Linux-only. Mac & Windows use a different mechanism for associating a
// MIME-type with the plugin.
// Note that this is called before NPP_Initialize().
EXPORT const char* API_CALL NP_GetMIMEDescription() {
  if (!pepper::g_plugin_info.initialized) {
    InitializePluginInfo(&pepper::g_plugin_info);
  }
  return pepper::g_plugin_info.mime_description;
}

// Old version of NPP_GetValue, required for Linux.
// Simply redirects to the NPP_GetValue.
EXPORT NPError API_CALL NP_GetValue(NPP instance,
                                    NPPVariable variable,
                                    void* value) {
  return NPP_GetValue(instance, variable, value);
}

// NP_Initialize for Linux.
// This is the equivalent of NP_Initialize and NP_GetEntryPoints for Mac/Win.
EXPORT NPError API_CALL NP_Initialize(NPNetscapeFuncs* browser_funcs,
                                      NPPluginFuncs* plugin_funcs) {
  pepper::Initialize(browser_funcs);
  pepper::SetupEntryPoints(plugin_funcs);
  return NPERR_NO_ERROR;
}

#else

// NP_Initialize for Mac/Windows.
EXPORT NPError API_CALL NP_Initialize(NPNetscapeFuncs* browser_funcs) {
  pepper::Initialize(browser_funcs);
  return NPERR_NO_ERROR;
}

// NP_GetEntryPoints for Mac/Windows.
EXPORT NPError API_CALL NP_GetEntryPoints(NPPluginFuncs* plugin_funcs) {
  pepper::SetupEntryPoints(plugin_funcs);
  return NPERR_NO_ERROR;
}

#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)

EXPORT NPError API_CALL NP_Shutdown() {
  pepper::g_browser_funcs = NULL;
  return NPERR_NO_ERROR;
}

}  // extern "C"


//
// Plugin Entrypoints
// Entry points that the plugin makes available to the browser.
//

EXPORT NPError NPP_New(NPMIMEType pluginType,
                       NPP instance,
                       uint16 mode,
                       int16 argc,
                       char* argn[],
                       char* argv[],
                       NPSavedData* saved) {
  if (!instance) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  pepper::PepperPlugin* plugin
      = CreatePlugin(pepper::g_browser_funcs, instance);
  NPError result = plugin->New(pluginType, argc, argn, argv);
  if (result != NPERR_NO_ERROR) {
    delete plugin;
    return result;
  }

  instance->pdata = plugin;
  return NPERR_NO_ERROR;
}

EXPORT NPError NPP_Destroy(NPP instance, NPSavedData** save) {
  if (!instance) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  NPError result = NPERR_NO_ERROR;
  pepper::PepperPlugin* plugin = pepper::GetPlugin(instance);
  if (plugin) {
    result = plugin->Destroy(save);
    if (result != NPERR_NO_ERROR) {
      return result;
    }
    delete plugin;
    instance->pdata = NULL;
  }
  return result;
}

EXPORT NPError NPP_SetWindow(NPP instance, NPWindow* window) {
  if (!instance) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  pepper::PepperPlugin* plugin = pepper::GetPlugin(instance);
  if (plugin) {
    return plugin->SetWindow(window);
  }

  return NPERR_GENERIC_ERROR;
}

EXPORT NPError NPP_NewStream(NPP instance,
                             NPMIMEType type,
                             NPStream* stream,
                             NPBool seekable,
                             uint16* stype) {
  if (!instance) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  pepper::PepperPlugin* plugin = pepper::GetPlugin(instance);
  if (plugin) {
    return plugin->NewStream(type, stream, seekable, stype);
  }

  return NPERR_GENERIC_ERROR;
}

EXPORT NPError NPP_DestroyStream(NPP instance,
                                 NPStream* stream,
                                 NPReason reason) {
  if (!instance) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  pepper::PepperPlugin* plugin = pepper::GetPlugin(instance);
  if (plugin) {
    return plugin->DestroyStream(stream, reason);
  }

  return NPERR_GENERIC_ERROR;
}

EXPORT void NPP_StreamAsFile(NPP instance,
                             NPStream* stream,
                             const char* fname) {
  if (!instance) {
    return;
  }

  pepper::PepperPlugin* plugin = pepper::GetPlugin(instance);
  if (plugin) {
    plugin->StreamAsFile(stream, fname);
  }
}

EXPORT int32 NPP_WriteReady(NPP instance,
                            NPStream* stream) {
  if (!instance) {
    return 0;
  }

  pepper::PepperPlugin* plugin = pepper::GetPlugin(instance);
  if (plugin) {
    return plugin->WriteReady(stream);
  }

  return 0;
}

EXPORT int32 NPP_Write(NPP instance,
                       NPStream* stream,
                       int32 offset,
                       int32 len,
                       void* buffer) {
  if (!instance) {
    return 0;
  }

  pepper::PepperPlugin* plugin = pepper::GetPlugin(instance);
  if (plugin) {
    return plugin->Write(stream, offset, len, buffer);
  }

  return 0;
}

EXPORT void NPP_Print(NPP instance,
                      NPPrint* platformPrint) {
  if (!instance) {
    return;
  }

  pepper::PepperPlugin* plugin = pepper::GetPlugin(instance);
  if (plugin) {
    plugin->Print(platformPrint);
  }
}

EXPORT int16 NPP_HandleEvent(NPP instance,
                             void* event) {
  if (!instance) {
    return false;
  }

  pepper::PepperPlugin* plugin = pepper::GetPlugin(instance);
  if (plugin) {
    return plugin->HandleEvent(event);
  }

  return false;
}

EXPORT void NPP_URLNotify(NPP instance,
                          const char* url,
                          NPReason reason,
                          void* notifyData) {
  if (!instance) {
    return;
  }

  pepper::PepperPlugin* plugin = pepper::GetPlugin(instance);
  if (plugin) {
    plugin->URLNotify(url, reason, notifyData);
  }
}

EXPORT NPError NPP_GetValue(NPP instance,
                            NPPVariable variable,
                            void* value) {
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  // Note that it is valid to call this routine before the plugin instance
  // has been created.
  // For example, the browser requests the name/description when plugin
  // is loaded or when about:plugins is opened (whichever comes first).
  // Thus, we can't check for a valid instance instance here and bail if
  // it's not setup (like we do for the other routines).

  // If the name/description is being requested, then get that directly.
  if (variable == NPPVpluginNameString) {
    *((const char**)value) = pepper::g_plugin_info.plugin_name;
    return NPERR_NO_ERROR;
  }
  if (variable == NPPVpluginDescriptionString) {
    *((const char**)value) = pepper::g_plugin_info.plugin_description;
    return NPERR_NO_ERROR;
  }
  if (variable == NPPVpluginNeedsXEmbed) {
    *(static_cast<NPBool*>(value)) = true;
    return NPERR_NO_ERROR;
  }
#endif  // defined(OS_POSIX) && !defined(OS_MACOSX)

  if (instance) {
    // If we have an instance, then let the plugin handle the call.
    pepper::PepperPlugin* plugin = pepper::GetPlugin(instance);
    if (plugin) {
      return plugin->GetValue(variable, value);
    }
  }

  return NPERR_GENERIC_ERROR;
}

EXPORT NPError NPP_SetValue(NPP instance,
                            NPNVariable variable,
                            void* value) {
  if (!instance) {
    return NPERR_INVALID_INSTANCE_ERROR;
  }

  pepper::PepperPlugin* plugin = pepper::GetPlugin(instance);
  if (plugin) {
    return plugin->SetValue(variable, value);
  }

  return NPERR_GENERIC_ERROR;
}
