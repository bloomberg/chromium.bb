// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_flash_message_loop.h"

#include "ppapi/c/pp_macros.h"
#include "ppapi/cpp/core.h"
#include "ppapi/cpp/logging.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/private/flash_message_loop.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(FlashMessageLoop);

TestFlashMessageLoop::TestFlashMessageLoop(TestingInstance* instance)
    : TestCase(instance),
      message_loop_(NULL),
      callback_factory_(this) {
}

TestFlashMessageLoop::~TestFlashMessageLoop() {
  PP_DCHECK(!message_loop_);
}

void TestFlashMessageLoop::RunTests(const std::string& filter) {
  RUN_TEST(Basics, filter);
  RUN_TEST(RunWithoutQuit, filter);
}

std::string TestFlashMessageLoop::TestBasics() {
  message_loop_ = new pp::flash::MessageLoop(instance_);

  pp::CompletionCallback callback = callback_factory_.NewCallback(
      &TestFlashMessageLoop::QuitMessageLoopTask);
  pp::Module::Get()->core()->CallOnMainThread(0, callback);
  int32_t result = message_loop_->Run();

  ASSERT_TRUE(message_loop_);
  delete message_loop_;
  message_loop_ = NULL;

  ASSERT_EQ(PP_OK, result);
  PASS();
}

std::string TestFlashMessageLoop::TestRunWithoutQuit() {
  message_loop_ = new pp::flash::MessageLoop(instance_);

  pp::CompletionCallback callback = callback_factory_.NewCallback(
      &TestFlashMessageLoop::DestroyMessageLoopResourceTask);
  pp::Module::Get()->core()->CallOnMainThread(0, callback);
  int32_t result = message_loop_->Run();

  if (message_loop_) {
    delete message_loop_;
    message_loop_ = NULL;
    ASSERT_TRUE(false);
  }

  ASSERT_EQ(PP_ERROR_ABORTED, result);
  PASS();
}

void TestFlashMessageLoop::QuitMessageLoopTask(int32_t unused) {
  if (message_loop_)
    message_loop_->Quit();
  else
    PP_NOTREACHED();
}

void TestFlashMessageLoop::DestroyMessageLoopResourceTask(int32_t unused) {
  if (message_loop_) {
    delete message_loop_;
    message_loop_ = NULL;
  } else {
    PP_NOTREACHED();
  }
}
