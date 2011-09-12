// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Scriptable handle implementation.

#include "native_client/src/trusted/plugin/scriptable_handle.h"

#include <stdio.h>
#include <string.h>

#include <assert.h>
#include <set>
#include <sstream>
#include <string>
#include <vector>


#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/srpc/nacl_srpc.h"
#include "native_client/src/trusted/plugin/array_ppapi.h"
#include "native_client/src/trusted/plugin/browser_interface.h"
#include "native_client/src/trusted/plugin/desc_based_handle.h"
#include "native_client/src/trusted/plugin/method_map.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/utility.h"
#include "native_client/src/trusted/plugin/var_utils.h"


namespace plugin {

namespace {

// For security we keep track of the set of scriptable handles that were
// created.

std::set<const plugin::ScriptableHandle*>* g_ValidHandles = 0;

void RememberValidHandle(const ScriptableHandle* handle) {
  // Initialize the set.
  // BUG: this is not thread safe.  We may leak sets, or worse, may not
  // correctly insert valid handles into the set.
  // TODO(sehr): use pthread_once or similar initialization.
  if (NULL == g_ValidHandles) {
    g_ValidHandles = new(std::nothrow) std::set<const ScriptableHandle*>;
    if (NULL == g_ValidHandles) {
      return;
    }
  }
  // Remember the handle.
  g_ValidHandles->insert(handle);
}

pp::Var Error(nacl::string call_name, const char* caller,
              const char* error, pp::Var* exception) {
  nacl::stringstream error_stream;
  error_stream << call_name << ": " << error;
  if (!exception->is_undefined()) {
    error_stream << " - " + exception->AsString();
  }
  // Get the error string in 2 steps; otherwise, the temporary string returned
  // by the stream is destructed, causing a dangling pointer.
  std::string str = error_stream.str();
  const char* e = str.c_str();
  PLUGIN_PRINTF(("ScriptableHandle::%s (%s)\n", caller, e));
  *exception = pp::Var(e);
  return pp::Var();
}

// Helper functionality common to HasProperty and HasMethod.
bool HasCallType(Plugin* plugin,
                 CallType call_type,
                 nacl::string call_name,
                 const char* caller) {
  uintptr_t id = plugin->browser_interface()->StringToIdentifier(call_name);
  PLUGIN_PRINTF(("ScriptableHandle::%s (id=%"NACL_PRIxPTR")\n",
                 caller, id));
  return plugin->HasMethod(id, call_type);
}

// Helper functionality common to GetProperty, SetProperty and Call.
// If |call_type| is PROPERTY_GET, ignores args and expects 1 return var.
// If |call_type| is PROPERTY_SET, expects 1 arg and returns void var.
// Sets |exception| on failure.
pp::Var Invoke(Plugin* plugin,
               CallType call_type,
               nacl::string call_name,
               const char* caller,
               const std::vector<pp::Var>& args,
               pp::Var* exception) {
  uintptr_t id = plugin->browser_interface()->StringToIdentifier(call_name);

  // Initialize input/output parameters.
  SrpcParams params;
  NaClSrpcArg** inputs = params.ins();
  NaClSrpcArg** outputs = params.outs();
  if (!plugin->InitParams(id, call_type, &params)) {
    return Error(call_name, caller,
                 "srpc parameter initialization failed", exception);
  }
  uint32_t input_length = params.InputLength();
  int32_t output_length = params.OutputLength();
  PLUGIN_PRINTF(("ScriptableHandle::%s (initialized %"NACL_PRIu32" ins, %"
                 NACL_PRIu32" outs)\n", caller, input_length, output_length));

  // Verify input/output parameter list length.
  if (args.size() != params.SignatureLength()) {
    return Error(call_name, caller,
                 "incompatible srpc parameter list", exception);
  }
  PLUGIN_PRINTF(("ScriptableHandle::%s (verified signature)\n", caller));

  // Marshall input parameters.
  if (input_length > 0) {
    assert(call_type != PROPERTY_GET);  // expect no inputs for "get"
    for (int i = 0; (i < NACL_SRPC_MAX_ARGS) && (inputs[i] != NULL); ++i) {
      if (!PPVarToNaClSrpcArg(args[i], inputs[i], exception)) {
        return Error(call_name, caller,
                     "srpc input marshalling failed", exception);
      }
    }
  }
  if (call_type == PROPERTY_SET) assert(input_length == 1);
  PLUGIN_PRINTF(("ScriptableHandle::%s (marshalled inputs)\n", caller));

  // Allocate array-typed output parameters.
  if (args.size() > input_length) {
    for (int i = 0; (i < NACL_SRPC_MAX_ARGS) && (outputs[i] != NULL); ++i) {
      if (!PPVarToAllocateNaClSrpcArg(args[input_length + i],
                                      outputs[i], exception)) {
        return Error(call_name, caller, "srpc output array allocation failed",
                     exception);
      }
    }
  }
  PLUGIN_PRINTF(("ScriptableHandle::%s (output array allocation done)\n",
                 caller));

  // Invoke.
  if (!plugin->Invoke(id, call_type, &params)) {
    nacl::string err = nacl::string(caller) + "('" + call_name + "') failed\n";
    if (params.exception_string() != NULL) {
      err = params.exception_string();
    }
    *exception = pp::Var(err.c_str());
    return Error(call_name, caller, "invocation failed", exception);
  }
  PLUGIN_PRINTF(("ScriptableHandle::%s (invocation done)\n", caller));

  // Marshall output parameters.
  pp::Var retvar;
  if (output_length > 0) {
    assert(call_type != PROPERTY_SET);  // expect no outputs for "set"
    retvar = NaClSrpcArgToPPVar(outputs[0], plugin, exception);
    if (output_length > 1) {
      ArrayPpapi* array = new(std::nothrow) ArrayPpapi(plugin);
      if (array == NULL) {
        *exception = pp::Var("failed to allocate output array");
      } else {
        array->SetProperty(pp::Var(0), retvar, exception);
        for (int32_t i = 1; i < output_length; ++i) {
          pp::Var v = NaClSrpcArgToPPVar(outputs[i], plugin, exception);
          array->SetProperty(pp::Var(i), v, exception);
        }
      }

      retvar = pp::VarPrivate(plugin, array);
    }
    if (!exception->is_undefined()) {
      return Error(call_name, caller, "srpc output marshalling failed",
                   exception);
    }
  }
  if (call_type == PROPERTY_GET) assert(output_length == 1);
  return retvar;
}

}  // namespace

ScriptableHandle::ScriptableHandle(Plugin* plugin)
  : var_(NULL), num_unref_calls_(0), plugin_(plugin), desc_handle_(NULL) {
  PLUGIN_PRINTF(("ScriptableHandle::ScriptableHandle (this=%p, plugin=%p)\n",
                 static_cast<void*>(this),
                 static_cast<void*>(plugin)));
  RememberValidHandle(this);
  PLUGIN_PRINTF(("ScriptableHandle::ScriptableHandle (this=%p)\n",
                 static_cast<void*>(this)));
}

ScriptableHandle::ScriptableHandle(DescBasedHandle* desc_handle)
  : var_(NULL), num_unref_calls_(0), plugin_(NULL), desc_handle_(desc_handle) {
  PLUGIN_PRINTF(("ScriptableHandle::ScriptableHandle (this=%p,"
                 " desc_handle=%p)\n",
                 static_cast<void*>(this),
                 static_cast<void*>(desc_handle)));
  RememberValidHandle(this);
  PLUGIN_PRINTF(("ScriptableHandle::ScriptableHandle (this=%p)\n",
                 static_cast<void*>(this)));
}

ScriptableHandle::~ScriptableHandle() {
  PLUGIN_PRINTF(("ScriptableHandle::~ScriptableHandle (this=%p)\n",
                 static_cast<void*>(this)));
  // If the set was empty, just return.
  if (NULL == g_ValidHandles) {
    return;
  }
  // Remove the scriptable handle from the set of valid handles.
  g_ValidHandles->erase(this);
  // If handle is a plugin, the browser is deleting it (and might have
  // already done so). Otherwise, delete here.
  if (desc_handle_ != NULL) {
    PLUGIN_PRINTF(("ScriptableHandle::~ScriptableHandle "
                   "(this=%p, delete desc_handle=%p)\n",
                   static_cast<void*>(this), static_cast<void*>(desc_handle_)));
    delete desc_handle_;
    desc_handle_ = NULL;
  }
  PLUGIN_PRINTF(("ScriptableHandle::~ScriptableHandle (this=%p, return)\n",
                  static_cast<void*>(this)));
}

// Check that an object is a validly created ScriptableHandle.
bool ScriptableHandle::is_valid(const ScriptableHandle* handle) {
  PLUGIN_PRINTF(("ScriptableHandle::is_valid (handle=%p)\n",
                 static_cast<void*>(const_cast<ScriptableHandle*>(handle))));
  if (NULL == g_ValidHandles) {
    PLUGIN_PRINTF(("ScriptableHandle::is_valid (return 0)\n"));
    return false;
  }
  size_t count =
      g_ValidHandles->count(static_cast<const ScriptableHandle*>(handle));
  PLUGIN_PRINTF(("ScriptableHandle::is_valid (handle=%p, count=%"
                 NACL_PRIuS")\n",
                 static_cast<void*>(const_cast<ScriptableHandle*>(handle)),
                 count));
  return 0 != count;
}

void ScriptableHandle::Unref(ScriptableHandle** handle) {
  if (*handle != NULL) {
    (*handle)->Unref();
    *handle = NULL;
  }
}

ScriptableHandle* ScriptableHandle::NewPlugin(Plugin* plugin) {
  PLUGIN_PRINTF(("ScriptableHandle::NewPlugin (plugin=%p)\n",
                 static_cast<void*>(plugin)));
  if (plugin == NULL) {
    return NULL;
  }
  ScriptableHandle* scriptable_handle =
      new(std::nothrow) ScriptableHandle(plugin);
  if (scriptable_handle == NULL) {
    return NULL;
  }
  PLUGIN_PRINTF(("ScriptableHandle::NewPlugin (return %p)\n",
                 static_cast<void*>(scriptable_handle)));
  return scriptable_handle;
}


ScriptableHandle* ScriptableHandle::NewDescHandle(
    DescBasedHandle* desc_handle) {
  PLUGIN_PRINTF(("ScriptableHandle::NewDescHandle (desc_handle=%p)\n",
                 static_cast<void*>(desc_handle)));
  if (desc_handle == NULL) {
    return NULL;
  }
  ScriptableHandle* scriptable_handle =
      new(std::nothrow) ScriptableHandle(desc_handle);
  if (scriptable_handle == NULL) {
    return NULL;
  }
  PLUGIN_PRINTF(("ScriptableHandle::NewDescHandle (return %p)\n",
                 static_cast<void*>(scriptable_handle)));
  return scriptable_handle;
}


bool ScriptableHandle::HasProperty(const pp::Var& name, pp::Var* exception) {
  UNREFERENCED_PARAMETER(exception);
  PLUGIN_PRINTF(("ScriptableHandle::HasProperty (this=%p, name=%s)\n",
                 static_cast<void*>(this), name.DebugString().c_str()));
  if (plugin_ == NULL) {
    return false;
  }
  if (!name.is_string() && !name.is_int())
    return false;
  bool has_property = HasCallType(plugin_,
                                  PROPERTY_GET,
                                  name.AsString(),
                                  "HasProperty");
  PLUGIN_PRINTF(("ScriptableHandle::HasProperty (has_property=%d)\n",
                 has_property));
  return has_property;
}


bool ScriptableHandle::HasMethod(const pp::Var& name, pp::Var* exception) {
  UNREFERENCED_PARAMETER(exception);
  PLUGIN_PRINTF(("ScriptableHandle::HasMethod (this=%p, name='%s')\n",
                 static_cast<void*>(this), name.DebugString().c_str()));
  if (plugin_ == NULL) {
    return false;
  }
  if (!name.is_string())
    return false;
  bool has_method = HasCallType(plugin_,
                                METHOD_CALL,
                                name.AsString(),
                                "HasMethod");
  PLUGIN_PRINTF(("ScriptableHandle::HasMethod (has_method=%d)\n",
                 has_method));
  return has_method;
}


pp::Var ScriptableHandle::GetProperty(const pp::Var& name,
                                      pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptableHandle::GetProperty (name=%s)\n",
                 name.DebugString().c_str()));
  if (plugin_ == NULL) {
    return pp::Var();
  }
  pp::Var property = Invoke(plugin_,
                            PROPERTY_GET,
                            NameAsString(name),
                            "GetProperty",
                            std::vector<pp::Var>(), exception);
  PLUGIN_PRINTF(("ScriptableHandle::GetProperty (property=%s)\n",
                 property.DebugString().c_str()));
  return property;
}


void ScriptableHandle::SetProperty(const pp::Var& name,
                                   const pp::Var& value,
                                   pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptableHandle::SetProperty (name=%s, value=%s)\n",
                 name.DebugString().c_str(), value.DebugString().c_str()));
  if (plugin_ == NULL) {
    return;
  }
  std::vector<pp::Var> args;
  args.push_back(pp::Var(pp::Var::DontManage(), value.pp_var()));
  Invoke(plugin_,
         PROPERTY_SET,
         NameAsString(name),
         "SetProperty",
         args,
         exception);
  std::string exception_string("NULL");
  if (exception != NULL) {
    exception_string = exception->DebugString();
  }
  PLUGIN_PRINTF(("ScriptableHandle::SetProperty (exception=%s)\n",
                 exception_string.c_str()));
}


void ScriptableHandle::RemoveProperty(const pp::Var& name,
                                      pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptableHandle::RemoveProperty (name=%s)\n",
                 name.DebugString().c_str()));
  Error(NameAsString(name), "RemoveProperty",
        "property removal is not supported", exception);
}

// TODO(polina): should methods also be added?
// This is currently never called and the exact semantics is not clear.
// http://code.google.com/p/chromium/issues/detail?id=51089
void ScriptableHandle::GetAllPropertyNames(std::vector<pp::Var>* properties,
                                           pp::Var* exception) {
  UNREFERENCED_PARAMETER(exception);
  PLUGIN_PRINTF(("ScriptableHandle::GetAllPropertyNames ()\n"));
  if (plugin_ == NULL) {
    return;
  }
  std::vector<uintptr_t>* ids = plugin_->GetPropertyIdentifiers();
  if (ids == NULL) {
    PLUGIN_PRINTF(("ScriptableHandle::GetAllPropertyNames "
                   "(ids=%p)\n", reinterpret_cast<void*>(ids)));
    return;
  }
  PLUGIN_PRINTF(("ScriptableHandle::GetAllPropertyNames "
                 "(ids->size()=%"NACL_PRIuS")\n", ids->size()));
  for (size_t i = 0; i < ids->size(); ++i) {
    nacl::string name =
        plugin_->browser_interface()->IdentifierToString(ids->at(i));
    properties->push_back(pp::Var(name));
  }
  PLUGIN_PRINTF(("ScriptableHandle::GetAllPropertyNames "
                 "(properties=%"NACL_PRIuS")\n", properties->size()));
}


pp::Var ScriptableHandle::Call(const pp::Var& name,
                               const std::vector<pp::Var>& args,
                               pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptableHandle::Call (name=%s, %"NACL_PRIuS
                 " args)\n", name.DebugString().c_str(), args.size()));
  if (plugin_ == NULL) {
    pp::Var();
  }
  if (name.is_undefined())  // invoke default
    return pp::Var();
  assert(name.is_string());
  pp::Var return_var = Invoke(plugin_,
                              METHOD_CALL,
                              name.AsString(),
                              "Call",
                              args,
                              exception);
  PLUGIN_PRINTF(("ScriptableHandle::Call (return=%s)\n",
                 return_var.DebugString().c_str()));
  return return_var;
}


pp::Var ScriptableHandle::Construct(const std::vector<pp::Var>& args,
                                    pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptableHandle::Construct (%"NACL_PRIuS
                 " args)\n", args.size()));
  return Error("constructor", "Construct", "constructor is not supported",
               exception);
}


ScriptableHandle* ScriptableHandle::AddRef() {
  // This is called when we are about to share this object with the browser,
  // and we need to make sure we have an internal plugin reference, so this
  // object doesn't get deallocated when the browser discards its references.
  if (var_ == NULL) {
    var_ = new(std::nothrow) pp::VarPrivate(plugin_, this);
    CHECK(var_ != NULL);
  }
  PLUGIN_PRINTF(("ScriptableHandle::AddRef (this=%p, var=%p)\n",
                 static_cast<void*>(this), static_cast<void*>(var_)));
  return this;
}


void ScriptableHandle::Unref() {
  // We should have no more than one internal owner of this object, so this
  // should be called no more than once.
  CHECK(++num_unref_calls_ == 1);
  PLUGIN_PRINTF(("ScriptableHandle::Unref (this=%p, var=%p)\n",
                 static_cast<void*>(this), static_cast<void*>(var_)));
  if (var_ != NULL) {
    // We have shared this with the browser while keeping our own var
    // reference, but we no longer need ours. If the browser has copies,
    // it will clean things up later, otherwise this object will get
    // deallocated right away.
    PLUGIN_PRINTF(("ScriptableHandle::Unref (delete var)\n"));
    pp::Var* var = var_;
    var_ = NULL;
    delete var;
  } else {
    // Neither the browser nor plugin ever var referenced this object,
    // so it can safely discarded.
    PLUGIN_PRINTF(("ScriptableHandle::Unref (delete this)\n"));
    CHECK(var_ == NULL);
    delete this;
  }
}


}  // namespace plugin
