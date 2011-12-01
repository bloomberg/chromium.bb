// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_flash.h"

#include "ppapi/c/private/ppb_flash.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/var.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Flash);

using pp::Var;

TestFlash::TestFlash(TestingInstance* instance)
    : TestCase(instance),
      PP_ALLOW_THIS_IN_INITIALIZER_LIST(callback_factory_(this)) {
}

bool TestFlash::Init() {
  flash_interface_ = static_cast<const PPB_Flash*>(
      pp::Module::Get()->GetBrowserInterface(PPB_FLASH_INTERFACE));
  return !!flash_interface_;
}

void TestFlash::RunTests(const std::string& filter) {
  RUN_TEST(SetInstanceAlwaysOnTop, filter);
  RUN_TEST(GetProxyForURL, filter);
  RUN_TEST(MessageLoop, filter);
  RUN_TEST(GetLocalTimeZoneOffset, filter);
  RUN_TEST(GetCommandLineArgs, filter);
}

std::string TestFlash::TestSetInstanceAlwaysOnTop() {
  flash_interface_->SetInstanceAlwaysOnTop(instance_->pp_instance(), PP_TRUE);
  flash_interface_->SetInstanceAlwaysOnTop(instance_->pp_instance(), PP_FALSE);
  PASS();
}

std::string TestFlash::TestGetProxyForURL() {
  Var result(Var::PassRef(),
             flash_interface_->GetProxyForURL(instance_->pp_instance(),
                                              "http://127.0.0.1/foobar/"));
  ASSERT_TRUE(result.is_string());
  // Assume no one configures a proxy for localhost.
  ASSERT_EQ("DIRECT", result.AsString());

  result = Var(Var::PassRef(),
               flash_interface_->GetProxyForURL(instance_->pp_instance(),
                                                "http://www.google.com"));
  // Don't know what the proxy might be, but it should be a valid result.
  ASSERT_TRUE(result.is_string());

  result = Var(Var::PassRef(),
               flash_interface_->GetProxyForURL(instance_->pp_instance(),
                                                "file:///tmp"));
  ASSERT_TRUE(result.is_string());
  // Should get "DIRECT" for file:// URLs.
  ASSERT_EQ("DIRECT", result.AsString());

  result = Var(Var::PassRef(),
               flash_interface_->GetProxyForURL(instance_->pp_instance(),
                                                "this_isnt_an_url"));
  // Should be an error.
  ASSERT_TRUE(result.is_undefined());

  PASS();
}

std::string TestFlash::TestMessageLoop() {
  pp::CompletionCallback callback =
      callback_factory_.NewRequiredCallback(&TestFlash::QuitMessageLoopTask);
  pp::Module::Get()->core()->CallOnMainThread(0, callback);
  flash_interface_->RunMessageLoop(instance_->pp_instance());

  PASS();
}

std::string TestFlash::TestGetLocalTimeZoneOffset() {
  double result = flash_interface_->GetLocalTimeZoneOffset(
      instance_->pp_instance(), 1321491298.0);
  // The result depends on the local time zone, but +/- 14h from UTC should
  // cover the possibilities.
  ASSERT_TRUE(result >= -14 * 60 * 60);
  ASSERT_TRUE(result <= 14 * 60 * 60);

  PASS();
}

std::string TestFlash::TestGetCommandLineArgs() {
  Var result(Var::PassRef(),
             flash_interface_->GetCommandLineArgs(
                 pp::Module::Get()->pp_module()));
  ASSERT_TRUE(result.is_string());

  PASS();
}

void TestFlash::QuitMessageLoopTask(int32_t) {
  flash_interface_->QuitMessageLoop(instance_->pp_instance());
}
