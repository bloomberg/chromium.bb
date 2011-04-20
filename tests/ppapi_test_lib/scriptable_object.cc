// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This implements the scripting interface of the plugin module instance.
// The interface is used to drive tests and analyze results from JavaScript.
//

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/ppapi_test_lib/get_browser_interface.h"
#include "native_client/tests/ppapi_test_lib/internal_utils.h"
#include "native_client/tests/ppapi_test_lib/test_interface.h"

#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppp_class_deprecated.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_url_request_info.h"
#include "ppapi/c/pp_completion_callback.h"
#include "ppapi/c/pp_instance.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_var.h"

///////////////////////////////////////////////////////////////////////////////
// PPP_Class_Deprecated implementation
///////////////////////////////////////////////////////////////////////////////

namespace {

// __moduleReady is used by our test harness to determine if user module has
// loaded and is ready for testing.
bool HasProperty(void* object, PP_Var name, PP_Var* exception) {
  uint32_t len = 0;
  const char* property_name = PPBVarDeprecated()->VarToUtf8(name, &len);
  return (0 == strncmp(property_name, "__moduleReady", len));
}

bool HasMethod(void* object, PP_Var name_var, PP_Var* /*exception*/) {
  // Intercept all methods and let Call() handle the rest.
  uint32_t len = 0;
  const char* method_name = PPBVarDeprecated()->VarToUtf8(name_var, &len);
  nacl::string name(method_name, len);
  return HasScriptableTest(name);
}

PP_Var GetProperty(void* object, PP_Var name, PP_Var* exception) {
  uint32_t len = 0;
  const char* property_name = PPBVarDeprecated()->VarToUtf8(name, &len);
  if (0 == strncmp(property_name, "__moduleReady", len))
    return PP_MakeInt32(1);
  return PP_MakeUndefined();
}

void GetAllPropertyNames(void* /*object*/,
                         uint32_t* property_count,
                         PP_Var** /*properties*/,
                         PP_Var* /*exception*/) {
  *property_count = 0;
}

void SetProperty(void* /*object*/,
                 PP_Var /*name*/,
                 PP_Var /*value*/,
                 PP_Var* /*exception*/) {
}

void RemoveProperty(void* /*object*/,
                    PP_Var /*name*/,
                    PP_Var* /*exception*/) {
}

PP_Var Call(void* object,
            PP_Var method_name,
            uint32_t argc,
            PP_Var* argv,
            PP_Var* exception) {
  uint32_t len;
  const char* name = PPBVarDeprecated()->VarToUtf8(method_name, &len);
  return RunScriptableTest(name);
}

PP_Var Construct(void* /*object*/,
                 uint32_t /*argc*/,
                 PP_Var* /*argv*/,
                 PP_Var* /*exception*/) {
  return PP_MakeUndefined();
}

void Deallocate(void* object) {
}

const PPP_Class_Deprecated ppp_class_interface = {
  HasProperty,
  HasMethod,
  GetProperty,
  GetAllPropertyNames,
  SetProperty,
  RemoveProperty,
  Call,
  Construct,
  Deallocate
};

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// PPB_Var_Deprecated object creation
///////////////////////////////////////////////////////////////////////////////

PP_Var GetScriptableObject(PP_Instance instance) {
  CHECK(ppb_get_interface() != NULL);
  SetupScriptableTests();
  return PPBVarDeprecated()->CreateObject(instance, &ppp_class_interface, NULL);
}
