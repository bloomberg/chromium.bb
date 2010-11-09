// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Copy of the stub test from ppapi/examples/stub/stub.c
// This is the simplest possible C Pepper plugin that does nothing. If you're
// using C++, you will want to look at stub.cc which uses the more convenient
// C++ wrappers.

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <nacl/nacl_inttypes.h>
#include <map>
#include <string>
#include <utility>
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/dev/ppp_class_deprecated.h"
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_instance.h"


namespace {

PP_Module g_module_id;
PPB_GetInterface g_get_browser_interface = NULL;
const PPB_Var_Deprecated* g_var_interface = NULL;

const PP_Var kGetFailed = PP_MakeNull();
const PP_Var kSetFailed = PP_MakeBool(PP_FALSE);
const PP_Var kCallFailed = PP_MakeBool(PP_FALSE);

// __moduleReady is required for tests that use the nacl_js_lib.js harness.
// This property really should go in the ppruntime somehow.  It used to be
// a "builtin" in the NPAPI runtime, but such builtins do not seem to be
// supported in the Pepper version.
// TODO(dspringer): This property is used by the nacl_js_lib.js code in the
// testing harness.  nacl_js_lib.js should be rewritten to use the onload
// and onfail events (and there should be separate tests for those!).
const char* const kPropModuleReady = "__moduleReady";

const char* const kPropUndefined = "propUndefined";
const char* const kPropNull = "propNull";
const char* const kPropBool = "propBool";
const char* const kPropInt32 = "propInt32";
const char* const kPropDouble = "propDouble";
const char* const kPropString = "propString";
const char* const kPropObject = "propObject";
const char* const kPropWindow = "propWindow";

// Each of these methods takes 1 parameter, which has to be the same type as
// the type named in the method.  E.g. "methodInt32" takes a pp_Var that
// must be int32 type.
const char* const kMethodUndefined = "methodUndefined";
const char* const kMethodNull = "methodNull";
const char* const kMethodBool = "methodBool";
const char* const kMethodInt32 = "methodInt32";
const char* const kMethodDouble = "methodDouble";
const char* const kMethodString = "methodString";
const char* const kMethodObject = "methodObject";

// Each of these methods takes 2 parameters, each has to be the same type as
// the type named in the method.  E.g. "methodInt32With2Args" takes two pp_Vars
// that must both be int32 type.
const char* const kMethodUndefinedWith2Args = "methodUndefinedWith2Args";
const char* const kMethodNullWith2Args = "methodNullWith2Args";
const char* const kMethodBoolWith2Args = "methodBoolWith2Args";
const char* const kMethodInt32With2Args = "methodInt32With2Args";
const char* const kMethodDoubleWith2Args = "methodDoubleWith2Args";
const char* const kMethodStringWith2Args = "methodStringWith2Args";
const char* const kMethodObjectWith2Args = "methodObjectWith2Args";

// This method takes an object as its single argument (should be the |window|
// object in the browser), and returns the value of the |location| property
// on that object.  In the test, this should produce the same object as
// the JavaScript window.location.
const char* const kMethodWindow = "windowLocation";

// This method takes no parameters and returns the string declared by
// |kHelloWorld|, below.
const char* const kMethodHelloWorld = "helloWorld";

const char* const kHelloWorld = "hello, world";
const uint32_t kHelloWorldLength = static_cast<uint32_t>(sizeof(kHelloWorld));

// A printer for PP_Vars.
void PrintPpVar(PP_Var var) {
  printf("PP_Var(");
  switch (var.type) {
    case PP_VARTYPE_UNDEFINED:
      printf("undefined");
      break;
    case PP_VARTYPE_NULL:
      printf("null");
      break;
    case PP_VARTYPE_BOOL:
      printf("bool(%s)", var.value.as_bool ? "true" : "false");
      break;
    case PP_VARTYPE_INT32:
      printf("int32(%d)", static_cast<int>(var.value.as_int));
      break;
    case PP_VARTYPE_DOUBLE:
      printf("double(%f)", var.value.as_double);
      break;
    case PP_VARTYPE_STRING:
      /* SCOPE */ {
        uint32_t len;
        const char* str = g_var_interface->VarToUtf8(var, &len);
        printf("string(\"%*s\")", static_cast<int>(len), str);
      }
      break;
    case PP_VARTYPE_OBJECT:
      printf("object(%d)", static_cast<int>(var.value.as_id));
      break;
    default:
      printf("bad_type(%d)", var.type);
      break;
  }
  printf(")");
}

class TestObject {
 public:
  TestObject();

  bool HasProperty(PP_Var name);
  bool HasMethod(PP_Var name);
  PP_Var GetProperty(PP_Var name);
  void RemoveProperty(PP_Var name);
  bool SetProperty(PP_Var name, PP_Var value);
  PP_Var Call(PP_Var name, uint32_t argc, PP_Var* argv, PP_Var* exception);

 private:
  // Getter/setter/function for PP_VARTYPE_UNDEFINED.
  PP_Var prop_undefined() const { return prop_undefined_; }
  bool set_prop_undefined(PP_Var prop_undefined);
  PP_Var method_undefined(uint32_t argc, PP_Var* argv, PP_Var* exception);
  PP_Var method_undefined_2_args(uint32_t argc,
                                 PP_Var* argv,
                                 PP_Var* exception);
  // Getter/setter/function for PP_VARTYPE_NULL.
  PP_Var prop_null() const { return prop_null_; }
  bool set_prop_null(PP_Var prop_null);
  PP_Var method_null(uint32_t argc, PP_Var* argv, PP_Var* exception);
  PP_Var method_null_2_args(uint32_t argc, PP_Var* argv, PP_Var* exception);
  // Getter/setter/function for PP_VARTYPE_BOOL.
  PP_Var prop_bool() const { return prop_bool_; }
  bool set_prop_bool(PP_Var prop_bool);
  PP_Var method_bool(uint32_t argc, PP_Var* argv, PP_Var* exception);
  PP_Var method_bool_2_args(uint32_t argc, PP_Var* argv, PP_Var* exception);
  // Getter/setter/function for PP_VARTYPE_INT32.
  PP_Var prop_int32() const { return prop_int32_; }
  bool set_prop_int32(PP_Var prop_int32);
  PP_Var method_int32(uint32_t argc, PP_Var* argv, PP_Var* exception);
  PP_Var method_int32_2_args(uint32_t argc, PP_Var* argv, PP_Var* exception);
  // Getter/setter/function for PP_VARTYPE_DOUBLE.
  PP_Var prop_double() const { return prop_double_; }
  bool set_prop_double(PP_Var prop_double);
  PP_Var method_double(uint32_t argc, PP_Var* argv, PP_Var* exception);
  PP_Var method_double_2_args(uint32_t argc, PP_Var* argv, PP_Var* exception);
  // Getter/setter/function for PP_VARTYPE_STRING.
  PP_Var prop_string() const { return prop_string_; }
  bool set_prop_string(PP_Var prop_string);
  PP_Var method_string(uint32_t argc, PP_Var* argv, PP_Var* exception);
  PP_Var method_string_2_args(uint32_t argc, PP_Var* argv, PP_Var* exception);
  // Getter/setter/function for PP_VARTYPE_OBJECT.
  PP_Var prop_object() const { return prop_object_; }
  bool set_prop_object(PP_Var prop_object);
  PP_Var method_object(uint32_t argc, PP_Var* argv, PP_Var* exception);
  PP_Var method_object_2_args(uint32_t argc, PP_Var* argv, PP_Var* exception);
  // Test of scripting the window object.
  PP_Var window_location(uint32_t argc, PP_Var* argv, PP_Var* exception);
  // Test that calls a method on a JS object which is exported from the NaCl
  // module.
  PP_Var hello_world(uint32_t argc, PP_Var* argv, PP_Var* exception);

  PP_Var module_ready() const { return prop_module_ready_; }
  bool set_module_ready(PP_Var module_ready);

  // Properties.
  typedef PP_Var (TestObject::*Getter)() const;
  typedef bool (TestObject::*Setter)(PP_Var val);
  typedef std::pair<Getter, Setter> Property;
  typedef std::map<std::string, Property> PropertyMap;
  // Methods.
  typedef PP_Var (TestObject::*Method)(uint32_t argc,
                                       PP_Var* argv,
                                       PP_Var* exception);
  typedef std::map<std::string, Method> MethodMap;

  PP_Var prop_undefined_;
  PP_Var prop_null_;
  PP_Var prop_bool_;
  PP_Var prop_int32_;
  PP_Var prop_double_;
  PP_Var prop_string_;
  PP_Var prop_object_;
  PP_Var prop_module_ready_;

  static PropertyMap property_map;
  static MethodMap method_map;
  static bool class_is_initialized;
};

TestObject::PropertyMap TestObject::property_map;
TestObject::MethodMap TestObject::method_map;
bool TestObject::class_is_initialized = false;

TestObject::TestObject()
    : prop_undefined_(PP_MakeNull()),
      prop_null_(PP_MakeNull()),
      prop_int32_(PP_MakeNull()),
      prop_double_(PP_MakeNull()),
      prop_string_(PP_MakeNull()),
      prop_object_(PP_MakeNull()),
      prop_module_ready_(PP_MakeBool(PP_TRUE)) {
  if (class_is_initialized) {
    return;
  }
  // Property map initialization.
  // |kPropModuleReady| is used by the nacl_js_lib.js script, and is polled to
  // decide when the NaCl module is loaded.  See TODO at the top of this file.
  property_map[kPropModuleReady]=
      Property(&TestObject::module_ready, &TestObject::set_module_ready);

  property_map[kPropUndefined] =
      Property(&TestObject::prop_undefined, &TestObject::set_prop_undefined);
  property_map[kPropNull] =
      Property(&TestObject::prop_null, &TestObject::set_prop_null);
  property_map[kPropBool] =
      Property(&TestObject::prop_bool, &TestObject::set_prop_bool);
  property_map[kPropInt32] =
      Property(&TestObject::prop_int32, &TestObject::set_prop_int32);
  property_map[kPropDouble] =
      Property(&TestObject::prop_double, &TestObject::set_prop_double);
  property_map[kPropString] =
      Property(&TestObject::prop_string, &TestObject::set_prop_string);
  property_map[kPropObject] =
      Property(&TestObject::prop_object, &TestObject::set_prop_object);
  // Method map initialization.
  method_map[kMethodUndefined] = &TestObject::method_undefined;
  method_map[kMethodNull] = &TestObject::method_null;
  method_map[kMethodBool] = &TestObject::method_bool;
  method_map[kMethodInt32] = &TestObject::method_int32;
  method_map[kMethodDouble] = &TestObject::method_double;
  method_map[kMethodString] = &TestObject::method_string;
  method_map[kMethodObject] = &TestObject::method_object;

  method_map[kMethodUndefinedWith2Args] = &TestObject::method_undefined_2_args;
  method_map[kMethodNullWith2Args] = &TestObject::method_null_2_args;
  method_map[kMethodBoolWith2Args] = &TestObject::method_bool_2_args;
  method_map[kMethodInt32With2Args] = &TestObject::method_int32_2_args;
  method_map[kMethodDoubleWith2Args] = &TestObject::method_double_2_args;
  method_map[kMethodStringWith2Args] = &TestObject::method_string_2_args;
  method_map[kMethodObjectWith2Args] = &TestObject::method_object_2_args;
  method_map[kMethodWindow] = &TestObject::window_location;
  method_map[kMethodHelloWorld] = &TestObject::hello_world;
  class_is_initialized = true;
}

bool TestObject::HasProperty(PP_Var name) {
  printf("basic_object: HasProperty(%p, ", reinterpret_cast<void*>(this));
  PrintPpVar(name);
  printf(")\n");
  uint32_t len;
  const char* str = g_var_interface->VarToUtf8(name, &len);
  if (str == NULL) return false;
  std::string name_str(str, len);
  return property_map.find(name_str) != property_map.end();
}

PP_Var TestObject::GetProperty(PP_Var name) {
  printf("basic_object: GetProperty(%p, ", reinterpret_cast<void*>(this));
  PrintPpVar(name);
  printf(")\n");
  uint32_t len;
  const char* str = g_var_interface->VarToUtf8(name, &len);
  if (str == NULL) return kGetFailed;
  std::string name_str(str, len);
  if (property_map.find(name_str) == property_map.end()) {
    return kGetFailed;
  }
  return (this->*property_map[name_str].first)();
}

bool TestObject::SetProperty(PP_Var name, PP_Var value) {
  printf("basic_object: SetProperty(%p, ", reinterpret_cast<void*>(this));
  PrintPpVar(name);
  printf(", ");
  PrintPpVar(value);
  printf(")\n");
  uint32_t len;
  const char* str = g_var_interface->VarToUtf8(name, &len);
  if (str == NULL) return false;
  std::string name_str(str, len);
  if (property_map.find(name_str) == property_map.end()) {
    return false;
  }
  return (this->*property_map[name_str].second)(value);
}

void TestObject::RemoveProperty(PP_Var name) {
  printf("basic_object: RemoveProperty(%p, ", reinterpret_cast<void*>(this));
  PrintPpVar(name);
  printf(")\n");
  uint32_t len;
  const char* str = g_var_interface->VarToUtf8(name, &len);
  if (str == NULL) return;
  std::string name_str(str, len);
  PropertyMap::iterator iter = property_map.find(name_str);
  if (iter != property_map.end()) {
    property_map.erase(iter);
  }
}

bool TestObject::HasMethod(PP_Var name) {
  printf("basic_object: HasMethod(%p, ", reinterpret_cast<void*>(this));
  PrintPpVar(name);
  printf(")\n");
  uint32_t len;
  const char* str = g_var_interface->VarToUtf8(name, &len);
  if (str == NULL) return false;
  std::string name_str(str, len);
  return method_map.find(name_str) != method_map.end();
}

PP_Var TestObject::Call(PP_Var name,
                        uint32_t argc,
                        PP_Var* argv,
                        PP_Var* exception) {
  printf("basic_object: Call(%p, ", reinterpret_cast<void*>(this));
  PrintPpVar(name);
  printf(", argc=%d, [", static_cast<int>(argc));
  for (uint32_t i = 0; i < argc; ++i) {
    if (i != 0) printf(", ");
    PrintPpVar(argv[i]);
  }
  printf("])\n");
  uint32_t len;
  const char* str = g_var_interface->VarToUtf8(name, &len);
  if (str == NULL) {
    *exception = kCallFailed;
    return kCallFailed;
  }
  std::string name_str(str, len);
  return (this->*method_map[name_str])(argc, argv, exception);
}

bool TestObject::set_prop_undefined(PP_Var prop_undefined) {
  if (prop_undefined.type != PP_VARTYPE_UNDEFINED) return false;
  prop_undefined_ = prop_undefined;
  return true;
}

bool TestObject::set_prop_null(PP_Var prop_null) {
  if (prop_null.type != PP_VARTYPE_NULL) return false;
  prop_null_ = prop_null;
  return true;
}

bool TestObject::set_prop_bool(PP_Var prop_bool) {
  if (prop_bool.type != PP_VARTYPE_BOOL) return false;
  prop_bool_ = prop_bool;
  return true;
}

bool TestObject::set_prop_int32(PP_Var prop_int32) {
  if (prop_int32.type != PP_VARTYPE_INT32) return false;
  prop_int32_ = prop_int32;
  return true;
}

bool TestObject::set_prop_double(PP_Var prop_double) {
  if (prop_double.type != PP_VARTYPE_DOUBLE) return false;
  prop_double_ = prop_double;
  return true;
}

bool TestObject::set_prop_string(PP_Var prop_string) {
  if (prop_string.type != PP_VARTYPE_STRING) return false;
  prop_string_ = prop_string;
  return true;
}

bool TestObject::set_prop_object(PP_Var prop_object) {
  if (prop_object.type != PP_VARTYPE_OBJECT) return false;
  prop_object_ = prop_object;
  return true;
}

bool TestObject::set_module_ready(PP_Var prop_string) {
  // This is a read-only property.
  return false;
}

PP_Var TestObject::method_undefined(uint32_t argc,
                                    PP_Var* argv,
                                    PP_Var* exception) {
  if (argc != 1 || argv[0].type != PP_VARTYPE_UNDEFINED) {
    *exception = kCallFailed;
  }
  return argv[0];
}

PP_Var TestObject::method_null(uint32_t argc,
                               PP_Var* argv,
                               PP_Var* exception) {
  if (argc != 1 || argv[0].type != PP_VARTYPE_NULL) {
    *exception = kCallFailed;
  }
  return argv[0];
}

PP_Var TestObject::method_bool(uint32_t argc,
                               PP_Var* argv,
                               PP_Var* exception) {
  if (argc != 1 || argv[0].type != PP_VARTYPE_BOOL) {
    *exception = kCallFailed;
  }
  return argv[0];
}

PP_Var TestObject::method_int32(uint32_t argc,
                                PP_Var* argv,
                                PP_Var* exception) {
  if (argc != 1 || argv[0].type != PP_VARTYPE_INT32) {
    *exception = kCallFailed;
  }
  return argv[0];
}

PP_Var TestObject::method_double(uint32_t argc,
                                 PP_Var* argv,
                                 PP_Var* exception) {
  if (argc != 1 || argv[0].type != PP_VARTYPE_DOUBLE) {
    *exception = kCallFailed;
  }
  return argv[0];
}

PP_Var TestObject::method_string(uint32_t argc,
                                 PP_Var* argv,
                                 PP_Var* exception) {
  if (argc != 1 || argv[0].type != PP_VARTYPE_STRING) {
    *exception = kCallFailed;
  }
  return argv[0];
}

PP_Var TestObject::method_object(uint32_t argc,
                                 PP_Var* argv,
                                 PP_Var* exception) {
  if (argc != 1 || argv[0].type != PP_VARTYPE_OBJECT) {
    *exception = kCallFailed;
    return kCallFailed;
  }
  return argv[0];
}

PP_Var TestObject::method_undefined_2_args(uint32_t argc,
                                           PP_Var* argv,
                                           PP_Var* exception) {
  if (argc != 2 ||
      argv[0].type != PP_VARTYPE_UNDEFINED ||
      argv[1].type != PP_VARTYPE_UNDEFINED) {
    *exception = kCallFailed;
  }
  return argv[0];
}

PP_Var TestObject::method_null_2_args(uint32_t argc,
                                      PP_Var* argv,
                                      PP_Var* exception) {
  if (argc != 2 ||
      argv[0].type != PP_VARTYPE_NULL ||
      argv[1].type != PP_VARTYPE_NULL) {
    *exception = kCallFailed;
  }
  return argv[0];
}

PP_Var TestObject::method_bool_2_args(uint32_t argc,
                                      PP_Var* argv,
                                      PP_Var* exception) {
  if (argc != 2 ||
      argv[0].type != PP_VARTYPE_BOOL ||
      argv[1].type != PP_VARTYPE_BOOL) {
    *exception = kCallFailed;
  }
  return argv[0];
}

PP_Var TestObject::method_int32_2_args(uint32_t argc,
                                       PP_Var* argv,
                                       PP_Var* exception) {
  if (argc != 2 ||
      argv[0].type != PP_VARTYPE_INT32 ||
      argv[1].type != PP_VARTYPE_INT32) {
    *exception = kCallFailed;
  }
  return argv[0];
}

PP_Var TestObject::method_double_2_args(uint32_t argc,
                                        PP_Var* argv,
                                        PP_Var* exception) {
  if (argc != 2 ||
      argv[0].type != PP_VARTYPE_DOUBLE ||
      argv[1].type != PP_VARTYPE_DOUBLE) {
    *exception = kCallFailed;
  }
  return argv[0];
}

PP_Var TestObject::method_string_2_args(uint32_t argc,
                                        PP_Var* argv,
                                        PP_Var* exception) {
  if (argc != 2 ||
      argv[0].type != PP_VARTYPE_STRING ||
      argv[1].type != PP_VARTYPE_STRING) {
    *exception = kCallFailed;
  }
  return argv[0];
}

PP_Var TestObject::method_object_2_args(uint32_t argc,
                                        PP_Var* argv,
                                        PP_Var* exception) {
  if (argc != 2 ||
      argv[0].type != PP_VARTYPE_OBJECT ||
      argv[1].type != PP_VARTYPE_OBJECT) {
    *exception = kCallFailed;
    return kCallFailed;
  }
  return argv[0];
}

// A function to be called with the JavaScript |window| object.
// This test basically checks that we can access browser object proxies.
PP_Var TestObject::window_location(uint32_t argc,
                                   PP_Var* argv,
                                   PP_Var* exception) {
  if (argc != 1 ||
      argv[0].type != PP_VARTYPE_OBJECT) {
    *exception = kCallFailed;
    return kCallFailed;
  }
  // Get the location property from the passed in object.
  const char kLocation[] = "location";
  const uint32_t kLocationLength = static_cast<uint32_t>(strlen(kLocation));
  PP_Var location_name =
      g_var_interface->VarFromUtf8(g_module_id, kLocation, kLocationLength);
  PP_Var location =
      g_var_interface->GetProperty(argv[0], location_name, exception);
  if (location.type != PP_VARTYPE_OBJECT ||
      exception->type != PP_VARTYPE_UNDEFINED) {
    *exception = kCallFailed;
    return kCallFailed;
  }
  // Get the href property on the returned object.
  const char kHref[] = "href";
  const uint32_t kHrefLength = static_cast<uint32_t>(strlen(kHref));
  PP_Var href_name =
      g_var_interface->VarFromUtf8(g_module_id, kHref, kHrefLength);
  PP_Var href = g_var_interface->GetProperty(location, href_name, exception);
  printf("window.location.href = ");
  PrintPpVar(href);
  printf("\n");
  if (href.type != PP_VARTYPE_STRING ||
      exception->type != PP_VARTYPE_UNDEFINED) {
    *exception = kCallFailed;
    return kCallFailed;
  }
  return PP_MakeBool(PP_TRUE);
}

PP_Var TestObject::hello_world(uint32_t argc, PP_Var* argv, PP_Var* exception) {
  return g_var_interface->VarFromUtf8(g_module_id,
                                      kHelloWorld,
                                      kHelloWorldLength);
}

// PPP_Class_Deprecated

bool HasProperty(void* object,
                 PP_Var name,
                 PP_Var* exception) {
  TestObject* test_object = static_cast<TestObject*>(object);
  return test_object->HasProperty(name);
}

bool HasMethod(void* object,
               PP_Var name,
               PP_Var* exception) {
  TestObject* test_object = static_cast<TestObject*>(object);
  return test_object->HasMethod(name);
}

PP_Var GetProperty(void* object,
                   PP_Var name,
                   PP_Var* exception) {
  TestObject* test_object = static_cast<TestObject*>(object);
  return test_object->GetProperty(name);
}

void SetProperty(void* object,
                 PP_Var name,
                 PP_Var value,
                 PP_Var* exception) {
  TestObject* test_object = static_cast<TestObject*>(object);
  if (!test_object->SetProperty(name, value)) {
    *exception = kSetFailed;
  }
}

void RemoveProperty(void* object,
                    PP_Var name,
                    PP_Var* exception) {
  TestObject* test_object = static_cast<TestObject*>(object);
  test_object->RemoveProperty(name);
}

PP_Var Call(void* object,
            PP_Var name,
            uint32_t argc,
            PP_Var* argv,
            PP_Var* exception) {
  TestObject* test_object = static_cast<TestObject*>(object);
  return test_object->Call(name, argc, argv, exception);
}

static const PPP_Class_Deprecated object_class = {
  HasProperty,
  HasMethod,
  GetProperty,
  NULL,
  SetProperty,
  RemoveProperty,
  Call,
  NULL,
  NULL,
};

// PPP_Instance functions.

PP_Bool DidCreate(PP_Instance instance,
               uint32_t argc,
               const char* argn[],
               const char* argv[]) {
  printf("basic_object: DidCreate(%"NACL_PRIu64")\n", instance);
  for (uint32_t i = 0; i < argc; ++i) {
    printf("  arg[%"NACL_PRIu32"]: '%s' = '%s'\n", i, argn[i], argv[i]);
  }
  return PP_TRUE;
}

void DidDestroy(PP_Instance instance) {
  printf("basic_object: DidDestroy(%"NACL_PRIu64")\n", instance);
}

PP_Var GetInstanceObject(PP_Instance instance) {
  printf("basic_object: GetInstanceObject(%"NACL_PRIu64")\n", instance);
  printf("  g_var_interface = %p\n",
         reinterpret_cast<const void*>(g_var_interface));
  PP_Var retval =
      g_var_interface->CreateObject(g_module_id,
                                    &object_class,
                                    static_cast<void*>(new TestObject));
  return retval;
}

static const void* GetInstanceInterface() {
  static const PPP_Instance instance_class = {
    DidCreate,
    DidDestroy,
    NULL,  // DidChangeView
    NULL,  // DidChangeFocus
    NULL,  // HandleInputEvent
    NULL,  // HandleDocumentLoad
    GetInstanceObject
  };

  return reinterpret_cast<const void*>(&instance_class);
}

}  // namespace

PP_EXPORT int32_t PPP_InitializeModule(PP_Module module_id,
                                       PPB_GetInterface get_browser_interface) {
  // Save the global module information for later.
  g_module_id = module_id;
  g_get_browser_interface = get_browser_interface;
  printf("basic_object: PPP_InitializeModule(%"NACL_PRId64", %p)\n",
         module_id,
         get_browser_interface);

  g_var_interface =
      reinterpret_cast<const PPB_Var_Deprecated*>(
          get_browser_interface(PPB_VAR_DEPRECATED_INTERFACE));

  return PP_OK;
}

PP_EXPORT void PPP_ShutdownModule() {
  printf("basic_object: PPP_ShutdownModule()\n");
}

PP_EXPORT const void* PPP_GetInterface(const char* interface_name) {
  printf("basic_object: PPP_GetInterface('%s')\n", interface_name);
  if (strcmp(interface_name, PPP_INSTANCE_INTERFACE) == 0) {
    return GetInstanceInterface();
  }
  return NULL;
}
