// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_flash_drm.h"

#include "ppapi/c/pp_macros.h"
#include "ppapi/c/private/ppb_flash_drm.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/flash_device_id.h"
#include "ppapi/cpp/private/flash_drm.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(FlashDRM);

using pp::flash::DeviceID;
using pp::flash::DRM;
using pp::Var;

TestFlashDRM::TestFlashDRM(TestingInstance* instance)
    : TestCase(instance),
      PP_ALLOW_THIS_IN_INITIALIZER_LIST(callback_factory_(this)) {
}

void TestFlashDRM::RunTests(const std::string& filter) {
  RUN_TEST(GetDeviceID, filter);
}

std::string TestFlashDRM::TestGetDeviceID() {
  // Test the old C++ wrapper.
  // TODO(raymes): Remove this once Flash switches APIs.
  {
    DeviceID device_id(instance_);
    TestCompletionCallbackWithOutput<Var> output_callback(
        instance_->pp_instance());
    int32_t rv = device_id.GetDeviceID(output_callback.GetCallback());
    output_callback.WaitForResult(rv);
    ASSERT_TRUE(output_callback.result() == PP_OK);
    Var result = output_callback.output();
    ASSERT_TRUE(result.is_string());
    std::string id = result.AsString();
    ASSERT_FALSE(id.empty());
  }

  {
    DRM drm(instance_);
    TestCompletionCallbackWithOutput<Var> output_callback(
        instance_->pp_instance());
    int32_t rv = drm.GetDeviceID(output_callback.GetCallback());
    output_callback.WaitForResult(rv);
    ASSERT_TRUE(output_callback.result() == PP_OK);
    Var result = output_callback.output();
    ASSERT_TRUE(result.is_string());
    std::string id = result.AsString();
    ASSERT_FALSE(id.empty());
  }

  PASS();
}
