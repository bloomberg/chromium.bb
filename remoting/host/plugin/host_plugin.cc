// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/stringize_macros.h"
#include "net/socket/ssl_server_socket.h"
#include "remoting/base/plugin_thread_task_runner.h"
#include "remoting/host/plugin/constants.h"
#include "remoting/host/plugin/host_log_handler.h"
#include "remoting/host/plugin/host_plugin_utils.h"
#include "remoting/host/plugin/host_script_object.h"
#if defined(OS_WIN)
#include "ui/base/win/dpi.h"
#endif
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npfunctions.h"
#include "third_party/npapi/bindings/npruntime.h"

// Symbol export is handled with a separate def file on Windows.
#if defined (__GNUC__) && __GNUC__ >= 4
#define EXPORT __attribute__((visibility("default")))
#else
#define EXPORT
#endif

#if defined(OS_WIN)
// TODO(wez): libvpx expects these 64-bit division functions to be provided
// by libgcc.a, which we aren't linked against.  These implementations can
// be removed once we have native MSVC libvpx builds for Windows.
extern "C" {

int64_t __cdecl __divdi3(int64_t a, int64_t b) {
  return a / b;
}
uint64_t __cdecl __udivdi3(uint64_t a, uint64_t b) {
  return a / b;
}

}
#endif

using remoting::g_npnetscape_funcs;
using remoting::HostLogHandler;
using remoting::HostNPScriptObject;
using remoting::StringFromNPIdentifier;

namespace {

base::AtExitManager* g_at_exit_manager = NULL;

// NPAPI plugin implementation for remoting host.
// Documentation for most of the calls in this class can be found here:
// https://developer.mozilla.org/en/Gecko_Plugin_API_Reference/Scripting_plugins
class HostNPPlugin : public remoting::PluginThreadTaskRunner::Delegate {
 public:
  // |mode| is the display mode of plug-in. Values:
  // NP_EMBED: (1) Instance was created by an EMBED tag and shares the browser
  //               window with other content.
  // NP_FULL: (2) Instance was created by a separate file and is the primary
  //              content in the window.
  HostNPPlugin(NPP instance, uint16 mode)
      : instance_(instance),
        scriptable_object_(NULL) {
    plugin_task_runner_ = new remoting::PluginThreadTaskRunner(this);
    plugin_auto_task_runner_ =
        new remoting::AutoThreadTaskRunner(
            plugin_task_runner_,
            base::Bind(&remoting::PluginThreadTaskRunner::Quit,
                       plugin_task_runner_));
  }

  virtual ~HostNPPlugin() {
    if (scriptable_object_) {
      DCHECK_EQ(scriptable_object_->referenceCount, 1UL);
      g_npnetscape_funcs->releaseobject(scriptable_object_);
      scriptable_object_ = NULL;
    }

    // Process tasks on |plugin_task_runner_| until all references to
    // |plugin_auto_task_runner_| have been dropped. This requires that the
    // browser has dropped any script object references - see above.
    plugin_auto_task_runner_ = NULL;
    plugin_task_runner_->DetachAndRunShutdownLoop();
  }

  bool Init(int16 argc, char** argn, char** argv, NPSavedData* saved) {
#if defined(OS_MACOSX)
    // Use the modern CoreGraphics and Cocoa models when available, since
    // QuickDraw and Carbon are deprecated.
    // The drawing and event models don't change anything for this plugin, since
    // none of the functions affected by the models actually do anything.
    // This does however keep the plugin from breaking when Chromium eventually
    // drops support for QuickDraw and Carbon, and it also keeps the browser
    // from sending Null Events once a second to support old Carbon based
    // timers.
    // Chromium should always be supporting the newer models.

    // Sanity check to see if Chromium supports the CoreGraphics drawing model.
    NPBool supports_core_graphics = false;
    NPError err = g_npnetscape_funcs->getvalue(instance_,
                                               NPNVsupportsCoreGraphicsBool,
                                               &supports_core_graphics);
    if (err == NPERR_NO_ERROR && supports_core_graphics) {
      // Switch to CoreGraphics drawing model.
      g_npnetscape_funcs->setvalue(instance_, NPPVpluginDrawingModel,
          reinterpret_cast<void*>(NPDrawingModelCoreGraphics));
    } else {
      LOG(ERROR) << "No Core Graphics support";
      return false;
    }

    // Sanity check to see if Chromium supports the Cocoa event model.
    NPBool supports_cocoa = false;
    err = g_npnetscape_funcs->getvalue(instance_, NPNVsupportsCocoaBool,
                                       &supports_cocoa);
    if (err == NPERR_NO_ERROR && supports_cocoa) {
      // Switch to Cocoa event model.
      g_npnetscape_funcs->setvalue(instance_, NPPVpluginEventModel,
          reinterpret_cast<void*>(NPEventModelCocoa));
    } else {
      LOG(ERROR) << "No Cocoa Event Model support";
      return false;
    }
#endif  // OS_MACOSX
    net::EnableSSLServerSockets();
    return true;
  }

  bool Save(NPSavedData** saved) {
    return true;
  }

  NPObject* GetScriptableObject() {
    if (!scriptable_object_) {
      // Must be static. If it is a temporary, objects created by this
      // method will fail in weird and wonderful ways later.
      static NPClass npc_ref_object = {
        NP_CLASS_STRUCT_VERSION,
        &Allocate,
        &Deallocate,
        &Invalidate,
        &HasMethod,
        &Invoke,
        &InvokeDefault,
        &HasProperty,
        &GetProperty,
        &SetProperty,
        &RemoveProperty,
        &Enumerate,
        NULL
      };
      scriptable_object_ = g_npnetscape_funcs->createobject(instance_,
                                                            &npc_ref_object);
    }
    return scriptable_object_;
  }

  // PluginThreadTaskRunner::Delegate implementation.
  virtual bool RunOnPluginThread(
      base::TimeDelta delay, void(function)(void*), void* data) OVERRIDE {
    if (delay == base::TimeDelta()) {
      g_npnetscape_funcs->pluginthreadasynccall(instance_, function, data);
    } else {
      base::AutoLock auto_lock(timers_lock_);
      uint32_t timer_id = g_npnetscape_funcs->scheduletimer(
          instance_, delay.InMilliseconds(), false, &NPDelayedTaskSpringboard);
      DelayedTask task = {function, data};
      timers_[timer_id] = task;
    }
    return true;
  }

  void SetWindow(NPWindow* np_window) {
    if (scriptable_object_) {
      ScriptableFromObject(scriptable_object_)->SetWindow(np_window);
    }
  }

  static void NPDelayedTaskSpringboard(NPP npp, uint32_t timer_id) {
    HostNPPlugin* self = reinterpret_cast<HostNPPlugin*>(npp->pdata);
    DelayedTask task;
    {
      base::AutoLock auto_lock(self->timers_lock_);
      std::map<uint32_t, DelayedTask>::iterator it =
          self->timers_.find(timer_id);
      CHECK(it != self->timers_.end());
      task = it->second;
      self->timers_.erase(it);
    }
    task.function(task.data);
  }

 private:
  struct ScriptableNPObject : public NPObject {
    HostNPScriptObject* scriptable_object;
  };

  struct DelayedTask {
    void (*function)(void*);
    void* data;
  };

  static HostNPScriptObject* ScriptableFromObject(NPObject* obj) {
    return reinterpret_cast<ScriptableNPObject*>(obj)->scriptable_object;
  }

  static NPObject* Allocate(NPP npp, NPClass* aClass) {
    VLOG(2) << "static Allocate";
    ScriptableNPObject* object =
        reinterpret_cast<ScriptableNPObject*>(
            g_npnetscape_funcs->memalloc(sizeof(ScriptableNPObject)));
    HostNPPlugin* plugin = reinterpret_cast<HostNPPlugin*>(npp->pdata);

    object->_class = aClass;
    object->referenceCount = 1;
    object->scriptable_object =
        new HostNPScriptObject(npp, object, plugin->plugin_auto_task_runner_);
    return object;
  }

  static void Deallocate(NPObject* npobj) {
    VLOG(2) << "static Deallocate";
    if (npobj) {
      Invalidate(npobj);
      g_npnetscape_funcs->memfree(npobj);
    }
  }

  static void Invalidate(NPObject* npobj) {
    if (npobj) {
      ScriptableNPObject* object = reinterpret_cast<ScriptableNPObject*>(npobj);
      if (object->scriptable_object) {
        delete object->scriptable_object;
        object->scriptable_object = NULL;
      }
    }
  }

  static bool HasMethod(NPObject* obj, NPIdentifier method_name) {
    VLOG(2) << "static HasMethod";
    HostNPScriptObject* scriptable = ScriptableFromObject(obj);
    if (!scriptable) return false;
    std::string method_name_string = StringFromNPIdentifier(method_name);
    if (method_name_string.empty())
      return false;
    return scriptable->HasMethod(method_name_string);
  }

  static bool InvokeDefault(NPObject* obj,
                            const NPVariant* args,
                            uint32_t argCount,
                            NPVariant* result) {
    VLOG(2) << "static InvokeDefault";
    HostNPScriptObject* scriptable = ScriptableFromObject(obj);
    if (!scriptable) return false;
    return scriptable->InvokeDefault(args, argCount, result);
  }

  static bool Invoke(NPObject* obj,
                     NPIdentifier method_name,
                     const NPVariant* args,
                     uint32_t argCount,
                     NPVariant* result) {
    VLOG(2) << "static Invoke";
    HostNPScriptObject* scriptable = ScriptableFromObject(obj);
    if (!scriptable)
      return false;
    std::string method_name_string = StringFromNPIdentifier(method_name);
    if (method_name_string.empty())
      return false;
    return scriptable->Invoke(method_name_string, args, argCount, result);
  }

  static bool HasProperty(NPObject* obj, NPIdentifier property_name) {
    VLOG(2) << "static HasProperty";
    HostNPScriptObject* scriptable = ScriptableFromObject(obj);
    if (!scriptable) return false;
    std::string property_name_string = StringFromNPIdentifier(property_name);
    if (property_name_string.empty())
      return false;
    return scriptable->HasProperty(property_name_string);
  }

  static bool GetProperty(NPObject* obj,
                          NPIdentifier property_name,
                          NPVariant* result) {
    VLOG(2) << "static GetProperty";
    HostNPScriptObject* scriptable = ScriptableFromObject(obj);
    if (!scriptable) return false;
    std::string property_name_string = StringFromNPIdentifier(property_name);
    if (property_name_string.empty())
      return false;
    return scriptable->GetProperty(property_name_string, result);
  }

  static bool SetProperty(NPObject* obj,
                          NPIdentifier property_name,
                          const NPVariant* value) {
    VLOG(2) << "static SetProperty";
    HostNPScriptObject* scriptable = ScriptableFromObject(obj);
    if (!scriptable) return false;
    std::string property_name_string = StringFromNPIdentifier(property_name);
    if (property_name_string.empty())
      return false;
    return scriptable->SetProperty(property_name_string, value);
  }

  static bool RemoveProperty(NPObject* obj, NPIdentifier property_name) {
    VLOG(2) << "static RemoveProperty";
    HostNPScriptObject* scriptable = ScriptableFromObject(obj);
    if (!scriptable) return false;
    std::string property_name_string = StringFromNPIdentifier(property_name);
    if (property_name_string.empty())
      return false;
    return scriptable->RemoveProperty(property_name_string);
  }

  static bool Enumerate(NPObject* obj,
                        NPIdentifier** value,
                        uint32_t* count) {
    VLOG(2) << "static Enumerate";
    HostNPScriptObject* scriptable = ScriptableFromObject(obj);
    if (!scriptable) return false;
    std::vector<std::string> values;
    bool is_good = scriptable->Enumerate(&values);
    if (is_good) {
      *count = values.size();
      *value = reinterpret_cast<NPIdentifier*>(
          g_npnetscape_funcs->memalloc(sizeof(NPIdentifier) * (*count)));
      for (uint32_t i = 0; i < *count; ++i) {
        (*value)[i] =
            g_npnetscape_funcs->getstringidentifier(values[i].c_str());
      }
    }
    return is_good;
  }

  NPP instance_;
  NPObject* scriptable_object_;

  scoped_refptr<remoting::PluginThreadTaskRunner> plugin_task_runner_;
  scoped_refptr<remoting::AutoThreadTaskRunner> plugin_auto_task_runner_;

  std::map<uint32_t, DelayedTask> timers_;
  base::Lock timers_lock_;
};

// Utility functions to map NPAPI Entry Points to C++ Objects.
HostNPPlugin* PluginFromInstance(NPP instance) {
  return reinterpret_cast<HostNPPlugin*>(instance->pdata);
}

NPError CreatePlugin(NPMIMEType pluginType,
                     NPP instance,
                     uint16 mode,
                     int16 argc,
                     char** argn,
                     char** argv,
                     NPSavedData* saved) {
  VLOG(2) << "CreatePlugin";

  // Register a global log handler.
  // The LogMessage registration code is not thread-safe, so we need to perform
  // this while we're running in a single thread.
  HostLogHandler::RegisterLogMessageHandler();

  HostNPPlugin* plugin = new HostNPPlugin(instance, mode);
  instance->pdata = plugin;
  if (!plugin->Init(argc, argn, argv, saved)) {
    delete plugin;
    instance->pdata = NULL;
    return NPERR_INVALID_PLUGIN_ERROR;
  } else {
    return NPERR_NO_ERROR;
  }
}

NPError DestroyPlugin(NPP instance,
                      NPSavedData** save) {
  VLOG(2) << "DestroyPlugin";

  // Normally, we would unregister the global log handler that we registered
  // in CreatePlugin. However, the LogHandler registration code is not thread-
  // safe so we could crash if we update (register or unregister) the
  // LogHandler while it's being read on another thread.
  // At this point, all our threads should be shutdown, but it's safer to leave
  // the handler registered until we're completely destroyed.

  HostNPPlugin* plugin = PluginFromInstance(instance);
  if (plugin) {
    plugin->Save(save);
    delete plugin;
    instance->pdata = NULL;
    return NPERR_NO_ERROR;
  } else {
    return NPERR_INVALID_PLUGIN_ERROR;
  }
}

NPError GetValue(NPP instance, NPPVariable variable, void* value) {
  switch(variable) {
  default:
    VLOG(2) << "GetValue - default " << variable;
    return NPERR_GENERIC_ERROR;
  case NPPVpluginNameString:
    VLOG(2) << "GetValue - name string";
    *reinterpret_cast<const char**>(value) = HOST_PLUGIN_NAME;
    break;
  case NPPVpluginDescriptionString:
    VLOG(2) << "GetValue - description string";
    *reinterpret_cast<const char**>(value) = HOST_PLUGIN_DESCRIPTION;
    break;
  case NPPVpluginNeedsXEmbed:
    VLOG(2) << "GetValue - NeedsXEmbed";
    *(static_cast<NPBool*>(value)) = true;
    break;
  case NPPVpluginScriptableNPObject:
    VLOG(2) << "GetValue - scriptable object";
    HostNPPlugin* plugin = PluginFromInstance(instance);
    if (!plugin)
      return NPERR_INVALID_PLUGIN_ERROR;
    NPObject* scriptable_object = plugin->GetScriptableObject();
    g_npnetscape_funcs->retainobject(scriptable_object);
    *reinterpret_cast<NPObject**>(value) = scriptable_object;
    break;
  }
  return NPERR_NO_ERROR;
}

NPError HandleEvent(NPP instance, void* ev) {
  VLOG(2) << "HandleEvent";
  return NPERR_NO_ERROR;
}

NPError SetWindow(NPP instance, NPWindow* pNPWindow) {
  VLOG(2) << "SetWindow";
  HostNPPlugin* plugin = PluginFromInstance(instance);
  if (plugin) {
    plugin->SetWindow(pNPWindow);
  }
  return NPERR_NO_ERROR;
}

}  // namespace

#if defined(OS_WIN)
BOOL APIENTRY DllMain(HMODULE hModule, DWORD dwReason, LPVOID lpReserved) {
  switch (dwReason) {
    case DLL_PROCESS_ATTACH:
      DisableThreadLibraryCalls(hModule);
      break;
    case DLL_PROCESS_DETACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
      break;
  }
  return TRUE;
}
#endif

// The actual required NPAPI Entry points

extern "C" {

EXPORT NPError API_CALL NP_GetEntryPoints(NPPluginFuncs* nppfuncs) {
  VLOG(2) << "NP_GetEntryPoints";
  nppfuncs->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
  nppfuncs->newp = &CreatePlugin;
  nppfuncs->destroy = &DestroyPlugin;
  nppfuncs->getvalue = &GetValue;
  nppfuncs->event = &HandleEvent;
  nppfuncs->setwindow = &SetWindow;

  return NPERR_NO_ERROR;
}

EXPORT NPError API_CALL NP_Initialize(NPNetscapeFuncs* npnetscape_funcs
#if defined(OS_POSIX) && !defined(OS_MACOSX)
                            , NPPluginFuncs* nppfuncs
#endif
                            ) {
  VLOG(2) << "NP_Initialize";
  if (g_at_exit_manager)
    return NPERR_MODULE_LOAD_FAILED_ERROR;

  if(npnetscape_funcs == NULL)
    return NPERR_INVALID_FUNCTABLE_ERROR;

  if(((npnetscape_funcs->version & 0xff00) >> 8) > NP_VERSION_MAJOR)
    return NPERR_INCOMPATIBLE_VERSION_ERROR;

  g_at_exit_manager = new base::AtExitManager;
  g_npnetscape_funcs = npnetscape_funcs;
#if defined(OS_POSIX) && !defined(OS_MACOSX)
  NP_GetEntryPoints(nppfuncs);
#endif
  // Init an empty command line for common objects that use it.
  CommandLine::Init(0, NULL);

#if defined(OS_WIN)
  ui::EnableHighDPISupport();
#endif

  return NPERR_NO_ERROR;
}

EXPORT NPError API_CALL NP_Shutdown() {
  VLOG(2) << "NP_Shutdown";
  delete g_at_exit_manager;
  g_at_exit_manager = NULL;
  return NPERR_NO_ERROR;
}

#if defined(OS_POSIX) && !defined(OS_MACOSX)
EXPORT const char* API_CALL NP_GetMIMEDescription(void) {
  VLOG(2) << "NP_GetMIMEDescription";
  return HOST_PLUGIN_MIME_TYPE "::";
}

EXPORT NPError API_CALL NP_GetValue(void* npp,
                                    NPPVariable variable,
                                    void* value) {
  return GetValue((NPP)npp, variable, value);
}
#endif

}  // extern "C"
