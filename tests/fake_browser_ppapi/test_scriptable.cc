/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/tests/fake_browser_ppapi/test_scriptable.h"

#include <string.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/ppapi_proxy/utility.h"
#include "native_client/src/shared/ppapi_proxy/plugin_var.h"
#include "native_client/tests/fake_browser_ppapi/fake_core.h"
#include "native_client/tests/fake_browser_ppapi/fake_host.h"
#include "native_client/tests/fake_browser_ppapi/fake_instance.h"
#include "native_client/tests/fake_browser_ppapi/fake_window.h"
#include "ppapi/c/ppb_core.h"
#include "ppapi/c/ppb_instance.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/c/ppp_instance.h"
#include "ppapi/c/pp_errors.h"


namespace {

// Some canonical constants used for various types.
static const bool kBoolValue = true;
static const int32_t kInt32Value = 144000;
static const double kDoubleValue = 3.1415;
static const char kStringValue[] = "hello, world";

// Some global state.
static const PPB_Var* g_var_interface;
static const PPB_Instance* g_instance_interface;
PP_Instance g_instance_id;

PP_Var MakeStringVar(const char* str) {
  return g_var_interface->VarFromUtf8(str, static_cast<uint32_t>(strlen(str)));
}

// Traits for PP_VarType:
// ident() returns a PP_Var of string type that names an identifier.
// var() returns a canonical PP_Var of the type.
// valid(var) checks that the var matches the canonical var.
template<PP_VarType> struct VarTraits { };

template<> struct VarTraits<PP_VARTYPE_VOID> {
  static PP_Var ident() { return MakeStringVar("identVoid"); }
  static PP_Var var() { return PP_MakeVoid(); }
  static bool valid(PP_Var var) {
    return var.type == PP_VARTYPE_VOID;
  }
};

template<> struct VarTraits<PP_VARTYPE_NULL> {
  static PP_Var ident() { return MakeStringVar("identNull"); }
  static PP_Var var() { return PP_MakeNull(); }
  static bool valid(PP_Var var) {
    return var.type == PP_VARTYPE_NULL;
  }
};

template<> struct VarTraits<PP_VARTYPE_BOOL> {
  static PP_Var ident() { return MakeStringVar("identBool"); }
  static PP_Var var() { return PP_MakeBool(kBoolValue); }
  static bool valid(PP_Var var) {
    return (var.type == PP_VARTYPE_BOOL) && (var.value.as_bool == kBoolValue);
  }
};

template<> struct VarTraits<PP_VARTYPE_INT32> {
  static PP_Var ident() { return MakeStringVar("identInt32"); }
  static PP_Var var() { return PP_MakeInt32(kInt32Value); }
  static bool valid(PP_Var var) {
    return (var.type == PP_VARTYPE_INT32) && (var.value.as_int == kInt32Value);
  }
};

template<> struct VarTraits<PP_VARTYPE_DOUBLE> {
  static PP_Var ident() { return MakeStringVar("identDouble"); }
  static PP_Var var() { return PP_MakeDouble(kDoubleValue); }
  static bool valid(PP_Var var) {
    return (var.type == PP_VARTYPE_DOUBLE) &&
        (var.value.as_double == kDoubleValue);
  }
};

template<> struct VarTraits<PP_VARTYPE_STRING> {
  static PP_Var ident() { return MakeStringVar("identString"); }
  static PP_Var var() { return MakeStringVar(kStringValue); }
  static bool valid(PP_Var var) {
    if (var.type != PP_VARTYPE_STRING) return false;
    uint32_t ret_length;
    const char* ret_str = g_var_interface->VarToUtf8(var, &ret_length);
    if (ret_length != static_cast<uint32_t>(strlen(kStringValue))) {
      return false;
    }
    return strcmp(ret_str, kStringValue) == 0;
  }
};

template<> struct VarTraits<PP_VARTYPE_OBJECT> {
  static PP_Var ident() { return MakeStringVar("identObject"); }
  static PP_Var var() {
    return g_instance_interface->GetWindowObject(g_instance_id);
  }
  static bool valid(PP_Var var) {
    return var.type == PP_VARTYPE_OBJECT;
  }
};

// Test HasProperty for two property names, one present, one absent.
void TestHasProperty(PP_Var receiver) {
  PP_Var exception = VarTraits<PP_VARTYPE_VOID>::var();
  // Integer name, should fail.
  CHECK(!g_var_interface->HasProperty(receiver,
                                      VarTraits<PP_VARTYPE_INT32>::var(),
                                      &exception));
  CHECK(VarTraits<PP_VARTYPE_VOID>::valid(exception));
  // String name, should succeed.
  CHECK(g_var_interface->HasProperty(receiver,
                                     VarTraits<PP_VARTYPE_STRING>::ident(),
                                     &exception));
  CHECK(VarTraits<PP_VARTYPE_VOID>::valid(exception));
}

// Invoke the SetProperty method on "receiver", passing the identifier
// for "property_type" and a value of "call_type".
template<PP_VarType property_type, PP_VarType call_type>
void SetProperty(PP_Var receiver) {
  PP_Var exception = VarTraits<PP_VARTYPE_VOID>::var();
  g_var_interface->SetProperty(receiver,
                               VarTraits<property_type>::ident(),
                               VarTraits<call_type>::var(),
                               &exception);
  // Only setting an property to a value of it's intrinsic type should succeed.
  if (property_type == call_type) {
    //  Success is indicated by the SetProperty returning a void exception.
    CHECK(VarTraits<PP_VARTYPE_VOID>::valid(exception));
  } else {
    //  Failure is indicated by the SetProperty returning a non-void exception.
    CHECK(!VarTraits<PP_VARTYPE_VOID>::valid(exception));
  }
}

// Test setting one property type with all argument types.
// expected_pass_type is the PP_VarType that should return a void exception.
// The others should all return non-void exceptions.
template<PP_VarType property_type> void TestOneSetter(PP_Var receiver) {
  SetProperty<property_type, PP_VARTYPE_VOID>(receiver);
  SetProperty<property_type, PP_VARTYPE_NULL>(receiver);
  SetProperty<property_type, PP_VARTYPE_BOOL>(receiver);
  SetProperty<property_type, PP_VARTYPE_INT32>(receiver);
  SetProperty<property_type, PP_VARTYPE_DOUBLE>(receiver);
  SetProperty<property_type, PP_VARTYPE_STRING>(receiver);
  SetProperty<property_type, PP_VARTYPE_OBJECT>(receiver);
}

// Test setting each type of property in succession.
// The values set in this function are used in TestGetProperty.
void TestSetProperty(PP_Var receiver) {
  TestOneSetter<PP_VARTYPE_VOID>(receiver);
  TestOneSetter<PP_VARTYPE_NULL>(receiver);
  TestOneSetter<PP_VARTYPE_BOOL>(receiver);
  TestOneSetter<PP_VARTYPE_INT32>(receiver);
  TestOneSetter<PP_VARTYPE_DOUBLE>(receiver);
  TestOneSetter<PP_VARTYPE_STRING>(receiver);
  TestOneSetter<PP_VARTYPE_OBJECT>(receiver);
}

// Invoke the GetProperty method on "receiver", passing the identifier
// for "property_type" and checking that the return value agrees in type
// and value.
template<PP_VarType property_type> void TestOneGetter(PP_Var receiver) {
  PP_Var exception = VarTraits<PP_VARTYPE_VOID>::var();
  PP_Var result;
  // void.
  result = g_var_interface->GetProperty(receiver,
                                        VarTraits<property_type>::ident(),
                                        &exception);
  CHECK(VarTraits<property_type>::valid(result));
  CHECK(VarTraits<PP_VARTYPE_VOID>::valid(exception));
}

// Test getting each type of property in succession.
void TestGetProperty(PP_Var receiver) {
  TestOneGetter<PP_VARTYPE_VOID>(receiver);
  TestOneGetter<PP_VARTYPE_NULL>(receiver);
  TestOneGetter<PP_VARTYPE_BOOL>(receiver);
  TestOneGetter<PP_VARTYPE_INT32>(receiver);
  TestOneGetter<PP_VARTYPE_DOUBLE>(receiver);
  TestOneGetter<PP_VARTYPE_STRING>(receiver);
  TestOneGetter<PP_VARTYPE_OBJECT>(receiver);
}

// Invoke a method as
//   method_type "VarTraits<method_type>::ident()"(arg1_type, arg2_type).
template<PP_VarType method_type, PP_VarType arg1_type, PP_VarType arg2_type>
void OneCallArgPair(PP_Var receiver) {
  PP_Var exception = VarTraits<PP_VARTYPE_VOID>::var();
  PP_Var argv[] = {
    VarTraits<arg1_type>::var(),
    VarTraits<arg2_type>::var()
  };
  uint32_t argc = static_cast<uint32_t>(sizeof(argv) / sizeof(argv[0]));
  PP_Var retval = g_var_interface->Call(receiver,
                                        VarTraits<method_type>::ident(),
                                        argc,
                                        argv,
                                        &exception);
  if (method_type == arg1_type && method_type == arg2_type) {
    // A successful call should return a var of "method_type" and exception
    // should be of type void.
    CHECK(VarTraits<method_type>::valid(retval));
    CHECK(VarTraits<PP_VARTYPE_VOID>::valid(exception));
  } else {
    // A failing call's exception should be of type non-void.
    CHECK(!VarTraits<PP_VARTYPE_VOID>::valid(exception));
  }
}

// Invoke all the possibilities of arg2_type of method
//   method_type "VarTraits<method_type>::ident()"(arg1_type, arg2_type).
template<PP_VarType method_type, PP_VarType arg1_type>
void OneCallArg1(PP_Var receiver) {
  OneCallArgPair<method_type, arg1_type, PP_VARTYPE_VOID>(receiver);
  OneCallArgPair<method_type, arg1_type, PP_VARTYPE_NULL>(receiver);
  OneCallArgPair<method_type, arg1_type, PP_VARTYPE_BOOL>(receiver);
  OneCallArgPair<method_type, arg1_type, PP_VARTYPE_INT32>(receiver);
  OneCallArgPair<method_type, arg1_type, PP_VARTYPE_DOUBLE>(receiver);
  OneCallArgPair<method_type, arg1_type, PP_VARTYPE_STRING>(receiver);
  OneCallArgPair<method_type, arg1_type, PP_VARTYPE_OBJECT>(receiver);
}

// Call with one parameter (method expects 2).
template<PP_VarType method_type>
void CallWithTooFewParameters(PP_Var receiver) {
  PP_Var exception = VarTraits<PP_VARTYPE_VOID>::var();
  PP_Var argv = VarTraits<method_type>::var();
  PP_Var retval = g_var_interface->Call(receiver,
                                        VarTraits<method_type>::ident(),
                                        1,
                                        &argv,
                                        &exception);
  CHECK(!VarTraits<PP_VARTYPE_VOID>::valid(exception));
}

// Call with three parameters (method expects 2).
template<PP_VarType method_type>
void CallWithTooManyParameters(PP_Var receiver) {
  PP_Var exception = VarTraits<PP_VARTYPE_VOID>::var();
  PP_Var argv[] = {
    VarTraits<method_type>::var(),
    VarTraits<method_type>::var(),
    VarTraits<method_type>::var()
  };
  uint32_t argc = static_cast<uint32_t>(sizeof(argv) / sizeof(argv[0]));
  PP_Var retval = g_var_interface->Call(receiver,
                                        VarTraits<method_type>::ident(),
                                        argc,
                                        argv,
                                        &exception);
  CHECK(!VarTraits<PP_VARTYPE_VOID>::valid(exception));
}

// Test calling one method with parameters and return type of each type
// in succession.  Invoke all the possibilities of arg1_type of method
//   method_type "VarTraits<method_type>::ident()"(arg1_type, arg2_type).
template<PP_VarType method_type> void TestOneCall(PP_Var receiver) {
  CallWithTooFewParameters<method_type>(receiver);
  CallWithTooManyParameters<method_type>(receiver);
  // Try with various parameter type combinations.
  OneCallArg1<method_type, PP_VARTYPE_VOID>(receiver);
  OneCallArg1<method_type, PP_VARTYPE_NULL>(receiver);
  OneCallArg1<method_type, PP_VARTYPE_BOOL>(receiver);
  OneCallArg1<method_type, PP_VARTYPE_INT32>(receiver);
  OneCallArg1<method_type, PP_VARTYPE_DOUBLE>(receiver);
  OneCallArg1<method_type, PP_VARTYPE_STRING>(receiver);
  OneCallArg1<method_type, PP_VARTYPE_OBJECT>(receiver);
}

// Test calling each type of function in succession.
void TestCall(PP_Var receiver) {
  TestOneCall<PP_VARTYPE_VOID>(receiver);
  TestOneCall<PP_VARTYPE_NULL>(receiver);
  TestOneCall<PP_VARTYPE_BOOL>(receiver);
  TestOneCall<PP_VARTYPE_INT32>(receiver);
  TestOneCall<PP_VARTYPE_DOUBLE>(receiver);
  TestOneCall<PP_VARTYPE_STRING>(receiver);
  TestOneCall<PP_VARTYPE_OBJECT>(receiver);
}

// Test calling a that scripts the object passed into it.  This is the
// beginning of a test of NaCl to browser scripting.
void TestWindowScripting(PP_Var receiver) {
  PP_Var exception = VarTraits<PP_VARTYPE_VOID>::var();
  PP_Var argv = VarTraits<PP_VARTYPE_OBJECT>::var();
  PP_Var retval = g_var_interface->Call(receiver,
                                        MakeStringVar("identWindow"),
                                        1,
                                        &argv,
                                        &exception);
  CHECK(VarTraits<PP_VARTYPE_BOOL>::valid(retval));
  CHECK(VarTraits<PP_VARTYPE_VOID>::valid(exception));
}

}  // namespace

// Test scriptable object.
void TestScriptableObject(PP_Var receiver,
                          const PPB_Instance* browser_instance_interface,
                          const PPB_Var* var_interface,
                          PP_Instance instance_id) {
  // Receiver needs to be a valid scriptable object.
  CHECK(VarTraits<PP_VARTYPE_OBJECT>::valid(receiver));
  // Initialize the global state.
  g_var_interface = var_interface;
  g_instance_interface = browser_instance_interface;
  g_instance_id = instance_id;
  // And test the scriptable object interfaces one-by-one.
  TestHasProperty(receiver);
  TestSetProperty(receiver);
  TestGetProperty(receiver);
  TestCall(receiver);
  TestWindowScripting(receiver);
}
