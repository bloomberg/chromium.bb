/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/tests/fake_browser_ppapi/fake_window.h"
#include <string.h>
#include <map>
#include <string>

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/ppapi_proxy/plugin_var.h"
#include "native_client/tests/fake_browser_ppapi/fake_object.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/pp_var.h"

using fake_browser_ppapi::Object;
using ppapi_proxy::PluginVar;

namespace {

PP_Var* NewStringVar(PP_Module browser_module, const char* str) {
  static const PPB_Var_Deprecated* ppb_var = NULL;
  if (ppb_var == NULL) {
    ppb_var = reinterpret_cast<const PPB_Var_Deprecated*>(
        PluginVar::GetInterface());
    if (ppb_var == NULL) {
      return NULL;
    }
  }

  PP_Var* var = reinterpret_cast<PP_Var*>(malloc(sizeof(*var)));
  *var = ppb_var->VarFromUtf8(browser_module,
                              str,
                              nacl::assert_cast<uint32_t>(strlen(str)));
  return var;
}

// Returns a PP_Var that mocks the window.location object.
PP_Var* LocationObject(PP_Module browser_module, const char* page_url) {
  // Populate the properties map.
  PP_Var* href = NewStringVar(browser_module, page_url);
  Object::PropertyMap properties;
  properties["href"] = href;

  // Populate the methods map.
  Object::MethodMap methods;

  // Create and return a PP_Var for location.
  PP_Var* location = reinterpret_cast<PP_Var*>(malloc(sizeof(*location)));
  *location = Object::New(browser_module, properties, methods);
  return location;
}

// Emulates the window.console.log method.
PP_Var ConsoleLog(Object* object,
                  uint32_t argc,
                  PP_Var* argv,
                  PP_Var* exception) {
  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(exception);
  printf("console.log(");
  for (uint32_t i = 0; i < argc; ++i) {
    // NB: currently we are not doing the printf-style formatting.
    // TODO(sehr): implement the formatting.
    printf("'%s'", PluginVar::DebugString(argv[i]).c_str());
    if (i < argc - 1) {
      printf(", ");
    }
  }
  printf(")\n");
  return PP_MakeUndefined();
}

// Returns a PP_Var that mocks the window.console object.
PP_Var* ConsoleObject(PP_Module browser_module) {
  // Populate the properties map.
  Object::PropertyMap properties;

  // Populate the methods map.
  Object::MethodMap methods;
  methods["log"] = ConsoleLog;

  PP_Var* console = reinterpret_cast<PP_Var*>(malloc(sizeof(*console)));
  *console = Object::New(browser_module, properties, methods);
  return console;
}

// Emulates the window.alert method.
PP_Var Alert(Object* object,
             uint32_t argc,
             PP_Var* argv,
             PP_Var* exception) {
  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(exception);
  printf("window.alert(");
  if (argc == 1) {
    printf("'%s'", PluginVar::DebugString(argv[0]).c_str());
  } else {
    printf("<BAD PARAMETER COUNT: %d>", argc);
  }
  printf(")\n");
  return PP_MakeUndefined();
}

}  // namespace

namespace fake_browser_ppapi {

FakeWindow::FakeWindow(PP_Module browser_module,
                       Host* host,
                       const char* page_url) : host_(host) {
  // Populate the properties map.
  Object::PropertyMap properties;
  properties["console"] = ConsoleObject(browser_module);
  properties["location"] = LocationObject(browser_module, page_url);
  // Populate the methods map.
  Object::MethodMap methods;
  methods["alert"] = Alert;
  Object* window_object = new Object(browser_module, properties, methods);
  window_var_ =
      host_->var_interface()->CreateObject(browser_module,
                                           &ppapi_proxy::Object::object_class,
                                           window_object);
}

FakeWindow::~FakeWindow() {
  host_->var_interface()->Release(window_var_);
}

PP_Var FakeWindow::FakeWindowObject() {
  host_->var_interface()->AddRef(window_var_);
  return window_var_;
}

}  // namespace fake_browser_ppapi
