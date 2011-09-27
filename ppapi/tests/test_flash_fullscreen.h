// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_FLASH_FULLSCREEN_H_
#define PAPPI_TESTS_TEST_FLASH_FULLSCREEN_H_

#include <string>

#include "ppapi/cpp/private/flash_fullscreen.h"
#include "ppapi/cpp/size.h"
#include "ppapi/tests/test_case.h"
#include "ppapi/tests/test_utils.h"

namespace pp {
class Rect;
}  // namespace pp

class TestFlashFullscreen : public TestCase {
 public:
  explicit TestFlashFullscreen(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTest();
  virtual void DidChangeView(const pp::Rect& position, const pp::Rect& clip);

 private:
  std::string TestGetScreenSize();
  std::string TestNormalToFullscreenToNormal();

  pp::FlashFullscreen screen_mode_;
  pp::Size screen_size_;

  bool fullscreen_pending_;
  bool normal_pending_;
  TestCompletionCallback fullscreen_callback_;
  TestCompletionCallback normal_callback_;
};

#endif  // PAPPI_TESTS_TEST_FLASH_FULLSCREEN_H_
