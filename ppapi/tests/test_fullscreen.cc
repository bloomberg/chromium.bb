// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_fullscreen.h"

#include <stdio.h>
#include <string.h>
#include <string>

#include "ppapi/c/dev/ppb_testing_dev.h"
#include "ppapi/c/ppb_fullscreen.h"
#include "ppapi/cpp/image_data.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/point.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(Fullscreen);

namespace {

const ColorPremul kOpaqueWhite = { 0xFF, 0xFF, 0xFF, 0xFF };
const ColorPremul kSheerRed = { 0x88, 0x88, 0x00, 0x00 };
const ColorPremul kSheerBlue = { 0x88, 0x00, 0x00, 0x88 };
const ColorPremul kOpaqueYellow = { 0xFF, 0xFF, 0xFF, 0x00 };
const int kBytesPerPixel = sizeof(uint32_t);  // 4 bytes for BGRA or RGBA.

uint32_t FormatColor(PP_ImageDataFormat format, ColorPremul color) {
  if (format == PP_IMAGEDATAFORMAT_BGRA_PREMUL)
    return (color.A << 24) | (color.R << 16) | (color.G << 8) | (color.B);
  else if (format == PP_IMAGEDATAFORMAT_RGBA_PREMUL)
    return (color.A << 24) | (color.B << 16) | (color.G << 8) | (color.R);
  else
    return 0;
}

bool HasMidScreen(const pp::Rect& position, const pp::Size& screen_size) {
  static int32_t mid_x = screen_size.width() / 2;
  static int32_t mid_y = screen_size.height() / 2;
  return (position.Contains(mid_x, mid_y));
}

void FlushCallback(void* user_data, int32_t result) {
}

}  // namespace

TestFullscreen::TestFullscreen(TestingInstance* instance)
    : TestCase(instance),
      error_(),
      screen_mode_(instance),
      fullscreen_pending_(false),
      normal_pending_(false),
      saw_first_fullscreen_didchangeview(false),
      set_fullscreen_true_callback_(instance->pp_instance()),
      fullscreen_callback_(instance->pp_instance()),
      normal_callback_(instance->pp_instance()) {
  screen_mode_.GetScreenSize(&screen_size_);
}

bool TestFullscreen::Init() {
  if (screen_size_.IsEmpty()) {
    instance_->AppendError("Failed to initialize screen_size_");
    return false;
  }
  graphics2d_ = pp::Graphics2D(instance_, screen_size_, true);
  if (!instance_->BindGraphics(graphics2d_)) {
    instance_->AppendError("Failed to initialize graphics2d_");
    return false;
  }
  return CheckTestingInterface();
}

void TestFullscreen::RunTests(const std::string& filter) {
  RUN_TEST(GetScreenSize, filter);
  RUN_TEST(NormalToFullscreenToNormal, filter);
}

bool TestFullscreen::GotError() {
  return !error_.empty();
}

std::string TestFullscreen::Error() {
  std::string last_error = error_;
  error_.clear();
  return last_error;
}

// TODO(polina): consider adding custom logic to JS for this test to
// get screen.width and screen.height and postMessage those to this code,
// so the dimensions can be checked exactly.
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
  if (screen_mode_.SetFullscreen(true))
    return ReportError("SetFullscreen(true) outside of user gesture", true);
  // Trigger another call to SetFullscreen(true) from HandleInputEvent().
  // The transition is asynchronous and ends at the next DidChangeView().
  instance_->RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
  SimulateUserGesture();
  // DidChangeView() will call the callback once in fullscreen mode.
  fullscreen_callback_.WaitForResult();
  if (GotError())
    return Error();
  if (fullscreen_pending_)
    return "fullscreen_pending_ has not been reset";
  if (!screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() in fullscreen", false);

  // 2. Stay in fullscreen. No change.
  if (screen_mode_.SetFullscreen(true))
    return ReportError("SetFullscreen(true) in fullscreen", true);
  if (!screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() in fullscreen^2", false);

  // 3. Switch to normal.
  // The transition is asynchronous and ends at DidChangeView().
  // No graphics devices can be bound while in transition.
  normal_pending_ = true;
  if (!screen_mode_.SetFullscreen(false))
    return ReportError("SetFullscreen(false) in fullscreen", false);
  // Normal is now pending, so additional requests should fail.
  if (screen_mode_.SetFullscreen(false))
    return ReportError("SetFullscreen(false) in normal pending", true);
  if (screen_mode_.SetFullscreen(true))
    return ReportError("SetFullscreen(true) in normal pending", true);
  if (!screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() in normal transition", false);
  // No graphics devices can be bound while in transition.
  if (instance_->BindGraphics(graphics2d_))
    return ReportError("BindGraphics() in normal transition", true);
  // DidChangeView() will call the callback once out of fullscreen mode.
  normal_callback_.WaitForResult();
  if (normal_pending_)
    return "normal_pending_ has not been reset";
  if (screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() in normal", true);

  // 4. Stay in normal. No change.
  if (screen_mode_.SetFullscreen(false))
    return ReportError("SetFullscreen(false) in normal", true);
  if (screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() in normal^2", true);

  PASS();
}

void TestFullscreen::SimulateUserGesture() {
  pp::Point plugin_center(
      normal_position_.x() + normal_position_.width() / 2,
      normal_position_.y() + normal_position_.height() / 2);
  pp::Point mouse_movement;
  pp::MouseInputEvent input_event(
      instance_,
      PP_INPUTEVENT_TYPE_MOUSEDOWN,
      0,  // time_stamp
      0,  // modifiers
      PP_INPUTEVENT_MOUSEBUTTON_LEFT,
      plugin_center,
      1,  // click_count
      mouse_movement);

  testing_interface_->SimulateInputEvent(instance_->pp_instance(),
                                         input_event.pp_resource());
}

void TestFullscreen::FailFullscreenTest(const std::string& error) {
  screen_mode_.SetFullscreen(false);
  fullscreen_pending_ = false;
  error_ = error;
  pp::Module::Get()->core()->CallOnMainThread(0, fullscreen_callback_);
}

void TestFullscreen::FailNormalTest(const std::string& error) {
  normal_pending_ = false;
  error_ = error;
  pp::Module::Get()->core()->CallOnMainThread(0, normal_callback_);
}

void TestFullscreen::PassFullscreenTest() {
  fullscreen_pending_ = false;
  pp::Module::Get()->core()->CallOnMainThread(0, fullscreen_callback_);
}

void TestFullscreen::PassNormalTest() {
  normal_pending_ = false;
  pp::Module::Get()->core()->CallOnMainThread(0, normal_callback_);
}

// Transition to fullscreen can only happen when processing a user gesture.
bool TestFullscreen::HandleInputEvent(const pp::InputEvent& event) {
  // We only let mouse events through and only mouse clicks count.
  if (event.GetType() != PP_INPUTEVENT_TYPE_MOUSEDOWN &&
      event.GetType() != PP_INPUTEVENT_TYPE_MOUSEUP)
    return false;
  // We got the gesture. No need to handle any more events.
  instance_->ClearInputEventRequest(PP_INPUTEVENT_CLASS_MOUSE);
  if (screen_mode_.IsFullscreen()) {
    FailFullscreenTest(
        ReportError("IsFullscreen() before fullscreen transition", true));
    return false;
  }
  fullscreen_pending_ = true;
  if (!screen_mode_.SetFullscreen(true)) {
    FailFullscreenTest(ReportError("SetFullscreen(true) in normal", false));
    return false;
  }
  // Fullscreen is now pending, so additional requests should fail.
  if (screen_mode_.SetFullscreen(true)) {
    FailFullscreenTest(
        ReportError("SetFullscreen(true) in fullscreen pending", true));
    return false;
  }
  if (screen_mode_.SetFullscreen(false)) {
    FailFullscreenTest(
        ReportError("SetFullscreen(false) in fullscreen pending", true));
  }
  if (screen_mode_.IsFullscreen()) {
    FailFullscreenTest(
        ReportError("IsFullscreen() in fullscreen transition", true));
    return false;
  }
  // No graphics devices can be bound while in transition.
  if (instance_->BindGraphics(graphics2d_)) {
    FailFullscreenTest(
        ReportError("BindGraphics() in fullscreen transition", true));
    return false;
  }
  // DidChangeView() will complete the transition to fullscreen.
  return false;
}

bool TestFullscreen::PaintPlugin(pp::Size size, ColorPremul color) {
  PP_ImageDataFormat image_format = pp::ImageData::GetNativeImageDataFormat();
  uint32_t pixel_color = FormatColor(image_format, color);
  if (pixel_color == 0)
    return false;
  pp::Point origin(0, 0);

  pp::ImageData image(instance_, image_format, size, false);
  if (image.is_null())
    return false;
  uint32_t* pixels = static_cast<uint32_t*>(image.data());
  int num_pixels = image.stride() / kBytesPerPixel * image.size().height();
  for (int i = 0; i < num_pixels; i++)
    pixels[i] = pixel_color;
  graphics2d_.PaintImageData(image, origin);
  pp::CompletionCallback cc(FlushCallback, NULL);
  if (graphics2d_.Flush(cc) != PP_OK_COMPLETIONPENDING)
    return false;

  // Confirm that painting was successful.
  pp::ImageData readback(instance_, image_format, graphics2d_.size(), false);
  if (readback.is_null())
    return false;
  if (PP_TRUE != testing_interface_->ReadImageData(graphics2d_.pp_resource(),
                                                   readback.pp_resource(),
                                                   &origin.pp_point()))
    return false;
  bool error = false;
  for (int y = 0; y < size.height() && !error; y++) {
    for (int x = 0; x < size.width() && !error; x++) {
      uint32_t* readback_color = readback.GetAddr32(pp::Point(x, y));
      if (pixel_color != *readback_color)
        return false;
    }
  }
  return true;
}

// Transitions to/from fullscreen is asynchornous ending at DidChangeView.
// When going to fullscreen, two DidChangeView calls are generated:
// one for moving the plugin to the middle of window and one for stretching
// the window and placing the plugin in the middle of the screen.
// This is not something we advertise to users and not something they should
// rely on. But the test checks for these, so we know when the underlying
// implementation changes.
//
// WebKit does not change the plugin size, but Pepper does explicitly set
// it to screen width and height when SetFullscreen(true) is called and
// resets it back when ViewChanged is received indicating that we exited
// fullscreen.
//
// NOTE: The number of DidChangeView calls for <object> might be different.
void TestFullscreen::DidChangeView(const pp::View& view) {
  pp::Rect position = view.GetRect();
  pp::Rect clip = view.GetClipRect();

  if (normal_position_.IsEmpty()) {
    normal_position_ = position;
    if (!PaintPlugin(position.size(), kSheerRed))
      error_ = "Failed to initialize plugin image";
  }

  if (fullscreen_pending_ && !saw_first_fullscreen_didchangeview) {
    saw_first_fullscreen_didchangeview = true;
    if (screen_mode_.IsFullscreen())
      FailFullscreenTest("DidChangeView1 is in fullscreen");
    else if (position.size() != screen_size_)
      FailFullscreenTest("DidChangeView1 does not have screen size");
    // Wait for the 2nd DidChangeView.
  } else if (fullscreen_pending_) {
    saw_first_fullscreen_didchangeview = false;
    if (!screen_mode_.IsFullscreen())
      FailFullscreenTest("DidChangeView2 is not in fullscreen");
    else if (!HasMidScreen(position, screen_size_))
      FailFullscreenTest("DidChangeView2 is not in the middle of the screen");
    else if (position.size() != screen_size_)
      FailFullscreenTest("DidChangeView2 does not have screen size");
    // NOTE: we cannot reliably test for clip size being equal to the screen
    // because it might be affected by JS console, info bars, etc.
    else if (!instance_->BindGraphics(graphics2d_))
      FailFullscreenTest("Failed to BindGraphics() in fullscreen");
    else if (!PaintPlugin(position.size(), kOpaqueYellow))
      FailFullscreenTest("Failed to paint plugin image in fullscreen");
    else
      PassFullscreenTest();
  } else if (normal_pending_) {
    normal_pending_ = false;
    if (screen_mode_.IsFullscreen())
      FailNormalTest("DidChangeview is in fullscreen");
    else if (position != normal_position_)
      FailNormalTest("DidChangeView position is not normal");
    else if (!instance_->BindGraphics(graphics2d_))
      FailNormalTest("Failed to BindGraphics() in normal");
    else if (!PaintPlugin(position.size(), kSheerBlue))
      FailNormalTest("Failed to paint plugin image in normal");
    else
      PassNormalTest();
  }
}
