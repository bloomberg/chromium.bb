// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_FLASH_FULLSCREEN_H_
#define PPAPI_TESTS_TEST_FLASH_FULLSCREEN_H_

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
  virtual void RunTests(const std::string& filter);
  virtual void DidChangeView(const pp::View& view);

 private:
  std::string TestGetScreenSize();
  std::string TestNormalToFullscreenToNormal();

  pp::FlashFullscreen screen_mode_;
  pp::Size screen_size_;

  bool fullscreen_pending_;
  bool normal_pending_;
  NestedEvent fullscreen_event_;
  NestedEvent normal_event_;
};

#endif  // PPAPI_TESTS_TEST_FLASH_FULLSCREEN_H_
