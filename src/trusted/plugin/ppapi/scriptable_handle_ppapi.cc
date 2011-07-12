// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <sstream>
#include <string>
#include <vector>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/trusted/plugin/method_map.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/portable_handle.h"
#include "native_client/src/trusted/plugin/ppapi/array_ppapi.h"
#include "native_client/src/trusted/plugin/ppapi/browser_interface_ppapi.h"
#include "native_client/src/trusted/plugin/ppapi/plugin_ppapi.h"
#include "native_client/src/trusted/plugin/ppapi/scriptable_handle_ppapi.h"
#include "native_client/src/trusted/plugin/ppapi/var_utils.h"

namespace plugin {

namespace {

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
  PLUGIN_PRINTF(("ScriptableHandlePpapi::%s (%s)\n", caller, e));
  *exception = pp::Var(e);
  return pp::Var();
}

}  // namespace

ScriptableHandlePpapi* ScriptableHandlePpapi::New(PortableHandle* handle) {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::New (portable_handle=%p)\n",
                 static_cast<void*>(handle)));
  if (handle == NULL) {
    return NULL;
  }
  ScriptableHandlePpapi* scriptable_handle =
      new(std::nothrow) ScriptableHandlePpapi(handle);
  if (scriptable_handle == NULL) {
    return NULL;
  }
  scriptable_handle->set_handle(handle);
  PLUGIN_PRINTF(("ScriptableHandlePpapi::New (return %p)\n",
                 static_cast<void*>(scriptable_handle)));
  return scriptable_handle;
}


bool ScriptableHandlePpapi::HasCallType(CallType call_type,
                                        nacl::string call_name,
                                        const char* caller) {
  uintptr_t id = handle()->browser_interface()->StringToIdentifier(call_name);
  PLUGIN_PRINTF(("ScriptableHandlePpapi::%s (id=%"NACL_PRIxPTR")\n",
                 caller, id));
  return handle()->HasMethod(id, call_type);
}


bool ScriptableHandlePpapi::HasProperty(const pp::Var& name,
                                        pp::Var* exception) {
  UNREFERENCED_PARAMETER(exception);
  PLUGIN_PRINTF(("ScriptableHandlePpapi::HasProperty (this=%p, name=%s)\n",
                 static_cast<void*>(this), name.DebugString().c_str()));
  if (!name.is_string() && !name.is_int())
    return false;
  bool has_property =
      HasCallType(PROPERTY_GET, NameAsString(name), "HasProperty");
  PLUGIN_PRINTF(("ScriptableHandlePpapi::HasProperty (has_property=%d)\n",
                 has_property));
  return has_property;
}


bool ScriptableHandlePpapi::HasMethod(const pp::Var& name, pp::Var* exception) {
  UNREFERENCED_PARAMETER(exception);
  PLUGIN_PRINTF(("ScriptableHandlePpapi::HasMethod (this=%p, name='%s')\n",
                 static_cast<void*>(this), name.DebugString().c_str()));
  if (!name.is_string())
    return false;
  bool has_method =
      HasCallType(METHOD_CALL, name.AsString(), "HasMethod");
  PLUGIN_PRINTF(("ScriptableHandlePpapi::HasMethod (has_method=%d)\n",
                 has_method));
  return has_method;
}


pp::Var ScriptableHandlePpapi::Invoke(CallType call_type,
                                      nacl::string call_name,
                                      const char* caller,
                                      const std::vector<pp::Var>& args,
                                      pp::Var* exception) {
  uintptr_t id =
      handle()->browser_interface()->StringToIdentifier(call_name);

  // Initialize input/output parameters.
  SrpcParams params;
  NaClSrpcArg** inputs = params.ins();
  NaClSrpcArg** outputs = params.outs();
  if (!handle()->InitParams(id, call_type, &params)) {
    return Error(call_name, caller,
                 "srpc parameter initialization failed", exception);
  }
  uint32_t input_length = params.InputLength();
  int32_t output_length = params.OutputLength();
  PLUGIN_PRINTF(("ScriptableHandlePpapi::%s (initialized %"NACL_PRIu32" ins, %"
                 NACL_PRIu32" outs)\n", caller, input_length, output_length));

  // Verify input/output parameter list length.
  if (args.size() != params.SignatureLength()) {
    return Error(call_name, caller,
                 "incompatible srpc parameter list", exception);
  }
  PLUGIN_PRINTF(("ScriptableHandlePpapi::%s (verified signature)\n", caller));

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
  PLUGIN_PRINTF(("ScriptableHandlePpapi::%s (marshalled inputs)\n", caller));

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
  PLUGIN_PRINTF(("ScriptableHandlePpapi::%s (output array allocation done)\n",
                 caller));

  // Invoke.
  if (!handle()->Invoke(id, call_type, &params)) {
    nacl::string err = nacl::string(caller) + "('" + call_name + "') failed\n";
    if (params.exception_string() != NULL) {
      err = params.exception_string();
    }
    *exception = pp::Var(err.c_str());
    return Error(call_name, caller, "invocation failed", exception);
  }
  PLUGIN_PRINTF(("ScriptableHandlePpapi::%s (invocation done)\n", caller));

  // Marshall output parameters.
  pp::Var retvar;
  PluginPpapi* plugin_ppapi = static_cast<PluginPpapi*>(handle()->plugin());
  if (output_length > 0) {
    assert(call_type != PROPERTY_SET);  // expect no outputs for "set"
    retvar = NaClSrpcArgToPPVar(outputs[0], plugin_ppapi, exception);
    if (output_length > 1) {
      ArrayPpapi* array = new(std::nothrow) ArrayPpapi(plugin_ppapi);
      if (array == NULL) {
        *exception = pp::Var("failed to allocate output array");
      } else {
        array->SetProperty(pp::Var(0), retvar, exception);
        for (int32_t i = 1; i < output_length; ++i) {
          pp::Var v = NaClSrpcArgToPPVar(outputs[i], plugin_ppapi, exception);
          array->SetProperty(pp::Var(i), v, exception);
        }
      }

      retvar = pp::VarPrivate(plugin_ppapi, array);
    }
    if (!exception->is_undefined()) {
      return Error(call_name, caller, "srpc output marshalling failed",
                   exception);
    }
  }
  if (call_type == PROPERTY_GET) assert(output_length == 1);
  return retvar;
}


pp::Var ScriptableHandlePpapi::GetProperty(const pp::Var& name,
                                           pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::GetProperty (name=%s)\n",
                 name.DebugString().c_str()));
  pp::Var property =
      Invoke(PROPERTY_GET, NameAsString(name), "GetProperty",
             std::vector<pp::Var>(), exception);
  PLUGIN_PRINTF(("ScriptableHandlePpapi::GetProperty (property=%s)\n",
                 property.DebugString().c_str()));
  return property;
}


void ScriptableHandlePpapi::SetProperty(const pp::Var& name,
                                        const pp::Var& value,
                                        pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::SetProperty (name=%s, value=%s)\n",
                 name.DebugString().c_str(), value.DebugString().c_str()));
  std::vector<pp::Var> args;
  args.push_back(pp::Var(pp::Var::DontManage(), value.pp_var()));
  Invoke(PROPERTY_SET, NameAsString(name), "SetProperty", args, exception);
  std::string exception_string("NULL");
  if (exception != NULL) {
    exception_string = exception->DebugString();
  }
  PLUGIN_PRINTF(("ScriptableHandlePpapi::SetProperty (exception=%s)\n",
                 exception_string.c_str()));
}


void ScriptableHandlePpapi::RemoveProperty(const pp::Var& name,
                                           pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::RemoveProperty (name=%s)\n",
                 name.DebugString().c_str()));
  Error(NameAsString(name), "RemoveProperty",
        "property removal is not supported", exception);
}

// TODO(polina): should methods also be added?
// This is currently never called and the exact semantics is not clear.
// http://code.google.com/p/chromium/issues/detail?id=51089
void ScriptableHandlePpapi::GetAllPropertyNames(
    std::vector<pp::Var>* properties,
    pp::Var* exception) {
  UNREFERENCED_PARAMETER(exception);
  PLUGIN_PRINTF(("ScriptableHandlePpapi::GetAllPropertyNames ()\n"));
  std::vector<uintptr_t>* ids = handle()->GetPropertyIdentifiers();
  PLUGIN_PRINTF(("ScriptableHandlePpapi::GetAllPropertyNames "
                 "(ids=%"NACL_PRIuS")\n", ids->size()));
  for (size_t i = 0; i < ids->size(); ++i) {
    nacl::string name =
        handle()->browser_interface()->IdentifierToString(ids->at(i));
    properties->push_back(pp::Var(name));
  }
  PLUGIN_PRINTF(("ScriptableHandlePpapi::GetAllPropertyNames "
                 "(properties=%"NACL_PRIuS")\n", properties->size()));
}


pp::Var ScriptableHandlePpapi::Call(const pp::Var& name,
                                    const std::vector<pp::Var>& args,
                                    pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::Call (name=%s, %"NACL_PRIuS
                 " args)\n", name.DebugString().c_str(), args.size()));
  if (name.is_undefined())  // invoke default
    return pp::Var();
  assert(name.is_string());
  pp::Var return_var =
      Invoke(METHOD_CALL, name.AsString(), "Call", args, exception);
  PLUGIN_PRINTF(("ScriptableHandlePpapi::Call (return=%s)\n",
                 return_var.DebugString().c_str()));
  return return_var;
}


pp::Var ScriptableHandlePpapi::Construct(const std::vector<pp::Var>& args,
                                         pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::Construct (%"NACL_PRIuS
                 " args)\n", args.size()));
  return Error("constructor", "Construct", "constructor is not supported",
               exception);
}


ScriptableHandle* ScriptableHandlePpapi::AddRef() {
  // This is called when we are about to share this object with the browser,
  // and we need to make sure we have an internal plugin reference, so this
  // object doesn't get deallocated when the browser discards its references.
  if (var_ == NULL) {
    PluginPpapi* plugin_ppapi = static_cast<PluginPpapi*>(handle()->plugin());
    var_ = new(std::nothrow) pp::VarPrivate(plugin_ppapi, this);
    CHECK(var_ != NULL);
  }
  PLUGIN_PRINTF(("ScriptableHandlePpapi::AddRef (this=%p, var=%p)\n",
                 static_cast<void*>(this), static_cast<void*>(var_)));
  return this;
}


void ScriptableHandlePpapi::Unref() {
  // We should have no more than one internal owner of this object, so this
  // should be called no more than once.
  CHECK(++num_unref_calls_ == 1);
  PLUGIN_PRINTF(("ScriptableHandlePpapi::Unref (this=%p, var=%p)\n",
                 static_cast<void*>(this), static_cast<void*>(var_)));
  if (var_ != NULL) {
    // We have shared this with the browser while keeping our own var
    // reference, but we no longer need ours. If the browser has copies,
    // it will clean things up later, otherwise this object will get
    // deallocated right away.
    PLUGIN_PRINTF(("ScriptableHandlePpapi::Unref (delete var)\n"));
    pp::Var* var = var_;
    var_ = NULL;
    delete var;
  } else {
    // Neither the browser nor plugin ever var referenced this object,
    // so it can safely discarded.
    PLUGIN_PRINTF(("ScriptableHandlePpapi::Unref (delete this)\n"));
    CHECK(var_ == NULL);
    delete this;
  }
}


ScriptableHandlePpapi::ScriptableHandlePpapi(PortableHandle* handle)
    : ScriptableHandle(handle), var_(NULL), num_unref_calls_(0) {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::ScriptableHandlePpapi "
                 "(this=%p, handle=%p)\n",
                 static_cast<void*>(this), static_cast<void*>(handle)));
  handle_is_plugin_ = (handle == handle->plugin());
  PLUGIN_PRINTF(("ScriptableHandlePpapi::ScriptableHandlePpapi "
                 "(this=%p, handle_is_plugin=%d)\n",
                 static_cast<void*>(this), handle_is_plugin_));
}


ScriptableHandlePpapi::~ScriptableHandlePpapi() {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::~ScriptableHandlePpapi "
                 "(this=%p, handle_is_plugin=%d)\n",
                 static_cast<void*>(this), handle_is_plugin_));
  // If handle is a plugin, the browser is deleting it (and might have
  // already done so). Otherwise, delete here.
  if (!handle_is_plugin_) {
    PLUGIN_PRINTF(("ScriptableHandlePpapi::~ScriptableHandlePpapi "
                   "(this=%p, delete handle=%p)\n",
                   static_cast<void*>(this), static_cast<void*>(handle())));
    handle()->Delete();
    set_handle(NULL);
  }
  PLUGIN_PRINTF(("ScriptableHandlePpapi::~ScriptableHandlePpapi "
                 "(this=%p, return)\n",
                 static_cast<void*>(this)));
}

}  // namespace plugin
