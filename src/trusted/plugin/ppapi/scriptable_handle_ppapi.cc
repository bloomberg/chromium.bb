/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <assert.h>
#include <sstream>
#include <string>
#include <vector>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/plugin/method_map.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/portable_handle.h"
#include "native_client/src/trusted/plugin/ppapi/browser_interface_ppapi.h"
#include "native_client/src/trusted/plugin/ppapi/plugin_ppapi.h"
#include "native_client/src/trusted/plugin/ppapi/scriptable_handle_ppapi.h"
#include "native_client/src/trusted/plugin/ppapi/var_utils.h"

namespace plugin {

static pp::Var Error(nacl::string call_name, const char* caller,
                     const char* error, pp::Var* exception) {
  nacl::stringstream error_stream;
  error_stream << call_name << ": " << error;
  if (!exception->is_void()) {
    error_stream << " - " + exception->AsString();
  }
  const char* e = error_stream.str().c_str();
  PLUGIN_PRINTF(("ScriptableHandlePpapi::%s (%s)\n", caller, e));
  *exception = pp::Var(e);
  return pp::Var();
}


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
  bool status = handle()->plugin()->HasMethod(id, call_type);
  PLUGIN_PRINTF(("ScriptableHandlePpapi::%s (return %d)\n",
                 caller, status));
  return status;
}


bool ScriptableHandlePpapi::HasProperty(const pp::Var& name,
                                        pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::HasProperty (name=%s)\n",
                 VarToString(name).c_str()));
  assert(name.is_string() || name.is_int());
  UNREFERENCED_PARAMETER(exception);
  return HasCallType(PROPERTY_GET, NameAsString(name), "HasProperty");
}


bool ScriptableHandlePpapi::HasMethod(const pp::Var& name, pp::Var* exception) {
  assert(name.is_string());
  PLUGIN_PRINTF(("ScriptableHandlePpapi::HasMethod (name='%s')\n",
                 name.AsString().c_str()));
  UNREFERENCED_PARAMETER(exception);
  return HasCallType(METHOD_CALL, name.AsString(), "HasMethod");
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
  uint32_t output_length = params.OutputLength();
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
  if (output_length > 0) {
    assert(call_type != PROPERTY_SET);  // expect no outputs for "set"
    retvar = NaClSrpcArgToPPVar(
        outputs[0], static_cast<PluginPpapi*>(handle()->plugin()), exception);
    if (output_length > 1) {  // TODO(polina): use an array for multiple outputs
      NACL_UNIMPLEMENTED();
    }
    if (!exception->is_void()) {
      return Error(call_name, caller, "srpc output marshalling failed",
                   exception);
    }
  }
  if (call_type == PROPERTY_GET) assert(output_length == 1);
  PLUGIN_PRINTF(("ScriptableHandlePpapi::%s (return %s)\n",
                 caller, VarToString(retvar).c_str()));
  return retvar;
}


pp::Var ScriptableHandlePpapi::GetProperty(const pp::Var& name,
                                           pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::GetProperty (name=%s)\n",
                 VarToString(name).c_str()));
  return Invoke(PROPERTY_GET, NameAsString(name), "GetProperty",
                std::vector<pp::Var>(), exception);
}


void ScriptableHandlePpapi::SetProperty(const pp::Var& name,
                                        const pp::Var& value,
                                        pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::SetProperty (name=%s, value=%s)\n",
                 VarToString(name).c_str(), VarToString(value).c_str()));
  std::vector<pp::Var> args;
  args.push_back(pp::Var(pp::Var::DontManage(), value.pp_var()));
  Invoke(PROPERTY_SET, NameAsString(name), "SetProperty", args, exception);
}


void ScriptableHandlePpapi::RemoveProperty(const pp::Var& name,
                                           pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::RemoveProperty (name=%s)\n",
                 VarToString(name).c_str()));
  Error(NameAsString(name), "RemoveProperty",
        "property removal is not supported", exception);
}


pp::Var ScriptableHandlePpapi::Call(const pp::Var& name,
                                    const std::vector<pp::Var>& args,
                                    pp::Var* exception) {
  assert(name.is_string());
  PLUGIN_PRINTF(("ScriptableHandlePpapi::Call (name=%s, %"NACL_PRIdS
                 " args)\n", VarToString(name).c_str(), args.size()));
  return Invoke(METHOD_CALL, name.AsString(), "Call", args, exception);
}


pp::Var ScriptableHandlePpapi::Construct(const std::vector<pp::Var>& args,
                                         pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::Construct (%"NACL_PRIdS
                 " args)\n", args.size()));
  return Error("constructor", "Construct", "constructor is not supported",
               exception);
}


ScriptableHandlePpapi::ScriptableHandlePpapi(PortableHandle* handle)
  : ScriptableHandle(handle) {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::ScriptableHandlePpapi (this=%p, "
                 "handle=%p)\n",
                 static_cast<void*>(this), static_cast<void*>(handle)));
}


ScriptableHandlePpapi::~ScriptableHandlePpapi() {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::~ScriptableHandlePpapi (this=%p)\n",
                 static_cast<void*>(this)));
}

}  // namespace plugin
