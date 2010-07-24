/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include <assert.h>
#include <string>
#include <vector>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/plugin/method_map.h"
#include "native_client/src/trusted/plugin/plugin.h"
#include "native_client/src/trusted/plugin/portable_handle.h"
#include "native_client/src/trusted/plugin/ppapi/browser_interface_ppapi.h"
#include "native_client/src/trusted/plugin/ppapi/scriptable_handle_ppapi.h"
#include "native_client/src/trusted/plugin/ppapi/var_utils.h"

namespace plugin {


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
  if (name.is_int()) {
    PLUGIN_PRINTF(("ScriptableHandlePpapi::HasProperty: unsupported int\n"));
    NACL_UNIMPLEMENTED();
    return false;
  }
  UNREFERENCED_PARAMETER(exception);
  return HasCallType(PROPERTY_GET, name.AsString(), "HasProperty");
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
  PLUGIN_PRINTF(("ScriptableHandlePpapi::%s (id=%"NACL_PRIxPTR")\n",
                 caller, id));

  // Initialize input/output parameters.
  SrpcParams params;
  if (!handle()->InitParams(id, PROPERTY_GET, &params)) {
    const char* error = "SRPC parameter initialization failed";
    *exception = pp::Var(error);
    PLUGIN_PRINTF(("ScriptableHandlePpapi::%s (%s)\n", caller, error));
    return pp::Var();
  }
  PLUGIN_PRINTF(("ScriptableHandlePpapi::%s (param init done)\n", caller));

  // Marshall input parameters.
  int input_length = params.InputLength();
  if (input_length > 0) {
    PLUGIN_PRINTF(("ScriptableHandlePpapi::%s (marshall inputs)\n", caller));
    // Expect no args for "get" and 1 arg for "set".
    assert(call_type != PROPERTY_GET);
    if (call_type == PROPERTY_SET) assert(input_length == 1);

    NaClSrpcArg** inputs = params.ins();
    for (int i = 0; (i < NACL_SRPC_MAX_ARGS) && (inputs[i] != NULL); ++i) {
      PPVarToNaClSrpcArg(args[i], inputs[i]);
    }
  }

  // Invoke.
  if (!handle()->Invoke(id, call_type, &params)) {
    if (params.exception_string() == NULL) {
      nacl::string message =
          nacl::string(caller) + "(\"" + call_name + "\") failed\n";
      *exception = pp::Var(message.c_str());
    } else {
      *exception = pp::Var(params.exception_string());
    }
    PLUGIN_PRINTF(("ScriptableHandlePpapi::%s (invocation failed)\n", caller));
    return pp::Var();
  }
  PLUGIN_PRINTF(("ScriptableHandlePpapi::%s (invocation done)\n", caller));

  // Marshall output parameters.
  int output_length = params.OutputLength();
  if (output_length > 0) {
    PLUGIN_PRINTF(("ScriptableHandlePpapi::%s (marshall outputs)\n", caller));
    // Expect no returns for "set" and 1 return for "get".
    assert(call_type != PROPERTY_SET);
    if (call_type != PROPERTY_GET) assert(output_length == 1);

    NaClSrpcArg** outputs = params.outs();
    pp::Var retvar = NaClSrpcArgToPPVar(outputs[0]);
    if (output_length > 1) {  // TODO(polina): use an array for multiple outputs
      NACL_UNIMPLEMENTED();
    }
    if (retvar.is_void()) {
      *exception = pp::Var("SRPC output marshalling failed");
    }
    PLUGIN_PRINTF(("ScriptableHandlePpapi::%s (return %s)\n", caller,
                   VarToString(retvar).c_str()));
    return retvar;
  }

  assert(call_type != PROPERTY_GET);
  PLUGIN_PRINTF(("ScriptableHandlePpapi::%s (return VOID)\n", caller));
  return pp::Var();
}


pp::Var ScriptableHandlePpapi::GetProperty(const pp::Var& name,
                                           pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::GetProperty (name=%s)\n",
                 VarToString(name).c_str()));
  if (name.is_int()) {
    PLUGIN_PRINTF(("ScriptableHandlePpapi::GetProperty: unsupported int\n"));
    NACL_UNIMPLEMENTED();
    return false;
  }
  return Invoke(PROPERTY_GET, name.AsString(), "GetProperty",
                std::vector<pp::Var>(),
                exception);
}


void ScriptableHandlePpapi::SetProperty(const pp::Var& name,
                                        const pp::Var& value,
                                        pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::SetProperty (name=%s, value=%s)\n",
                 VarToString(name).c_str(), VarToString(value).c_str()));
  if (name.is_int()) {
    PLUGIN_PRINTF(("ScriptableHandlePpapi::SetProperty: unsupported int\n"));
    NACL_UNIMPLEMENTED();
    return;
  }
  std::vector<pp::Var> args;
  args.push_back(value);
  Invoke(PROPERTY_SET, name.AsString(), "SetProperty", args, exception);
}


void ScriptableHandlePpapi::RemoveProperty(const pp::Var& name,
                                           pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::RemoveProperty (name=%s)\n",
                 VarToString(name).c_str()));
  *exception = pp::Var("Property removal is not supported");
}


pp::Var ScriptableHandlePpapi::Call(const pp::Var& name,
                                    const std::vector<pp::Var>& args,
                                    pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::SetProperty (name=%s, %"NACL_PRIdS
                 " args)\n", VarToString(name).c_str(), args.size()));
  return Invoke(METHOD_CALL, name.AsString(), "Call", args, exception);
}


pp::Var ScriptableHandlePpapi::Construct(const std::vector<pp::Var>& args,
                                         pp::Var* exception) {
  PLUGIN_PRINTF(("ScriptableHandlePpapi::Construct (%"NACL_PRIdS
                 " args)\n", args.size()));
  *exception = pp::Var("Constructor is not supported");
  return pp::Var();
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
