// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "base/at_exit.h"
#include "base/basictypes.h"
#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/rand_util.h"
#include "base/stringize_macros.h"
#include "base/task.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread.h"
#include "remoting/host/chromoting_host.h"
#include "remoting/host/chromoting_host_context.h"
#include "remoting/host/host_config.h"
#include "remoting/host/host_key_pair.h"
#include "remoting/host/in_memory_host_config.h"
#include "remoting/host/register_support_host_request.h"
#include "remoting/host/support_access_verifier.h"
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
 readonly attribute string accessCode;
 readonly attribute int state;

 state: {
     DISCONNECTED,
     REQUESTED_ACCESS_CODE,
     RECEIVED_ACCESS_CODE,
     CONNECTED,
     AFFIRMING_CONNECTION,
     ERROR,
 }

 attribute Function void onStateChanged();

 void connect(string uid, string auth_token);
 void disconnect();
*/

namespace {

// Global netscape functions initialized in NP_Initialize.
NPNetscapeFuncs* g_npnetscape_funcs = NULL;

// The name and description are returned by GetValue, but are also
// combined with the MIME type to satisfy GetMIMEDescription, so we
// use macros here to allow that to happen at compile-time.
#define HOST_PLUGIN_NAME "Remoting Host Plugin"
#define HOST_PLUGIN_DESCRIPTION "Remoting Host Plugin"

// Convert an NPIdentifier into a std::string.
std::string StringFromNPIdentifier(NPIdentifier identifier) {
  if (!g_npnetscape_funcs->identifierisstring(identifier))
    return std::string();
  NPUTF8* np_string = g_npnetscape_funcs->utf8fromidentifier(identifier);
  std::string string(np_string);
  g_npnetscape_funcs->memfree(np_string);
  return string;
}

// Convert an NPVariant into a std::string.
std::string StringFromNPVariant(const NPVariant& variant) {
  if (!NPVARIANT_IS_STRING(variant))
    return std::string();
  const NPString& np_string = NPVARIANT_TO_STRING(variant);
  return std::string(np_string.UTF8Characters, np_string.UTF8Length);
}

// Convert a std::string into an NPVariant.
// Caller is responsible for making sure that NPN_ReleaseVariantValue is
// called on returned value.
NPVariant NPVariantFromString(const std::string& val) {
  size_t len = val.length();
  NPUTF8* chars =
      reinterpret_cast<NPUTF8*>(g_npnetscape_funcs->memalloc(len + 1));
  strcpy(chars, val.c_str());
  NPVariant variant;
  STRINGN_TO_NPVARIANT(chars, len, variant);
  return variant;
}

// Convert an NPVariant into an NSPObject.
NPObject* ObjectFromNPVariant(const NPVariant& variant) {
  if (!NPVARIANT_IS_OBJECT(variant))
    return NULL;
  return NPVARIANT_TO_OBJECT(variant);
}

// NPAPI plugin implementation for remoting host script object.
class HostNPScriptObject {
 public:
  HostNPScriptObject(NPP plugin, NPObject* parent)
      : plugin_(plugin),
        parent_(parent),
        state_(kDisconnected),
        on_state_changed_func_(NULL),
        np_thread_id_(base::PlatformThread::CurrentId()) {
    LOG(INFO) << "HostNPScriptObject";
  }

  ~HostNPScriptObject() {
    CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
    host_context_.Stop();
    if (on_state_changed_func_) {
      g_npnetscape_funcs->releaseobject(on_state_changed_func_);
    }
  }

  bool Init() {
    LOG(INFO) << "Init";
    // TODO(wez): This starts a bunch of threads, which might fail.
    host_context_.Start();
    return true;
  }

  bool HasMethod(const std::string& method_name) {
    LOG(INFO) << "HasMethod " << method_name;
    CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
    return (method_name == kFuncNameConnect ||
            method_name == kFuncNameDisconnect);
  }

  bool InvokeDefault(const NPVariant* args,
                     uint32_t argCount,
                     NPVariant* result) {
    LOG(INFO) << "InvokeDefault";
    CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
    SetException("exception during default invocation");
    return false;
  }

  bool Invoke(const std::string& method_name,
              const NPVariant* args,
              uint32_t argCount,
              NPVariant* result) {
    LOG(INFO) << "Invoke " << method_name;
    CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
    if (method_name == kFuncNameConnect) {
      return Connect(args, argCount, result);
    } else if (method_name == kFuncNameDisconnect) {
      return Disconnect(args, argCount, result);
    } else {
      SetException("Invoke: unknown method " + method_name);
      return false;
    }
  }

  bool HasProperty(const std::string& property_name) {
    LOG(INFO) << "HasProperty " << property_name;
    CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
    return (property_name == kAttrNameAccessCode ||
            property_name == kAttrNameState ||
            property_name == kAttrNameOnStateChanged ||
            property_name == kAttrNameDisconnected ||
            property_name == kAttrNameRequestedAccessCode ||
            property_name == kAttrNameReceivedAccessCode ||
            property_name == kAttrNameConnected ||
            property_name == kAttrNameAffirmingConnection ||
            property_name == kAttrNameError);
  }

  bool GetProperty(const std::string& property_name, NPVariant* result) {
    LOG(INFO) << "GetProperty " << property_name;
    CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
    if (!result) {
      SetException("GetProperty: NULL result");
      return false;
    }

    if (property_name == kAttrNameOnStateChanged) {
      OBJECT_TO_NPVARIANT(on_state_changed_func_, *result);
      return true;
    } else if (property_name == kAttrNameState) {
      INT32_TO_NPVARIANT(state_, *result);
      return true;
    } else if (property_name == kAttrNameAccessCode) {
      *result = NPVariantFromString(access_code_);
      return true;
    } else if (property_name == kAttrNameDisconnected) {
      INT32_TO_NPVARIANT(kDisconnected, *result);
      return true;
    } else if (property_name == kAttrNameRequestedAccessCode) {
      INT32_TO_NPVARIANT(kRequestedAccessCode, *result);
      return true;
    } else if (property_name == kAttrNameReceivedAccessCode) {
      INT32_TO_NPVARIANT(kReceivedAccessCode, *result);
      return true;
    } else if (property_name == kAttrNameConnected) {
      INT32_TO_NPVARIANT(kConnected, *result);
      return true;
    } else if (property_name == kAttrNameAffirmingConnection) {
      INT32_TO_NPVARIANT(kAffirmingConnection, *result);
      return true;
    } else if (property_name == kAttrNameError) {
      INT32_TO_NPVARIANT(kError, *result);
      return true;
    } else {
      SetException("GetProperty: unsupported property " + property_name);
      return false;
    }
  }

  bool SetProperty(const std::string& property_name, const NPVariant* value) {
    LOG(INFO) << "SetProperty " << property_name;
    CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);

    if (property_name == kAttrNameOnStateChanged) {
      if (on_state_changed_func_) {
        g_npnetscape_funcs->releaseobject(on_state_changed_func_);
        on_state_changed_func_ = NULL;
      }
      if (NPVARIANT_IS_OBJECT(*value)) {
        on_state_changed_func_ = NPVARIANT_TO_OBJECT(*value);
        if (on_state_changed_func_) {
          g_npnetscape_funcs->retainobject(on_state_changed_func_);
        }
        return true;
      } else {
        SetException("SetProperty: unexpected type for property " +
                     property_name);
      }
    }

    return false;
  }

  bool RemoveProperty(const std::string& property_name) {
    LOG(INFO) << "RemoveProperty " << property_name;
    CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
    return false;
  }

  bool Enumerate(std::vector<std::string>* values) {
    LOG(INFO) << "Enumerate";
    CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
    const char* entries[] = {
      kAttrNameAccessCode,
      kAttrNameState,
      kAttrNameOnStateChanged,
      kFuncNameConnect,
      kFuncNameDisconnect,
      kAttrNameDisconnected,
      kAttrNameRequestedAccessCode,
      kAttrNameReceivedAccessCode,
      kAttrNameConnected,
      kAttrNameAffirmingConnection,
      kAttrNameError
    };
    for (size_t i = 0; i < arraysize(entries); ++i) {
      values->push_back(entries[i]);
    }
    return true;
  }

 private:
  // JS string
  static const char* kAttrNameAccessCode;
  // JS int
  static const char* kAttrNameState;
  // JS func onStateChanged()
  static const char* kAttrNameOnStateChanged;
  // void connect(string uid, string auth_token);
  static const char* kFuncNameConnect;
  // void disconnect();
  static const char* kFuncNameDisconnect;

  // States
  static const char* kAttrNameDisconnected;
  static const char* kAttrNameRequestedAccessCode;
  static const char* kAttrNameReceivedAccessCode;
  static const char* kAttrNameConnected;
  static const char* kAttrNameAffirmingConnection;
  static const char* kAttrNameError;

  enum State {
    kDisconnected,
    kRequestedAccessCode,
    kReceivedAccessCode,
    kConnected,
    kAffirmingConnection,
    kError
  };

  NPP plugin_;
  NPObject* parent_;
  int state_;
  std::string access_code_;
  NPObject* on_state_changed_func_;
  base::AtExitManager exit_manager_;
  base::PlatformThreadId np_thread_id_;

  scoped_refptr<remoting::RegisterSupportHostRequest> register_request_;
  scoped_refptr<remoting::ChromotingHost> host_;
  scoped_refptr<remoting::MutableHostConfig> host_config_;
  remoting::ChromotingHostContext host_context_;
  std::string host_secret_;

  // Start connection. args are:
  //   string uid, string auth_token
  // No result.
  bool Connect(const NPVariant* args, uint32_t argCount, NPVariant* result);

  // Disconnect. No arguments or result.
  bool Disconnect(const NPVariant* args, uint32_t argCount, NPVariant* result);

  // Call OnStateChanged handler if there is one.
  void OnStateChanged(State state);

  // Currently just mock methods to verify that everything is working
  void OnReceivedSupportID(bool success, const std::string& support_id);
  void OnConnected();
  void OnHostShutdown();

  // Call a JavaScript function wrapped as an NPObject.
  // If result is non-null, the result of the call will be stored in it.
  // Caller is responsible for releasing result if they ask for it.
  static bool CallJSFunction(NPObject* func,
                             const NPVariant* args,
                             uint32_t argCount,
                             NPVariant* result);

  // Posts a task on the main NP thread.
  void PostTaskToNPThread(Task* task);

  // Utility function for PostTaskToNPThread.
  static void NPTaskSpringboard(void* task);

  // Set an exception for the current call.
  void SetException(const std::string& exception_string);
};

const char* HostNPScriptObject::kAttrNameAccessCode = "accessCode";
const char* HostNPScriptObject::kAttrNameState = "state";
const char* HostNPScriptObject::kAttrNameOnStateChanged = "onStateChanged";
const char* HostNPScriptObject::kFuncNameConnect = "connect";
const char* HostNPScriptObject::kFuncNameDisconnect = "disconnect";

// States
const char* HostNPScriptObject::kAttrNameDisconnected = "DISCONNECTED";
const char* HostNPScriptObject::kAttrNameRequestedAccessCode =
    "REQUESTED_ACCESS_CODE";
const char* HostNPScriptObject::kAttrNameReceivedAccessCode =
    "RECEIVED_ACCESS_CODE";
const char* HostNPScriptObject::kAttrNameConnected = "CONNECTED";
const char* HostNPScriptObject::kAttrNameAffirmingConnection =
    "AFFIRMING_CONNECTION";
const char* HostNPScriptObject::kAttrNameError = "ERROR";

// string uid, string auth_token
bool HostNPScriptObject::Connect(const NPVariant* args,
                                 uint32_t arg_count,
                                 NPVariant* result) {
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  if (arg_count != 2) {
    SetException("connect: bad number of arguments");
    return false;
  }

  std::string uid = StringFromNPVariant(args[0]);
  if (uid.empty()) {
    SetException("connect: bad uid argument");
    return false;
  }

  std::string auth_token = StringFromNPVariant(args[1]);
  if (auth_token.empty()) {
    SetException("connect: bad auth_token argument");
    return false;
  }

  // Store the supplied user ID and token to the Host configuration.
  scoped_refptr<remoting::MutableHostConfig> host_config =
      new remoting::InMemoryHostConfig;
  host_config->SetString(remoting::kXmppLoginConfigPath, uid);
  host_config->SetString(remoting::kXmppAuthTokenConfigPath, auth_token);

  // Create an access verifier and fetch the host secret.
  scoped_ptr<remoting::SupportAccessVerifier> access_verifier;
  access_verifier.reset(new remoting::SupportAccessVerifier);
  if (!access_verifier->Init()) {
    SetException("connect: SupportAccessVerifier::Init failed");
    return false;
  }
  host_secret_ = access_verifier->host_secret();

  // Generate a key pair for the Host to use.
  // TODO(wez): Move this to the worker thread.
  remoting::HostKeyPair host_key_pair;
  host_key_pair.Generate();
  host_key_pair.Save(host_config);

  // Create the Host.
  scoped_refptr<remoting::ChromotingHost> host =
      remoting::ChromotingHost::Create(&host_context_, host_config,
                                       access_verifier.release());

  // Request registration of the host for support.
  scoped_refptr<remoting::RegisterSupportHostRequest> register_request =
      new remoting::RegisterSupportHostRequest();
  if (!register_request->Init(
          host_config.get(),
          base::Bind(&HostNPScriptObject::OnReceivedSupportID,
                     base::Unretained(this)))) {
    SetException("connect: RegisterSupportHostRequest::Init failed");
    return false;
  }
  host->AddStatusObserver(register_request);

  // Nothing went wrong, so lets save the host, config and request.
  host_ = host;
  host_config_ = host_config;
  register_request_ = register_request;

  OnStateChanged(kRequestedAccessCode);
  return true;
}

bool HostNPScriptObject::Disconnect(const NPVariant* args,
                                    uint32_t arg_count,
                                    NPVariant* result) {
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  if (arg_count != 0) {
    SetException("disconnect: bad number of arguments");
    return false;
  }

  if (host_.get()) {
    host_->Shutdown();
    host_ = NULL;
  }
  register_request_ = NULL;
  host_config_ = NULL;
  host_secret_.clear();

  OnStateChanged(kDisconnected);
  return true;
}

void HostNPScriptObject::OnReceivedSupportID(bool success,
                                             const std::string& support_id) {
  CHECK_NE(base::PlatformThread::CurrentId(), np_thread_id_);

  if (!success) {
    // TODO(wez): Replace the success/fail flag with full error reporting.
    OnHostShutdown();
    return;
  }

  // Combine the Support Id with the Host Id to make the Access Code
  // TODO(wez): Locking, anyone?
  access_code_ = support_id + "-" + host_secret_;
  host_secret_ = std::string();

  // TODO(wez): Attach to the Host for notifications of state changes.
  // It currently has no interface for that, though.

  // Start the Host.
  host_->Start(NewRunnableMethod(this, &HostNPScriptObject::OnHostShutdown));

  // Let the caller know that life is good.
  OnStateChanged(kReceivedAccessCode);
}

void HostNPScriptObject::OnConnected() {
  CHECK_NE(base::PlatformThread::CurrentId(), np_thread_id_);
  OnStateChanged(kConnected);
}

void HostNPScriptObject::OnHostShutdown() {
  CHECK_NE(base::PlatformThread::CurrentId(), np_thread_id_);
  OnStateChanged(kDisconnected);
}

void HostNPScriptObject::OnStateChanged(State state) {
  if (base::PlatformThread::CurrentId() != np_thread_id_) {
    PostTaskToNPThread(NewRunnableMethod(this,
                                         &HostNPScriptObject::OnStateChanged,
                                         state));
    return;
  }
  state_ = state;
  if (on_state_changed_func_) {
    LOG(INFO) << "Calling state changed " << state;
    bool is_good = CallJSFunction(on_state_changed_func_, NULL, 0, NULL);
    LOG_IF(ERROR, !is_good) << "OnStateChangedNP failed";
  }
}

void HostNPScriptObject::SetException(const std::string& exception_string) {
  CHECK_EQ(base::PlatformThread::CurrentId(), np_thread_id_);
  g_npnetscape_funcs->setexception(parent_, exception_string.c_str());
}

bool HostNPScriptObject::CallJSFunction(NPObject* func,
                                        const NPVariant* args,
                                        uint32_t argCount,
                                        NPVariant* result) {
  NPVariant np_result;
  bool is_good = func->_class->invokeDefault(func, args, argCount, &np_result);
  if (is_good) {
    if (result) {
      *result = np_result;
    } else {
      g_npnetscape_funcs->releasevariantvalue(&np_result);
    }
  }
  return is_good;
}

void HostNPScriptObject::PostTaskToNPThread(Task* task) {
  // Can be called from any thread.
  g_npnetscape_funcs->pluginthreadasynccall(plugin_,
                                            &NPTaskSpringboard,
                                            task);
}

void HostNPScriptObject::NPTaskSpringboard(void* task) {
  Task* real_task = reinterpret_cast<Task*>(task);
  real_task->Run();
  delete real_task;
}

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

 private:
  struct ScriptableNPObject : public NPObject {
    HostNPScriptObject* scriptable_object;
  };

  static HostNPScriptObject* ScriptableFromObject(NPObject* obj) {
    return reinterpret_cast<ScriptableNPObject*>(obj)->scriptable_object;
  }

  static NPObject* Allocate(NPP npp, NPClass* aClass) {
    LOG(INFO) << "static Allocate";
    ScriptableNPObject* object =
        reinterpret_cast<ScriptableNPObject*>(
            g_npnetscape_funcs->memalloc(sizeof(ScriptableNPObject)));

    object->_class = aClass;
    object->referenceCount = 1;
    object->scriptable_object = new HostNPScriptObject(npp, object);
    if (!object->scriptable_object->Init()) {
      Deallocate(object);
      object = NULL;
    }
    return object;
  }

  static void Deallocate(NPObject* npobj) {
    LOG(INFO) << "static Deallocate";
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
    LOG(INFO) << "static HasMethod";
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
    LOG(INFO) << "static InvokeDefault";
    HostNPScriptObject* scriptable = ScriptableFromObject(obj);
    if (!scriptable) return false;
    return scriptable->InvokeDefault(args, argCount, result);
  }

  static bool Invoke(NPObject* obj,
                     NPIdentifier method_name,
                     const NPVariant* args,
                     uint32_t argCount,
                     NPVariant* result) {
    LOG(INFO) << "static Invoke";
    HostNPScriptObject* scriptable = ScriptableFromObject(obj);
    if (!scriptable)
      return false;
    std::string method_name_string = StringFromNPIdentifier(method_name);
    if (method_name_string.empty())
      return false;
    return scriptable->Invoke(method_name_string, args, argCount, result);
  }

  static bool HasProperty(NPObject* obj, NPIdentifier property_name) {
    LOG(INFO) << "static HasProperty";
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
    LOG(INFO) << "static GetProperty";
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
    LOG(INFO) << "static SetProperty";
    HostNPScriptObject* scriptable = ScriptableFromObject(obj);
    if (!scriptable) return false;
    std::string property_name_string = StringFromNPIdentifier(property_name);
    if (property_name_string.empty())
      return false;
    return scriptable->SetProperty(property_name_string, value);
  }

  static bool RemoveProperty(NPObject* obj, NPIdentifier property_name) {
    LOG(INFO) << "static RemoveProperty";
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
    LOG(INFO) << "static Enumerate";
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
  LOG(INFO) << "CreatePlugin";
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
  LOG(INFO) << "DestroyPlugin";
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
    LOG(INFO) << "GetValue - default " << variable;
    return NPERR_GENERIC_ERROR;
  case NPPVpluginNameString:
    LOG(INFO) << "GetValue - name string";
    *reinterpret_cast<const char**>(value) = HOST_PLUGIN_NAME;
    break;
  case NPPVpluginDescriptionString:
    LOG(INFO) << "GetValue - description string";
    *reinterpret_cast<const char**>(value) = HOST_PLUGIN_DESCRIPTION;
    break;
  case NPPVpluginNeedsXEmbed:
    LOG(INFO) << "GetValue - NeedsXEmbed";
    *(static_cast<NPBool*>(value)) = true;
    break;
  case NPPVpluginScriptableNPObject:
    LOG(INFO) << "GetValue - scriptable object";
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
  LOG(INFO) << "HandleEvent";
  return NPERR_NO_ERROR;
}

NPError SetWindow(NPP instance, NPWindow* pNPWindow) {
  LOG(INFO) << "SetWindow";
  return NPERR_NO_ERROR;
}

}  // namespace

// TODO(fix): Temporary hack while we figure out threading models correctly.
DISABLE_RUNNABLE_METHOD_REFCOUNT(HostNPScriptObject);

// The actual required NPAPI Entry points

extern "C" {

OSCALL NPError NP_GetEntryPoints(NPPluginFuncs* nppfuncs) {
  LOG(INFO) << "NP_GetEntryPoints";
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
  LOG(INFO) << "NP_Initialize";
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
  LOG(INFO) << "NP_Shutdown";
  return NPERR_NO_ERROR;
}

OSCALL const char* NP_GetMIMEDescription(void) {
  LOG(INFO) << "NP_GetMIMEDescription";
  return STRINGIZE(HOST_PLUGIN_MIME_TYPE) ":"
      HOST_PLUGIN_NAME ":"
      HOST_PLUGIN_DESCRIPTION;
}

OSCALL NPError NP_GetValue(void* npp, NPPVariable variable, void* value) {
  return GetValue((NPP)npp, variable, value);
}

}  // extern "C"
