// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "third_party/npapi/bindings/npapi.h"
#include "third_party/npapi/bindings/npfunctions.h"
#include "third_party/npapi/bindings/npruntime.h"

#if defined (OS_WIN)
#define OSCALL __declspec(dllexport)
#else
#define OSCALL __attribute__((visibility("default")))
#endif

/*
 Supported Javascript interface:
 readonly attribute string supportID;
 readonly attribute int state;
   state: {
     disconnected: 1,
     connecting: 2,
     connected: 3,
     affirmingConnection: 4,
     error: 5,
     initializing: 6
  }
 attribute Function void debugInfo(string info);
 attribute Function void onStateChanged();

 void connect(string uid, string auth_token, function done);
 void disconnect();
*/

namespace {

// Global netscape functions initialized in NP_Initialize.
NPNetscapeFuncs* g_npnetscape_funcs = NULL;

// Values returned in GetValue
const char* g_plugin_name = "Remoting Host Plugin";
const char* g_plugin_description = "Remoting Host Plugin";

// Convert an NPIdentifier into a std::string
std::string StringFromNPIdentifier(NPIdentifier identifier) {
  if (!g_npnetscape_funcs->identifierisstring(identifier))
    return std::string();
  NPUTF8* np_string = g_npnetscape_funcs->utf8fromidentifier(identifier);
  std::string string(np_string);
  g_npnetscape_funcs->memfree(np_string);
  return string;
}

// NPAPI plugin implementation for remoting host script object
class HostNPScriptObject {
 public:
  explicit HostNPScriptObject(NPObject* parent) : parent_(parent) {}

  bool Init() {
    return true;
  }

  bool HasMethod(std::string method_name) {
    VLOG(1) << "HasMethod" << method_name;
    return (method_name == kFuncNameConnect ||
            method_name == kFuncNameDisconnect);
  }

  bool InvokeDefault(const NPVariant* args,
                     uint32_t argCount,
                     NPVariant* result) {
    VLOG(1) << "InvokeDefault";
    g_npnetscape_funcs->setexception(parent_, "exception during invocation");
    return false;
  }

  bool Invoke(std::string method_name,
              const NPVariant* args,
              uint32_t argCount,
              NPVariant* result) {
    VLOG(1) << "Invoke " << method_name;
    if (method_name == kFuncNameConnect) {
      return Connect(args, argCount, result);
    } else if (method_name == kFuncNameDisconnect) {
      return Disconnect(args, argCount, result);
    } else {
      g_npnetscape_funcs->setexception(parent_, "exception during invocation");
      return false;
    }
  }

  bool HasProperty(std::string property_name) {
    VLOG(1) << "HasProperty " << property_name;
    return (property_name == kAttrNameSupportID ||
            property_name == kAttrNameState ||
            property_name == kAttrNameDebugInfo ||
            property_name == kAttrNameOnStateChanged);
  }

  bool GetProperty(std::string property_name, NPVariant* result) {
    VLOG(1) << "GetProperty " << property_name;
    NOTIMPLEMENTED();
    return false;
  }

  bool SetProperty(std::string property_name, const NPVariant* value) {
    VLOG(1) << "SetProperty " << property_name;
    // Read-only
    if (property_name == kAttrNameSupportID ||
        property_name == kAttrNameState)
      return false;

    NOTIMPLEMENTED();

    return false;
  }

  bool RemoveProperty(std::string property_name) {
    VLOG(1) << "RemoveProperty " << property_name;
    return false;
  }

  bool Enumerate(std::vector<std::string>* values) {
    VLOG(1) << "Enumerate";
    const char* entries[] = {
      kAttrNameSupportID,
      kAttrNameState,
      kAttrNameDebugInfo,
      kAttrNameOnStateChanged,
      kFuncNameConnect,
      kFuncNameDisconnect
    };
    for (size_t i = 0; i < arraysize(entries); ++i) {
      values->push_back(entries[i]);
    }
    return true;
  }

 private:
  // JS string
  static const char* kAttrNameSupportID;
  // JS int
  static const char* kAttrNameState;
  // JS func(string)
  static const char* kAttrNameDebugInfo;
  // JS func onStateChanged()
  static const char* kAttrNameOnStateChanged;
  // void connect(string uid, string auth_token, function done);
  static const char* kFuncNameConnect;
  // void disconnect();
  static const char* kFuncNameDisconnect;

  NPObject* parent_;

  bool Connect(const NPVariant* args, uint32_t argCount, NPVariant* result) {
    NOTIMPLEMENTED();
    return false;
  }

  bool Disconnect(const NPVariant* args, uint32_t argCount, NPVariant* result) {
    NOTIMPLEMENTED();
    return false;
  }
};

const char* HostNPScriptObject::kAttrNameSupportID = "supportID";
const char* HostNPScriptObject::kAttrNameState = "state";
const char* HostNPScriptObject::kAttrNameDebugInfo = "debugInfo";
const char* HostNPScriptObject::kAttrNameOnStateChanged = "onStateChanged";
const char* HostNPScriptObject::kFuncNameConnect = "connect";
const char* HostNPScriptObject::kFuncNameDisconnect = "disconnect";


// NPAPI plugin implementation for remoting host.
// Documentation for most of the calls in this class can be found here:
// https://developer.mozilla.org/en/Gecko_Plugin_API_Reference/Scripting_plugins
class HostNPPlugin {
 public:
  // |mode| is the display mode of plug-in. Values:
  // NP_EMBED: (1) Instance was created by an EMBED tag and shares the browser
  //               window with other content.
  // NP_FULL: (2) Instance was created by a separate file and is the primary
  //              content in the window.
  HostNPPlugin(NPP instance, uint16 mode)
    : instance_(instance), scriptable_object_(NULL) {}

  ~HostNPPlugin() {
    if (scriptable_object_) {
      g_npnetscape_funcs->releaseobject(scriptable_object_);
      scriptable_object_ = NULL;
    }
  }

  bool Init(int16 argc, char** argn, char** argv, NPSavedData* saved) {
    return true;
  }

  bool Save(NPSavedData** saved) {
    return true;
  }

  NPObject* GetScriptableObject() {
    if (!scriptable_object_) {
      NPClass npc_ref_object = {
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

 private:
  struct ScriptableNPObject : public NPObject {
    HostNPScriptObject* scriptable_object;
  };

  static HostNPScriptObject* ScriptableFromObject(NPObject* obj) {
    return reinterpret_cast<ScriptableNPObject*>(obj)->scriptable_object;
  }

  static NPObject* Allocate(NPP npp, NPClass* aClass) {
    ScriptableNPObject* object =
        reinterpret_cast<ScriptableNPObject*>(
            g_npnetscape_funcs->memalloc(sizeof(ScriptableNPObject)));

    object->_class = aClass;
    object->referenceCount = 1;
    object->scriptable_object = new HostNPScriptObject(object);
    if (!object->scriptable_object->Init()) {
      Deallocate(object);
      object = NULL;
    }
    return object;
  }

  static void Deallocate(NPObject* npobj) {
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
    HostNPScriptObject* scriptable = ScriptableFromObject(obj);
    if (!scriptable) return false;
    return scriptable->InvokeDefault(args, argCount, result);
  }

  static bool Invoke(NPObject* obj,
                     NPIdentifier method_name,
                     const NPVariant* args,
                     uint32_t argCount,
                     NPVariant* result) {
    HostNPScriptObject* scriptable = ScriptableFromObject(obj);
    if (!scriptable)
      return false;
    std::string method_name_string = StringFromNPIdentifier(method_name);
    if (method_name_string.empty())
      return false;
    return scriptable->Invoke(method_name_string, args, argCount, result);
  }

  static bool HasProperty(NPObject* obj, NPIdentifier property_name) {
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
    HostNPScriptObject* scriptable = ScriptableFromObject(obj);
    if (!scriptable) return false;
    std::string property_name_string = StringFromNPIdentifier(property_name);
    if (property_name_string.empty())
      return false;
    return scriptable->SetProperty(property_name_string, value);
  }

  static bool RemoveProperty(NPObject* obj, NPIdentifier property_name) {
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
  VLOG(1) << "CreatePlugin";
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
  VLOG(1) << "DestroyPlugin";
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
    VLOG(1) << "GetValue - default " << variable;
    return NPERR_GENERIC_ERROR;
  case NPPVpluginNameString:
    VLOG(1) << "GetValue - name string";
    *reinterpret_cast<const char**>(value) = g_plugin_name;
    break;
  case NPPVpluginDescriptionString:
    VLOG(1) << "GetValue - description string";
    *reinterpret_cast<const char**>(value) = g_plugin_description;
    break;
  case NPPVpluginScriptableNPObject:
    VLOG(1) << "GetValue - scriptable object";
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
  VLOG(1) << "HandleEvent";
  return NPERR_NO_ERROR;
}

NPError SetWindow(NPP instance, NPWindow* pNPWindow) {
  VLOG(1) << "SetWindow";
  return NPERR_NO_ERROR;
}

}  // namespace

// The actual required NPAPI Entry points

extern "C" {

OSCALL NPError NP_GetEntryPoints(NPPluginFuncs* nppfuncs) {
  VLOG(1) << "NP_GetEntryPoints";
  nppfuncs->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
  nppfuncs->newp = &CreatePlugin;
  nppfuncs->destroy = &DestroyPlugin;
  nppfuncs->getvalue = &GetValue;
  nppfuncs->event = &HandleEvent;
  nppfuncs->setwindow = &SetWindow;

  return NPERR_NO_ERROR;
}

OSCALL NPError NP_Initialize(NPNetscapeFuncs* npnetscape_funcs
#if defined(OS_LINUX)
                            , NPPluginFuncs* nppfuncs
#endif  // OS_LINUX
                            ) {
  VLOG(1) << "NP_Initialize";
  if(npnetscape_funcs == NULL)
    return NPERR_INVALID_FUNCTABLE_ERROR;

  if(((npnetscape_funcs->version & 0xff00) >> 8) > NP_VERSION_MAJOR)
    return NPERR_INCOMPATIBLE_VERSION_ERROR;

  g_npnetscape_funcs = npnetscape_funcs;
#if defined(OS_LINUX)
  NP_GetEntryPoints(nppfuncs);
#endif  // OS_LINUX
  return NPERR_NO_ERROR;
}

OSCALL NPError NP_Shutdown() {
  VLOG(1) << "NP_Shutdown";
  return NPERR_NO_ERROR;
}

OSCALL const char* NP_GetMIMEDescription(void) {
  VLOG(1) << "NP_GetMIMEDescription";
  return "";
}

OSCALL NPError NP_GetValue(void* npp, NPPVariable variable, void* value) {
  return GetValue((NPP)npp, variable, value);
}

}  // extern "C"
