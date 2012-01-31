// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_FLASH_MESSAGE_LOOP_H_
#define PAPPI_TESTS_TEST_FLASH_MESSAGE_LOOP_H_

#include <string>

#include "ppapi/c/pp_stdint.h"
#include "ppapi/tests/test_case.h"
#include "ppapi/utility/completion_callback_factory.h"

namespace pp {
namespace flash {
class MessageLoop;
}
}

class TestFlashMessageLoop : public TestCase {
 public:
  explicit TestFlashMessageLoop(TestingInstance* instance);
  virtual ~TestFlashMessageLoop();

  // TestCase implementation.
  virtual void RunTests(const std::string& filter);

 private:
  std::string TestBasics();
  std::string TestRunWithoutQuit();

  void QuitMessageLoopTask(int32_t unused);
  void DestroyMessageLoopResourceTask(int32_t unused);

  pp::flash::MessageLoop* message_loop_;
  pp::CompletionCallbackFactory<TestFlashMessageLoop> callback_factory_;
};

#endif  // PAPPI_TESTS_TEST_FLASH_MESSAGE_LOOP_H_
