// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_output_protection_private.h"

#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(OutputProtectionPrivate);

TestOutputProtectionPrivate::TestOutputProtectionPrivate(
    TestingInstance* instance)
    : TestCase(instance) {
}

bool TestOutputProtectionPrivate::Init() {
  output_protection_interface_ =
      static_cast<const PPB_OutputProtection_Private*>(
          pp::Module::Get()->GetBrowserInterface(
              PPB_OUTPUTPROTECTION_PRIVATE_INTERFACE_0_1));
  return output_protection_interface_ && CheckTestingInterface();
}

void TestOutputProtectionPrivate::RunTests(const std::string& filter) {
  RUN_TEST(QueryStatus, filter);
  RUN_TEST(EnableProtection, filter);
}

std::string TestOutputProtectionPrivate::TestQueryStatus() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  PP_Resource output_protection_resource = output_protection_interface_->
      Create(instance_->pp_instance());
  uint32_t link_mask;
  uint32_t protection_mask;
  callback.WaitForResult(
      output_protection_interface_->QueryStatus(
          output_protection_resource,
          &link_mask,
          &protection_mask,
          callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);

  PASS();
}

std::string TestOutputProtectionPrivate::TestEnableProtection() {
  TestCompletionCallback callback(instance_->pp_instance(), callback_type());

  PP_Resource output_protection_resource = output_protection_interface_->
      Create(instance_->pp_instance());
  callback.WaitForResult(
      output_protection_interface_->EnableProtection(
          output_protection_resource,
          PP_OUTPUT_PROTECTION_METHOD_PRIVATE_NONE,
          callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);

  PASS();
}
