// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PPAPI_TESTS_TEST_FLASH_FULLSCREEN_FOR_BROWSER_UI_H_
#define PPAPI_TESTS_TEST_FLASH_FULLSCREEN_FOR_BROWSER_UI_H_

#include <stdint.h>

#include <string>

#include "ppapi/cpp/graphics_3d.h"
#include "ppapi/cpp/private/flash_fullscreen.h"
#include "ppapi/cpp/size.h"
#include "ppapi/tests/test_case.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/utility/completion_callback_factory.h"

struct PPB_OpenGLES2;

// This is a special TestCase whose purpose is *not* to test the correctness of
// the Pepper APIs.  Instead, this is a simulated Flash plugin, used to place
// the browser window and other UI elements into Flash Fullscreen mode for
// layout, event, focus, etc. testing.  See
// chrome/browser/ui/exclusive_access/
//      flash_fullscreen_interactive_browsertest.cc.
//
// At start, this simulated Flash plugin will enter fullscreen and paint a green
// color fill.  From there, it will respond to mouse clicks or key presses by
// toggling its fill color between red and blue.  The browser test reads these
// color changes to detect the desired behavior.
class TestFlashFullscreenForBrowserUI : public TestCase {
 public:
  explicit TestFlashFullscreenForBrowserUI(TestingInstance* instance);
  ~TestFlashFullscreenForBrowserUI() override;

  // TestCase implementation.
  bool Init() override;
  void RunTests(const std::string& filter) override;
  void DidChangeView(const pp::View& view) override;
  bool HandleInputEvent(const pp::InputEvent& event) override;

 private:
  std::string TestEnterFullscreen();
  void RequestPaint();
  void DoPaint();
  void DidSwap(int32_t last_compositor_result);
  void SimulateUserGesture();
  bool GotError();
  std::string Error();
  void FailFullscreenTest(const std::string& error);

  // OpenGL ES2 interface.
  const PPB_OpenGLES2* opengl_es2_ = nullptr;

  std::string error_;

  pp::FlashFullscreen screen_mode_;
  NestedEvent view_change_event_;

  pp::Graphics3D graphics_3d_;
  pp::Size layer_size_;
  pp::Rect normal_position_;

  int num_trigger_events_ = 0;
  bool request_fullscreen_ = false;
  bool swap_pending_ = false;
  bool needs_paint_ = false;

  pp::CompletionCallbackFactory<TestFlashFullscreenForBrowserUI>
      callback_factory_;
};

#endif  // PPAPI_TESTS_TEST_FLASH_FULLSCREEN_FOR_BROWSER_UI_H_
