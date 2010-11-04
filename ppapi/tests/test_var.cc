// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_var.h"

#include <limits>

#include "ppapi/c/pp_var.h"
#include "ppapi/c/ppb_var.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Var);

namespace {
pp::Var adoptVar(PP_Var var) {
  return pp::Var(pp::Var::PassRef(), var);
}
}  // namespace

bool TestVar::Init() {
  var_interface_ = reinterpret_cast<PPB_Var const*>(
      pp::Module::Get()->GetBrowserInterface(PPB_VAR_INTERFACE));
  return var_interface_ && InitTestingInterface();
}

void TestVar::RunTest() {
  RUN_TEST(ConvertType);
  RUN_TEST(DefineProperty);
}

std::string TestVar::TestConvertType() {
  pp::Var result;
  PP_Var exception = PP_MakeUndefined();
  double NaN = std::numeric_limits<double>::quiet_NaN();

  // Int to string
  result = adoptVar(var_interface_->ConvertType(instance_->pp_instance(),
                                                pp::Var(100).pp_var(),
                                                PP_VARTYPE_STRING,
                                                &exception));
  ASSERT_EQ(pp::Var("100"), result);
  ASSERT_TRUE(adoptVar(exception).is_undefined());

  // Int to double
  result = adoptVar(var_interface_->ConvertType(instance_->pp_instance(),
                                                pp::Var(100).pp_var(),
                                                PP_VARTYPE_DOUBLE,
                                                &exception));
  ASSERT_EQ(pp::Var(100.0), result);
  ASSERT_TRUE(adoptVar(exception).is_undefined());

  // Double to int
  result = adoptVar(var_interface_->ConvertType(instance_->pp_instance(),
                                                pp::Var(100.0).pp_var(),
                                                PP_VARTYPE_INT32,
                                                &exception));
  ASSERT_EQ(pp::Var(100), result);
  ASSERT_TRUE(adoptVar(exception).is_undefined());

  // Double(NaN) to int
  result = adoptVar(var_interface_->ConvertType(instance_->pp_instance(),
                                                pp::Var(NaN).pp_var(),
                                                PP_VARTYPE_INT32,
                                                &exception));
  ASSERT_EQ(pp::Var(0), result);
  ASSERT_TRUE(adoptVar(exception).is_undefined());

  // Double to string
  result = adoptVar(var_interface_->ConvertType(instance_->pp_instance(),
                                                pp::Var(100.0).pp_var(),
                                                PP_VARTYPE_STRING,
                                                &exception));
  ASSERT_EQ(pp::Var("100"), result);
  ASSERT_TRUE(adoptVar(exception).is_undefined());

  // Double(NaN) to string
  result = adoptVar(var_interface_->ConvertType(instance_->pp_instance(),
                                                pp::Var(NaN).pp_var(),
                                                PP_VARTYPE_STRING,
                                                &exception));
  ASSERT_EQ(pp::Var("NaN"), result);
  ASSERT_TRUE(adoptVar(exception).is_undefined());

  // String to int, valid string
  result = adoptVar(var_interface_->ConvertType(instance_->pp_instance(),
                                                pp::Var("100").pp_var(),
                                                PP_VARTYPE_INT32,
                                                &exception));
  ASSERT_EQ(pp::Var(100), result);
  ASSERT_TRUE(adoptVar(exception).is_undefined());

  // String to int, invalid string
  result = adoptVar(var_interface_->ConvertType(instance_->pp_instance(),
                                                pp::Var("jockey").pp_var(),
                                                PP_VARTYPE_INT32,
                                                &exception));
  ASSERT_EQ(pp::Var(0), result);
  ASSERT_TRUE(adoptVar(exception).is_undefined());

  PASS();
}

std::string TestVar::TestDefineProperty() {
  pp::Var exception;
  pp::Var property;
  pp::Var value;
  PP_Var unmanaged_exception = PP_MakeUndefined();

  // Create an empty object.
  pp::Var test_obj = instance_->ExecuteScript("({})", &exception);
  ASSERT_TRUE(exception.is_undefined());
  ASSERT_TRUE(test_obj.is_object());

  // Define a simple property.
  property = "x";
  value = 1001;
  var_interface_->DefineProperty(test_obj.pp_var(),
                                 PP_MakeSimpleProperty(property.pp_var(),
                                                       value.pp_var()),
                                 &unmanaged_exception);
  ASSERT_TRUE(adoptVar(unmanaged_exception).is_undefined());

  ASSERT_EQ(value, test_obj.GetProperty(property, &exception));
  ASSERT_TRUE(exception.is_undefined());

  // Define a property with a getter that always returns 123 and setter that
  // sets another property to the given value.
  property = "y";
  pp::Var getter = instance_->ExecuteScript(
      "(function(){return 'okey';})", &exception);
  ASSERT_TRUE(getter.is_object());
  ASSERT_TRUE(exception.is_undefined());
  pp::Var setter = instance_->ExecuteScript(
      "(function(x){this['another']=x;})", &exception);
  ASSERT_TRUE(setter.is_object());
  ASSERT_TRUE(exception.is_undefined());

  struct PP_ObjectProperty property_attributes = {
    property.pp_var(),
    PP_MakeUndefined(),
    getter.pp_var(),
    setter.pp_var(),
    PP_OBJECTPROPERTY_MODIFIER_NONE
  };
  var_interface_->DefineProperty(test_obj.pp_var(), property_attributes,
                                 &unmanaged_exception);
  ASSERT_TRUE(adoptVar(unmanaged_exception).is_undefined());

  value = test_obj.GetProperty(property, &exception);
  ASSERT_EQ(pp::Var("okey"), value);
  ASSERT_TRUE(exception.is_undefined());

  value = test_obj.GetProperty("another", &exception);
  ASSERT_TRUE(value.is_undefined());
  ASSERT_TRUE(exception.is_undefined());

  test_obj.SetProperty("another", "dokey", &exception);
  ASSERT_TRUE(exception.is_undefined());

  ASSERT_EQ(pp::Var("dokey"), test_obj.GetProperty("another", &exception));
  ASSERT_TRUE(exception.is_undefined());

  PASS();
}
