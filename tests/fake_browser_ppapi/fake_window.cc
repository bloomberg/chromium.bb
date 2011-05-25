/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/tests/fake_browser_ppapi/fake_window.h"

#include <stdio.h>
#include <string.h>

#include <map>
#include <string>

#include "native_client/src/include/checked_cast.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/ppapi_proxy/plugin_ppb_var_deprecated.h"
#include "native_client/tests/fake_browser_ppapi/fake_object.h"
#include "native_client/tests/fake_browser_ppapi/utility.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/pp_var.h"

using fake_browser_ppapi::Object;
using ppapi_proxy::PluginVarDeprecated;

namespace {

PP_Module g_browser_module;
PP_Instance g_browser_instance;

PP_Var* NewStringVar(PP_Module browser_module, const char* str) {
  static const PPB_Var_Deprecated* ppb_var = NULL;
  if (ppb_var == NULL) {
    ppb_var = reinterpret_cast<const PPB_Var_Deprecated*>(
        PluginVarDeprecated::GetInterface());
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
PP_Var* LocationObject(PP_Module browser_module,
                       PP_Instance browser_instance,
                       const char* page_url) {
  // Populate the properties map.
  PP_Var* href = NewStringVar(browser_module, page_url);
  Object::PropertyMap properties;
  properties["href"] = href;

  // Populate the methods map.
  Object::MethodMap methods;

  // Create and return a PP_Var for location.
  PP_Var* location = reinterpret_cast<PP_Var*>(malloc(sizeof(*location)));
  *location = Object::New(browser_module, browser_instance,
                          properties, methods);
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
    printf("'%s'", PluginVarDeprecated::DebugString(argv[i]).c_str());
    if (i < argc - 1) {
      printf(", ");
    }
  }
  printf(")\n");
  return PP_MakeUndefined();
}

// Returns a PP_Var that mocks the window.console object.
PP_Var* ConsoleObject(PP_Module browser_module, PP_Instance browser_instance) {
  // Populate the properties map.
  Object::PropertyMap properties;

  // Populate the methods map.
  Object::MethodMap methods;
  methods["log"] = ConsoleLog;

  PP_Var* console = reinterpret_cast<PP_Var*>(malloc(sizeof(*console)));
  *console = Object::New(browser_module, browser_instance, properties, methods);
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
    printf("'%s'", PluginVarDeprecated::DebugString(argv[0]).c_str());
  } else {
    printf("<BAD PARAMETER COUNT: %d>", argc);
  }
  printf(")\n");
  return PP_MakeUndefined();
}

std::string GetNexeURL(const char* nexes, const char* isa) {
  const char* match = strstr(nexes, isa);
  if (match == NULL) {
    return std::string("");
  }
  const char* start = match + strlen(isa);
  const char* end = strchr(start, '"');
  return std::string(start, end - start);
}

// Returns a PP_Var that mocks the nexes dictionary.
PP_Var* NexesObject(PP_Module browser_module,
                    PP_Instance browser_instance,
                    const char* nexes) {
  // Populate the properties map.
  Object::PropertyMap properties;
  properties["x86-32"] =
      NewStringVar(browser_module, GetNexeURL(nexes, "\"x86-32\": \"").c_str());
  properties["x86-64"] =
      NewStringVar(browser_module, GetNexeURL(nexes, "\"x86-64\": \"").c_str());
  properties["arm"] =
      NewStringVar(browser_module, GetNexeURL(nexes, "\"arm\": \"").c_str());


  // Populate the methods map.
  Object::MethodMap methods;

  PP_Var* nexes_object =
      reinterpret_cast<PP_Var*>(malloc(sizeof(*nexes_object)));
  *nexes_object =
      Object::New(browser_module, browser_instance, properties, methods);
  return nexes_object;
}

// Emulates the window.JSON.parse method.
PP_Var Parse(Object* object,
             uint32_t argc,
             PP_Var* argv,
             PP_Var* exception) {
  UNREFERENCED_PARAMETER(object);
  UNREFERENCED_PARAMETER(exception);
  printf("window.JSON.parse(");
  if (argc == 1) {
    printf("'%s')\n", PluginVarDeprecated::DebugString(argv[0]).c_str());
  } else {
    printf("<BAD PARAMETER COUNT: %d>)\n", argc);
    return PP_MakeUndefined();
  }
  // Build the nexes object from the json string.
  // Populate the properties map.
  Object::PropertyMap properties;
  properties["nexes"] =
      NexesObject(g_browser_module,
                  g_browser_instance,
                  PluginVarDeprecated::DebugString(argv[0]).c_str());

  // Populate the methods map.
  Object::MethodMap methods;

  return Object::New(g_browser_module, g_browser_instance, properties, methods);
}

// Returns a PP_Var that mocks the window.JSON object.
PP_Var* JsonParserObject(PP_Module browser_module,
                         PP_Instance browser_instance) {
  // Populate the properties map.
  Object::PropertyMap properties;

  // Populate the methods map.
  Object::MethodMap methods;
  methods["parse"] = Parse;

  PP_Var* console = reinterpret_cast<PP_Var*>(malloc(sizeof(*console)));
  *console = Object::New(browser_module, browser_instance, properties, methods);
  return console;
}

}  // namespace

namespace fake_browser_ppapi {

FakeWindow::FakeWindow(PP_Module browser_module,
                       PP_Instance browser_instance,
                       Host* host,
                       const char* page_url) : host_(host) {
  // Populate the properties map.
  Object::PropertyMap properties;
  properties["console"] = ConsoleObject(browser_module, browser_instance);
  properties["location"] = LocationObject(browser_module, browser_instance,
                                          page_url);
  properties["JSON"] = JsonParserObject(browser_module, browser_instance);
  // Populate the methods map.
  Object::MethodMap methods;
  methods["alert"] = Alert;
  Object* window_object = new Object(browser_module, browser_instance,
                                     properties, methods);
  window_var_ =
      host_->var_interface()->CreateObject(browser_instance,
                                           &ppapi_proxy::Object::object_class,
                                           window_object);
  g_browser_module = browser_module;
  g_browser_instance = browser_instance;
}

FakeWindow::~FakeWindow() {
  host_->var_interface()->Release(window_var_);
}

PP_Var FakeWindow::FakeWindowObject() {
  host_->var_interface()->AddRef(window_var_);
  return window_var_;
}

}  // namespace fake_browser_ppapi
