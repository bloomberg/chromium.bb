// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PAPPI_TESTS_TEST_FULLSCREEN_H_
#define PAPPI_TESTS_TEST_FULLSCREEN_H_

#include <string>

#include "ppapi/cpp/fullscreen.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "ppapi/tests/test_case.h"
#include "ppapi/tests/test_utils.h"

namespace pp {
class InputEvent;
}  // namespace pp

struct ColorPremul { uint32_t A, R, G, B; };  // Use premultipled Alpha.

class TestFullscreen : public TestCase {
 public:
  explicit TestFullscreen(TestingInstance* instance);

  // TestCase implementation.
  virtual bool Init();
  virtual void RunTests(const std::string& filter);
  virtual bool HandleInputEvent(const pp::InputEvent& event);
  virtual void DidChangeView(const pp::View& view);

 private:
  std::string TestGetScreenSize();
  std::string TestNormalToFullscreenToNormal();

  void SimulateUserGesture();
  void FailFullscreenTest(const std::string& error);
  void FailNormalTest(const std::string& error);
  void PassFullscreenTest();
  void PassNormalTest();
  bool PaintPlugin(pp::Size size, ColorPremul color);

  bool GotError();
  std::string Error();

  std::string error_;

  pp::Fullscreen screen_mode_;
  pp::Size screen_size_;
  pp::Rect normal_position_;

  bool fullscreen_pending_;
  bool normal_pending_;
  bool saw_first_fullscreen_didchangeview;
  pp::Graphics2D graphics2d_;
  TestCompletionCallback set_fullscreen_true_callback_;
  TestCompletionCallback fullscreen_callback_;
  TestCompletionCallback normal_callback_;
};

#endif  // PAPPI_TESTS_TEST_FULLSCREEN_H_
