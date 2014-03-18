// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_talk_private.h"

#include <stdio.h>
#include <string.h>
#include <string>

#include "ppapi/c/pp_errors.h"
#include "ppapi/c/private/ppb_talk_private.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(TalkPrivate);

TestTalkPrivate::TestTalkPrivate(TestingInstance* instance)
    : TestCase(instance),
      talk_private_interface_1(NULL) {
}

bool TestTalkPrivate::Init() {
  if (!CheckTestingInterface()) {
    instance_->AppendError("Testing interface not available");
    return false;
  }

  talk_private_interface_1 = static_cast<const PPB_Talk_Private_1_0*>(
      pp::Module::Get()->GetBrowserInterface(PPB_TALK_PRIVATE_INTERFACE_1_0));

#if defined(__native_client__)
  if (talk_private_interface_1)
    instance_->AppendError("TalkPrivate interface is supported by NaCl");
#else
  if (!talk_private_interface_1)
    instance_->AppendError("TalkPrivate interface not available");
#endif
  return true;
}

void TestTalkPrivate::RunTests(const std::string& filter) {
  RUN_CALLBACK_TEST(TestTalkPrivate, GetPermission, filter);
}

std::string TestTalkPrivate::TestGetPermission() {
  if (!talk_private_interface_1) {
    PASS();
  }

  if (!testing_interface_->IsOutOfProcess()) {
    // We only support out-of-process access to this API, so skip in-process
    PASS();
  }

  // Under Ash, this will prompt the user so the test cannot run in an automated
  // fashion. To manually test under Ash, replace "!defined(USE_ASH)" with 1.
#if !defined(USE_ASH)
  PP_Resource talk_resource = talk_private_interface_1->Create(
      instance_->pp_instance());

  TestCompletionCallback callback(instance_->pp_instance(), callback_type());
  callback.WaitForResult(talk_private_interface_1->GetPermission(talk_resource,
      callback.GetCallback().pp_completion_callback()));
  CHECK_CALLBACK_BEHAVIOR(callback);

#if defined(USE_ASH)
  // Under Ash, this test will actually prompt the user and return either true
  // or false depending on their choice.
  if (callback.result() != 0 && callback.result() != 1)
    return "Unexpected result";
#else
  // Currently not implemented without Ash, bur always returns false.
  if (callback.result() != 0)
    return "Unexpected non-zero result";
#endif
#endif
  PASS();
}
