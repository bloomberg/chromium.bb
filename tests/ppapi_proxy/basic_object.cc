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
#include "ppapi/c/pp_errors.h"
#include "ppapi/c/pp_module.h"
#include "ppapi/c/pp_resource.h"
#include "ppapi/c/ppb.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppp.h"
#include "ppapi/c/ppp_class.h"
#include "ppapi/c/ppp_instance.h"


namespace {

PP_Module g_module_id;
PPB_GetInterface g_get_browser_interface = NULL;
const PPB_Var* g_var_interface = NULL;

const PP_Var kGetFailed = PP_MakeNull();
const PP_Var kSetFailed = PP_MakeInt32(-1);
const PP_Var kCallFailed = PP_MakeInt32(-1);

const char kPropVoid[] = "identVoid";
const char kPropNull[] = "identNull";
const char kPropBool[] = "identBool";
const char kPropInt32[] = "identInt32";
const char kPropDouble[] = "identDouble";
const char kPropString[] = "identString";
const char kPropObject[] = "identObject";
const char kPropWindow[] = "identWindow";
const char kHelloWorld[] = "hello, world";
const uint32_t kHelloWorldLength = static_cast<uint32_t>(sizeof(kHelloWorld));

// A printer for PP_Vars.
void PrintPpVar(PP_Var var) {
  printf("PP_Var(");
  switch (var.type) {
    case PP_VARTYPE_VOID:
      printf("void");
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
  PP_Var GetProperty(PP_Var name);
  bool SetProperty(PP_Var name, PP_Var value);
  PP_Var Call(PP_Var name, uint32_t argc, PP_Var* argv, PP_Var* exception);

 private:
  // Getter/setter/function for PP_VARTYPE_VOID.
  PP_Var prop_void() const { return prop_void_; }
  bool set_prop_void(PP_Var prop_void);
  PP_Var func_void(uint32_t argc, PP_Var* argv, PP_Var* exception);
  // Getter/setter/function for PP_VARTYPE_NULL.
  PP_Var prop_null() const { return prop_null_; }
  bool set_prop_null(PP_Var prop_null);
  PP_Var func_null(uint32_t argc, PP_Var* argv, PP_Var* exception);
  // Getter/setter/function for PP_VARTYPE_BOOL.
  PP_Var prop_bool() const { return prop_bool_; }
  bool set_prop_bool(PP_Var prop_bool);
  PP_Var func_bool(uint32_t argc, PP_Var* argv, PP_Var* exception);
  // Getter/setter/function for PP_VARTYPE_INT32.
  PP_Var prop_int32() const { return prop_int32_; }
  bool set_prop_int32(PP_Var prop_int32);
  PP_Var func_int32(uint32_t argc, PP_Var* argv, PP_Var* exception);
  // Getter/setter/function for PP_VARTYPE_DOUBLE.
  PP_Var prop_double() const { return prop_double_; }
  bool set_prop_double(PP_Var prop_double);
  PP_Var func_double(uint32_t argc, PP_Var* argv, PP_Var* exception);
  // Getter/setter/function for PP_VARTYPE_STRING.
  PP_Var prop_string() const { return prop_string_; }
  bool set_prop_string(PP_Var prop_string);
  PP_Var func_string(uint32_t argc, PP_Var* argv, PP_Var* exception);
  // Getter/setter/function for PP_VARTYPE_OBJECT.
  PP_Var prop_object() const { return prop_object_; }
  bool set_prop_object(PP_Var prop_object);
  PP_Var func_object(uint32_t argc, PP_Var* argv, PP_Var* exception);
  // Test of scripting the window object.
  PP_Var func_window(uint32_t argc, PP_Var* argv, PP_Var* exception);

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

  PP_Var prop_void_;
  PP_Var prop_null_;
  PP_Var prop_bool_;
  PP_Var prop_int32_;
  PP_Var prop_double_;
  PP_Var prop_string_;
  PP_Var prop_object_;

  static PropertyMap property_map;
  static MethodMap method_map;
  static bool class_is_initialized;
};

TestObject::PropertyMap TestObject::property_map;
TestObject::MethodMap TestObject::method_map;
bool TestObject::class_is_initialized = false;

TestObject::TestObject() : prop_void_(PP_MakeNull()),
  prop_null_(PP_MakeNull()),
  prop_int32_(PP_MakeNull()),
  prop_double_(PP_MakeNull()),
  prop_string_(PP_MakeNull()),
  prop_object_(PP_MakeNull()) {
  if (class_is_initialized) {
    return;
  }
  // Property map initialization.
  property_map[kPropVoid] =
      Property(&TestObject::prop_void, &TestObject::set_prop_void);
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
  method_map[kPropVoid] = &TestObject::func_void;
  method_map[kPropNull] = &TestObject::func_null;
  method_map[kPropBool] = &TestObject::func_bool;
  method_map[kPropInt32] = &TestObject::func_int32;
  method_map[kPropDouble] = &TestObject::func_double;
  method_map[kPropString] = &TestObject::func_string;
  method_map[kPropObject] = &TestObject::func_object;
  method_map[kPropWindow] = &TestObject::func_window;
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

bool TestObject::set_prop_void(PP_Var prop_void) {
  if (prop_void.type != PP_VARTYPE_VOID) return false;
  prop_void_ = prop_void;
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

PP_Var TestObject::func_void(uint32_t argc,
                             PP_Var* argv,
                             PP_Var* exception) {
  if (argc != 2 ||
      argv[0].type != PP_VARTYPE_VOID ||
      argv[1].type != PP_VARTYPE_VOID) {
    *exception = kCallFailed;
  }
  return argv[0];
}

PP_Var TestObject::func_null(uint32_t argc,
                             PP_Var* argv,
                             PP_Var* exception) {
  if (argc != 2 ||
      argv[0].type != PP_VARTYPE_NULL ||
      argv[1].type != PP_VARTYPE_NULL) {
    *exception = kCallFailed;
  }
  return argv[0];
}

PP_Var TestObject::func_bool(uint32_t argc,
                             PP_Var* argv,
                             PP_Var* exception) {
  if (argc != 2 ||
      argv[0].type != PP_VARTYPE_BOOL ||
      argv[1].type != PP_VARTYPE_BOOL) {
    *exception = kCallFailed;
  }
  return argv[0];
}

PP_Var TestObject::func_int32(uint32_t argc,
                              PP_Var* argv,
                              PP_Var* exception) {
  if (argc != 2 ||
      argv[0].type != PP_VARTYPE_INT32 ||
      argv[1].type != PP_VARTYPE_INT32) {
    *exception = kCallFailed;
  }
  return argv[0];
}

PP_Var TestObject::func_double(uint32_t argc,
                               PP_Var* argv,
                               PP_Var* exception) {
  if (argc != 2 ||
      argv[0].type != PP_VARTYPE_DOUBLE ||
      argv[1].type != PP_VARTYPE_DOUBLE) {
    *exception = kCallFailed;
  }
  return argv[0];
}

PP_Var TestObject::func_string(uint32_t argc,
                               PP_Var* argv,
                               PP_Var* exception) {
  if (argc != 2 ||
      argv[0].type != PP_VARTYPE_STRING ||
      argv[1].type != PP_VARTYPE_STRING) {
    *exception = kCallFailed;
  }
  return argv[0];
}

PP_Var TestObject::func_object(uint32_t argc,
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

// A function to be called with the JavaScript windows object.
// This test basically checks that we can access browser object proxies.
PP_Var TestObject::func_window(uint32_t argc,
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
      exception->type != PP_VARTYPE_VOID) {
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
  if (href.type != PP_VARTYPE_STRING || exception->type != PP_VARTYPE_VOID) {
    *exception = kCallFailed;
    return kCallFailed;
  }
  return PP_MakeBool(true);
}

// PPP_Class

bool HasProperty(void* object,
                 PP_Var name,
                 PP_Var* exception) {
  TestObject* test_object = static_cast<TestObject*>(object);
  return test_object->HasProperty(name);
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

PP_Var Call(void* object,
            PP_Var name,
            uint32_t argc,
            PP_Var* argv,
            PP_Var* exception) {
  TestObject* test_object = static_cast<TestObject*>(object);
  return test_object->Call(name, argc, argv, exception);
}

static const PPP_Class object_class = {
  HasProperty,
  NULL,
  GetProperty,
  NULL,
  SetProperty,
  NULL,
  Call,
  NULL,
  NULL,
};

// PPP_Instance functions.

bool New(PP_Instance instance) {
  printf("basic_object: New(%"NACL_PRIu64")\n", instance);
  return true;
}

void Delete(PP_Instance instance) {
  printf("basic_object: Delete(%"NACL_PRIu64")\n", instance);
}

bool Initialize(PP_Instance instance,
                uint32_t argc,
                const char* argn[],
                const char* argv[]) {
  printf("basic_object: Initialize(%"NACL_PRIu64")\n", instance);
  for (uint32_t i = 0; i < argc; ++i) {
    printf("  arg[%"NACL_PRIu32"]: '%s' = '%s'\n", i, argn[i], argv[i]);
  }
  return true;
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
    New,
    Delete,
    Initialize,
    NULL,
    NULL,
    NULL,
    GetInstanceObject,
    NULL,
    NULL
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
      reinterpret_cast<const PPB_Var*>(
          get_browser_interface(PPB_VAR_INTERFACE));

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
