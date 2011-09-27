// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_fullscreen.h"

#include <stdio.h>
#include <string.h>
#include <string>

#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/dev/ppb_fullscreen_dev.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/point.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Fullscreen);

namespace {

bool HasMidScreen(const pp::Rect& position, const pp::Size& screen_size) {
  static int32_t mid_x = screen_size.width() / 2;
  static int32_t mid_y = screen_size.height() / 2;
  return (position.Contains(mid_x, mid_y));
}

}  // namespace

TestFullscreen::TestFullscreen(TestingInstance* instance)
    : TestCase(instance),
      error_(),
      screen_mode_(instance),
      fullscreen_pending_(false),
      normal_pending_(false),
      saw_first_fullscreen_didchangeview(false),
      graphics2d_fullscreen_(instance, pp::Size(10, 10), false),
      graphics2d_normal_(instance, pp::Size(15, 15), false),
      set_fullscreen_true_callback_(instance->pp_instance()),
      fullscreen_callback_(instance->pp_instance()),
      normal_callback_(instance->pp_instance()) {
  screen_mode_.GetScreenSize(&screen_size_);
}

bool TestFullscreen::Init() {
  if (graphics2d_fullscreen_.is_null() && graphics2d_normal_.is_null())
    return false;
  return InitTestingInterface();
}

void TestFullscreen::RunTest() {
  RUN_TEST(GetScreenSize);
  RUN_TEST(NormalToFullscreenToNormal);
}

bool TestFullscreen::GotError() {
  return !error_.empty();
}

std::string TestFullscreen::Error() {
  std::string last_error = error_;
  error_.clear();
  return last_error;
}

std::string TestFullscreen::TestGetScreenSize() {
  if (screen_size_.width() < 320 || screen_size_.width() > 2560)
    return ReportError("screen_size.width()", screen_size_.width());
  if (screen_size_.height() < 200 || screen_size_.height() > 2048)
    return ReportError("screen_size.height()", screen_size_.height());
  PASS();
}

std::string TestFullscreen::TestNormalToFullscreenToNormal() {
  // 0. Start in normal mode.
  if (screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() at start", true);

  // 1. Switch to fullscreen.
  // This is only allowed within a context of a user gesture (e.g. mouse click).
  // The transition is asynchronous and ends at the next DidChangeView().
  // No graphics devices can be bound while in transition.
  instance_->RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
  // HandleInputEvent() will call SetFullscreen(true).
  // DidChangeView() will call the callback once in fullscreen mode.
  fullscreen_callback_.WaitForResult();
  if (GotError())
    return Error();
  if (fullscreen_pending_)
    return "fullscreen_pending_ has not been reset";
  if (!screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() in fullscreen", false);
  if (!instance_->BindGraphics(graphics2d_fullscreen_))
    return ReportError("BindGraphics() in fullscreen", false);

  // 2. Stay in fullscreen. No change.
  if (!screen_mode_.SetFullscreen(true))
    return ReportError("SetFullscreen(true) in fullscreen", false);
  if (!screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() in fullscreen^2", false);

  // 3. Switch to normal.
  // The transition is asynchronous and ends at DidChangeView().
  // No graphics devices can be bound while in transition.
  normal_pending_ = true;
  if (!screen_mode_.SetFullscreen(false))
    return ReportError("SetFullscreen(false) in fullscreen", false);
  if (instance_->BindGraphics(graphics2d_normal_))
    return ReportError("BindGraphics() in normal transition", true);
  if (!screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() in normal transition", false);
  // DidChangeView() will call the callback once out of fullscreen mode.
  normal_callback_.WaitForResult();
  if (normal_pending_)
    return "normal_pending_ has not been reset";
  if (screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() in normal", true);
  if (!instance_->BindGraphics(graphics2d_fullscreen_))
    return ReportError("BindGraphics() in normal", false);

  // 4. Stay in normal. No change.
  if (!screen_mode_.SetFullscreen(false))
    return ReportError("SetFullscreen(false) in normal", false);
  if (screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() in normal^2", true);

  PASS();
}

void TestFullscreen::FailFullscreenTest(const std::string& error) {
  screen_mode_.SetFullscreen(false);
  fullscreen_pending_ = false;
  error_ = error;
  pp::Module::Get()->core()->CallOnMainThread(0, fullscreen_callback_);
}

// Transition to fullscreen can only happen when processing a user gesture.
bool TestFullscreen::HandleInputEvent(const pp::InputEvent& event) {
  // We only let mouse events through and only mouse clicks count.
  if (event.GetType() != PP_INPUTEVENT_TYPE_MOUSEDOWN &&
      event.GetType() != PP_INPUTEVENT_TYPE_MOUSEUP)
    return false;
  // We got the gesture. No need to handle any more events.
  instance_->ClearInputEventRequest(PP_INPUTEVENT_CLASS_MOUSE);
  fullscreen_pending_ = true;
  if (!screen_mode_.SetFullscreen(true)) {
    FailFullscreenTest(ReportError("SetFullscreen(true) in normal", false));
    return false;
  }
  // No graphics devices can be bound while in transition.
  if (instance_->BindGraphics(graphics2d_fullscreen_)) {
    FailFullscreenTest(
        ReportError("BindGraphics() in fullscreen transition", true));
    return false;
  }
  if (screen_mode_.IsFullscreen()) {
    FailFullscreenTest(
        ReportError("IsFullscreen() in fullscreen transtion", true));
    return false;
  }
  // DidChangeView() will complete the transition to fullscreen.
  return false;
}

// Transitions to/from fullscreen is asynchornous ending at DidChangeView.
// When going to fullscreen, two DidChangeView calls are generated:
// one for moving the plugin to the middle of window and one for stretching
// the window and placing the plugin in the middle of the screen.
// Plugin size does not change.
void TestFullscreen::DidChangeView(const pp::Rect& position,
                                   const pp::Rect& clip) {
  if (normal_position_.IsEmpty())
    normal_position_ = position;

  if (fullscreen_pending_ && !saw_first_fullscreen_didchangeview) {
    saw_first_fullscreen_didchangeview = true;
    if (!screen_mode_.IsFullscreen())
      FailFullscreenTest("DidChangeView1 is not in fullscreen");
    if (position.size() != normal_position_.size())
      FailFullscreenTest("DidChangeView1 has different plugin size");
    // Wait for the 2nd DidChangeView.
  } else if (fullscreen_pending_) {
    fullscreen_pending_ = false;
    saw_first_fullscreen_didchangeview = false;
    if (!screen_mode_.IsFullscreen())
      FailFullscreenTest("DidChangeView2 is not in fullscreen");
    else if (!HasMidScreen(position, screen_size_))
      FailFullscreenTest("DidChangeView2 is not in the middle of the screen");
    else if (position.size() != normal_position_.size())
      FailFullscreenTest("DidChangeView2 has different plugin size");
    else
      pp::Module::Get()->core()->CallOnMainThread(0, fullscreen_callback_);
  } else if (normal_pending_) {
    normal_pending_ = false;
    if (screen_mode_.IsFullscreen())
      error_ = "DidChangeview is in fullscreen";
    else if (position != normal_position_)
      error_ = "DidChangeView position is not normal";
    pp::Module::Get()->core()->CallOnMainThread(0, normal_callback_);
  }
}
