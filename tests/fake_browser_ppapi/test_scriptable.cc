/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/tests/fake_browser_ppapi/test_scriptable.h"

#include <stdio.h>
#include <string.h>

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/tests/fake_browser_ppapi/fake_core.h"
#include "native_client/tests/fake_browser_ppapi/fake_host.h"
#include "native_client/tests/fake_browser_ppapi/fake_instance.h"
#include "native_client/tests/fake_browser_ppapi/fake_window.h"
#include "native_client/tests/fake_browser_ppapi/utility.h"
#include "ppapi/c/dev/ppb_var_deprecated.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/pp_errors.h"


namespace {

// Some canonical constants used to initialize properties of various types.
const PP_Bool kBoolValue = PP_TRUE;
const int32_t kInt32Value = 144000;
const double kDoubleValue = 3.1415;
const char* const kStringValue = "hello, world";

// Some global state.
const PPB_Var_Deprecated* g_var_interface;
const PPB_Instance* g_instance_interface;
PP_Instance g_instance_id;
PP_Instance g_browser_module_id;
int64_t g_object_as_id;

// TODO(sehr,polina): add this to the ppapi/c/dev/ppb_var_deprecated.h?
PP_Var MakeString(const char* str) {
  return g_var_interface->VarFromUtf8(g_browser_module_id,
                                      str,
                                      static_cast<uint32_t>(strlen(str)));
}

// PP_Var of string type that names the property that corresponds to the
// specified type.
PP_Var PropertyName(PP_VarType type) {
  switch (type) {
    case PP_VARTYPE_UNDEFINED:
      return MakeString("propUndefined");
    case PP_VARTYPE_NULL:
      return MakeString("propNull");
    case PP_VARTYPE_BOOL:
      return MakeString("propBool");
    case PP_VARTYPE_INT32:
      return MakeString("propInt32");
    case PP_VARTYPE_DOUBLE:
      return MakeString("propDouble");
    case PP_VARTYPE_STRING:
      return MakeString("propString");
    case PP_VARTYPE_OBJECT:
      return MakeString("propObject");
    case PP_VARTYPE_ARRAY:
    case PP_VARTYPE_DICTIONARY:
      break;
  }
  NACL_NOTREACHED();
  return PP_MakeUndefined();
}

// PP_Var of string type that names a method that corresponds to the
// specified type.  These methods all expect two args of |type|.
PP_Var MethodNameWith2Args(PP_VarType type) {
  switch (type) {
    case PP_VARTYPE_UNDEFINED:
      return MakeString("methodUndefinedWith2Args");
    case PP_VARTYPE_NULL:
      return MakeString("methodNullWith2Args");
    case PP_VARTYPE_BOOL:
      return MakeString("methodBoolWith2Args");
    case PP_VARTYPE_INT32:
      return MakeString("methodInt32With2Args");
    case PP_VARTYPE_DOUBLE:
      return MakeString("methodDoubleWith2Args");
    case PP_VARTYPE_STRING:
      return MakeString("methodStringWith2Args");
    case PP_VARTYPE_OBJECT:
      return MakeString("methodObjectWith2Args");
    case PP_VARTYPE_ARRAY:
    case PP_VARTYPE_DICTIONARY:
      break;
  }
  NACL_NOTREACHED();
  return PP_MakeUndefined();
}

// Returns a canonical PP_Var of type.
PP_Var PropertyValue(PP_VarType type) {
  switch (type) {
    case PP_VARTYPE_UNDEFINED:
      return PP_MakeUndefined();
    case PP_VARTYPE_NULL:
      return PP_MakeNull();
    case PP_VARTYPE_BOOL:
      return PP_MakeBool(kBoolValue);
    case PP_VARTYPE_INT32:
      return PP_MakeInt32(kInt32Value);
    case PP_VARTYPE_DOUBLE:
      return PP_MakeDouble(kDoubleValue);
    case PP_VARTYPE_STRING:
      return MakeString(kStringValue);
    case PP_VARTYPE_OBJECT:
      return g_instance_interface->GetWindowObject(g_instance_id);
    case PP_VARTYPE_ARRAY:
    case PP_VARTYPE_DICTIONARY:
      break;
  }
  NACL_NOTREACHED();
  return PP_MakeUndefined();
}

// Checks that the var matches the canonical var for the specified type.
PP_Bool PropertyIsValidValue(PP_VarType type, PP_Var value) {
  switch (type) {
    case PP_VARTYPE_UNDEFINED:
      return static_cast<PP_Bool>(value.type == PP_VARTYPE_UNDEFINED);
    case PP_VARTYPE_NULL:
      return static_cast<PP_Bool>(value.type == PP_VARTYPE_NULL);
    case PP_VARTYPE_BOOL:
      return static_cast<PP_Bool>((value.type == PP_VARTYPE_BOOL) &&
              (value.value.as_bool == kBoolValue));
    case PP_VARTYPE_INT32:
      return static_cast<PP_Bool>((value.type == PP_VARTYPE_INT32) &&
              (value.value.as_int == kInt32Value));
    case PP_VARTYPE_DOUBLE:
      return static_cast<PP_Bool>((value.type == PP_VARTYPE_DOUBLE) &&
              (value.value.as_double == kDoubleValue));
    case PP_VARTYPE_STRING: {
      if (value.type != PP_VARTYPE_STRING) {
        return PP_FALSE;
      }
      uint32_t ret_length;
      const char* ret_str = g_var_interface->VarToUtf8(value, &ret_length);
      if (ret_length == 0 || ret_str == NULL) {
        return PP_FALSE;
      }
      return static_cast<PP_Bool>(strcmp(ret_str, kStringValue) == 0);
    }
    case PP_VARTYPE_OBJECT: {
      // TODO(sehr,polina): check that the value is correct also.  This is
      // quite complicated with proxies-to-proxies at the moment.
      return static_cast<PP_Bool>(value.type == PP_VARTYPE_OBJECT);
    }
    case PP_VARTYPE_ARRAY:
    case PP_VARTYPE_DICTIONARY:
      break;
  }
  return PP_FALSE;
}

// Test that the "prop<Type>" property is present on object.
void CheckPresentProperty(PP_VarType type, PP_Var object) {
  PP_Var exception = PP_MakeUndefined();
  // PropertyName always returns a string PP_Var that names a valid property.
  CHECK(g_var_interface->HasProperty(object, PropertyName(type), &exception));
  CHECK(exception.type == PP_VARTYPE_UNDEFINED);
}

void CheckAbsentProperty(PP_VarType type, PP_Var object) {
  // PropertyValue returns a PP_Var of the requested type.  Only when type
  // == PP_VARTYPE_STRING can HasProperty succeed, and then only for valid
  // property names.  PropertyValue(PP_VARTYPE_STRING) returns a string
  // that is not a valid property name.
  PP_Var exception = PP_MakeUndefined();
  CHECK(!g_var_interface->HasProperty(object,
                                      PropertyValue(type),
                                      &exception));
  // TODO(sehr): Exception should be raised if type is not int or string.
  CHECK(exception.type == PP_VARTYPE_UNDEFINED);
}

void TestHasProperty(PP_Var object) {
  CheckAbsentProperty(PP_VARTYPE_UNDEFINED, object);
  CheckAbsentProperty(PP_VARTYPE_NULL, object);
  CheckAbsentProperty(PP_VARTYPE_BOOL, object);
  CheckAbsentProperty(PP_VARTYPE_INT32, object);
  CheckAbsentProperty(PP_VARTYPE_DOUBLE, object);
  CheckAbsentProperty(PP_VARTYPE_STRING, object);
  CheckAbsentProperty(PP_VARTYPE_OBJECT, object);
  CheckPresentProperty(PP_VARTYPE_UNDEFINED, object);
  CheckPresentProperty(PP_VARTYPE_NULL, object);
  CheckPresentProperty(PP_VARTYPE_BOOL, object);
  CheckPresentProperty(PP_VARTYPE_INT32, object);
  CheckPresentProperty(PP_VARTYPE_DOUBLE, object);
  CheckPresentProperty(PP_VARTYPE_STRING, object);
  CheckPresentProperty(PP_VARTYPE_OBJECT, object);
}

// Test setting the "prop<Property_type>" method on object, passing a value of
// "value_type".
void CheckSetProperty(PP_VarType property_type,
                      PP_VarType value_type,
                      PP_Var object) {
  PP_Var exception = PP_MakeUndefined();
  g_var_interface->SetProperty(object,
                               PropertyName(property_type),
                               PropertyValue(value_type),
                               &exception);
  // Only setting an property to a value of it's intrinsic type should succeed.
  // Success is indicated by the SetProperty returning a void exception.
  if (property_type == value_type) {
    CHECK(exception.type == PP_VARTYPE_UNDEFINED);
  } else {
    CHECK(exception.type != PP_VARTYPE_UNDEFINED);
  }
}

// Test setting one the "prop<Property_type>" property of object with all
// argument types.
void TestSetPropertyForType(PP_VarType property_type, PP_Var object) {
  CheckSetProperty(property_type, PP_VARTYPE_UNDEFINED, object);
  CheckSetProperty(property_type, PP_VARTYPE_NULL, object);
  CheckSetProperty(property_type, PP_VARTYPE_BOOL, object);
  CheckSetProperty(property_type, PP_VARTYPE_INT32, object);
  CheckSetProperty(property_type, PP_VARTYPE_DOUBLE, object);
  CheckSetProperty(property_type, PP_VARTYPE_STRING, object);
  CheckSetProperty(property_type, PP_VARTYPE_OBJECT, object);
}

// Test setting each type of property of object in succession.
// The values set in this function are used in TestGetProperty.
void TestSetProperty(PP_Var object) {
  TestSetPropertyForType(PP_VARTYPE_UNDEFINED, object);
  TestSetPropertyForType(PP_VARTYPE_NULL, object);
  TestSetPropertyForType(PP_VARTYPE_BOOL, object);
  TestSetPropertyForType(PP_VARTYPE_INT32, object);
  TestSetPropertyForType(PP_VARTYPE_DOUBLE, object);
  TestSetPropertyForType(PP_VARTYPE_STRING, object);
  TestSetPropertyForType(PP_VARTYPE_OBJECT, object);
}

// Invoke the GetProperty method on "object", passing the identifier
// for "property_type" and checking that the return value agrees in type
// and value.
void TestGetPropertyForType(PP_VarType property_type, PP_Var object) {
  PP_Var exception = PP_MakeUndefined();
  PP_Var result = g_var_interface->GetProperty(object,
                                               PropertyName(property_type),
                                               &exception);
  CHECK(PropertyIsValidValue(property_type, result));
  CHECK(exception.type == PP_VARTYPE_UNDEFINED);
}

// Test getting each type of property of object in succession.
void TestGetProperty(PP_Var object) {
  TestGetPropertyForType(PP_VARTYPE_UNDEFINED, object);
  TestGetPropertyForType(PP_VARTYPE_NULL, object);
  TestGetPropertyForType(PP_VARTYPE_BOOL, object);
  TestGetPropertyForType(PP_VARTYPE_INT32, object);
  TestGetPropertyForType(PP_VARTYPE_DOUBLE, object);
  TestGetPropertyForType(PP_VARTYPE_STRING, object);
  TestGetPropertyForType(PP_VARTYPE_OBJECT, object);
}

// Invoke the "method2<Type>" method of object with values of types arg1_type
// and arg2_type as parameters.
void CheckCallWithArgPair(PP_VarType type,
                          PP_VarType arg1_type,
                          PP_VarType arg2_type,
                          PP_Var object) {
  PP_Var argv[] = { PropertyValue(arg1_type), PropertyValue(arg2_type) };
  uint32_t argc = static_cast<uint32_t>(sizeof argv / sizeof argv[0]);
  PP_Var exception = PP_MakeUndefined();
  PP_Var retval = g_var_interface->Call(object,
                                        MethodNameWith2Args(type),
                                        argc,
                                        argv,
                                        &exception);
  // A successful call should return a var of "type" and exception should be
  // void.  A failing call's exception should be non-void.
  if (type == arg1_type && type == arg2_type) {
    CHECK(PropertyIsValidValue(type, retval));
    CHECK(exception.type == PP_VARTYPE_UNDEFINED);
  } else {
    CHECK(exception.type != PP_VARTYPE_UNDEFINED);
  }
}

// Invoke the "method<Type>" method of object with first argument type arg1_type
// and all possibilities of second argument type.
void TestCallForTypeWithArg1Type(PP_VarType type,
                                 PP_VarType arg1_type,
                                 PP_Var object) {
  CheckCallWithArgPair(type, arg1_type, PP_VARTYPE_UNDEFINED, object);
  CheckCallWithArgPair(type, arg1_type, PP_VARTYPE_NULL, object);
  CheckCallWithArgPair(type, arg1_type, PP_VARTYPE_BOOL, object);
  CheckCallWithArgPair(type, arg1_type, PP_VARTYPE_INT32, object);
  CheckCallWithArgPair(type, arg1_type, PP_VARTYPE_DOUBLE, object);
  CheckCallWithArgPair(type, arg1_type, PP_VARTYPE_STRING, object);
  CheckCallWithArgPair(type, arg1_type, PP_VARTYPE_OBJECT, object);
}

// Invoke the "method<Type>" method of object with one parameter of type "type".
void CallWithTooFewParameters(PP_VarType type, PP_Var object) {
  PP_Var argv[] = { PropertyValue(type) };
  uint32_t argc = static_cast<uint32_t>(sizeof argv / sizeof argv[0]);
  PP_Var exception = PP_MakeUndefined();
  (void) g_var_interface->Call(object,
                               MethodNameWith2Args(type),
                               argc,
                               argv,
                               &exception);
  CHECK(exception.type != PP_VARTYPE_UNDEFINED);
}

// Invoke the "method<Type>" method of object with three parameters of type
// "type".
void CallWithTooManyParameters(PP_VarType type, PP_Var object) {
  PP_Var argv[] = {
    PropertyValue(type),
    PropertyValue(type),
    PropertyValue(type)
  };
  uint32_t argc = static_cast<uint32_t>(sizeof argv / sizeof argv[0]);
  PP_Var exception = PP_MakeUndefined();
  (void) g_var_interface->Call(object,
                               MethodNameWith2Args(type),
                               argc,
                               argv,
                               &exception);
  CHECK(exception.type != PP_VARTYPE_UNDEFINED);
}

// Invoke the "method<Type>" method of object with all combinations of types for
// the first and second arguments.
void TestCallForType(PP_VarType method_type, PP_Var object) {
  CallWithTooFewParameters(method_type, object);
  CallWithTooManyParameters(method_type, object);
  // Try with various parameter type combinations.
  TestCallForTypeWithArg1Type(method_type, PP_VARTYPE_UNDEFINED, object);
  TestCallForTypeWithArg1Type(method_type, PP_VARTYPE_NULL, object);
  TestCallForTypeWithArg1Type(method_type, PP_VARTYPE_BOOL, object);
  TestCallForTypeWithArg1Type(method_type, PP_VARTYPE_INT32, object);
  TestCallForTypeWithArg1Type(method_type, PP_VARTYPE_DOUBLE, object);
  TestCallForTypeWithArg1Type(method_type, PP_VARTYPE_STRING, object);
  TestCallForTypeWithArg1Type(method_type, PP_VARTYPE_OBJECT, object);
}

// Test invoking each method of object in succession.
void TestCall(PP_Var object) {
  TestCallForType(PP_VARTYPE_UNDEFINED, object);
  TestCallForType(PP_VARTYPE_NULL, object);
  TestCallForType(PP_VARTYPE_BOOL, object);
  TestCallForType(PP_VARTYPE_INT32, object);
  TestCallForType(PP_VARTYPE_DOUBLE, object);
  TestCallForType(PP_VARTYPE_STRING, object);
  TestCallForType(PP_VARTYPE_OBJECT, object);
}

// Test calling a that scripts the object passed into it.  This is the
// beginning of a test of NaCl to browser scripting.
void TestWindowScripting(PP_Var object) {
  PP_Var argv = PropertyValue(PP_VARTYPE_OBJECT);
  PP_Var exception = PP_MakeUndefined();
  PP_Var retval = g_var_interface->Call(object,
                                        MakeString("windowLocation"),
                                        1,
                                        &argv,
                                        &exception);
  CHECK(PropertyIsValidValue(PP_VARTYPE_BOOL, retval));
  CHECK(exception.type == PP_VARTYPE_UNDEFINED);
}

}  // namespace

// Called from the fake browser.  The object needs to have certain
// attributes.  For each PP_VARTYPE variant,
// 1) There is a property, of PP_VARTYPE_<TYPE>, named "prop<Type>".
// 2) SetProperty("prop<Type>", value) only succeeds when value.type == type.
// 3) GetProperty("prop<Type>") only succeeds when value.type == type,
//    and returns the value set by the corresponding call to SetProperty.
// 4) Call("prop<Type>", { arg1, arg2 }) only succeeds when arg1.type == type,
//    arg2.type == type.  It returns arg1.type.
void TestScriptableObject(PP_Var object,
                          const PPB_Instance* browser_instance_interface,
                          const PPB_Var_Deprecated* var_interface,
                          PP_Instance instance_id,
                          PP_Module browser_module_id) {
  // Receiver needs to be a valid scriptable object.  We cannot use
  // is_valid_value here because we haven't set g_object_as_id yet.
  CHECK(object.type == PP_VARTYPE_OBJECT);
  // Save g_object_as_id for future object value validity checks.
  g_object_as_id = object.value.as_id;
  // Initialize the global state.
  g_var_interface = var_interface;
  g_instance_interface = browser_instance_interface;
  g_instance_id = instance_id;
  g_browser_module_id = browser_module_id;
  // And test the scriptable object interfaces one-by-one.
  TestHasProperty(object);
  TestSetProperty(object);
  TestGetProperty(object);
  TestCall(object);
  TestWindowScripting(object);
  // TODO(sehr,polina): add other methods such as RemoveProperty.
}
