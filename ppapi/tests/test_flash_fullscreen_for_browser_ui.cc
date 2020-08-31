// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_flash_fullscreen_for_browser_ui.h"

#include <GLES2/gl2.h>

#include "ppapi/c/ppb_opengles2.h"
#include "ppapi/cpp/input_event.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(FlashFullscreenForBrowserUI);

TestFlashFullscreenForBrowserUI::
    TestFlashFullscreenForBrowserUI(TestingInstance* instance)
    : TestCase(instance),
      screen_mode_(instance),
      view_change_event_(instance->pp_instance()),
      callback_factory_(this) {
  // This plugin should not be removed after this TestCase passes because
  // browser UI testing requires it to remain and to be interactive.
  instance_->set_remove_plugin(false);
}

TestFlashFullscreenForBrowserUI::~TestFlashFullscreenForBrowserUI() {
}

bool TestFlashFullscreenForBrowserUI::Init() {
  opengl_es2_ = static_cast<const PPB_OpenGLES2*>(
      pp::Module::Get()->GetBrowserInterface(PPB_OPENGLES2_INTERFACE));
  return opengl_es2_ && CheckTestingInterface();
}

void TestFlashFullscreenForBrowserUI::RunTests(const std::string& filter) {
  RUN_TEST(EnterFullscreen, filter);
}

std::string TestFlashFullscreenForBrowserUI::TestEnterFullscreen() {
  if (screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() at start", true);

  // This is only allowed within a contet of a user gesture (e.g. mouse click).
  if (screen_mode_.SetFullscreen(true))
    return ReportError("SetFullscreen(true) outside of user gesture", true);

  int32_t attribs[] = {PP_GRAPHICS3DATTRIB_RED_SIZE,     8,
                       PP_GRAPHICS3DATTRIB_GREEN_SIZE,   8,
                       PP_GRAPHICS3DATTRIB_BLUE_SIZE,    8,
                       PP_GRAPHICS3DATTRIB_ALPHA_SIZE,   8,
                       PP_GRAPHICS3DATTRIB_DEPTH_SIZE,   0,
                       PP_GRAPHICS3DATTRIB_STENCIL_SIZE, 0,
                       PP_GRAPHICS3DATTRIB_WIDTH,        layer_size_.width(),
                       PP_GRAPHICS3DATTRIB_HEIGHT,       layer_size_.height(),
                       PP_GRAPHICS3DATTRIB_NONE};
  graphics_3d_ = pp::Graphics3D(instance_, attribs);
  instance_->BindGraphics(graphics_3d_);

  // Trigger another call to SetFullscreen(true) from HandleInputEvent().
  // The transition is asynchronous and ends at the next DidChangeView().
  view_change_event_.Reset();
  request_fullscreen_ = true;
  instance_->RequestInputEvents(PP_INPUTEVENT_CLASS_MOUSE);
  SimulateUserGesture();
  // DidChangeView() will call the callback once in fullscreen mode.
  view_change_event_.Wait();
  if (GotError())
    return Error();

  if (!screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() in fullscreen", false);

  const int32_t result =
      instance_->RequestFilteringInputEvents(PP_INPUTEVENT_CLASS_MOUSE |
                                             PP_INPUTEVENT_CLASS_KEYBOARD);
  if (result != PP_OK)
    return ReportError("RequestFilteringInputEvents() failed", result);

  PASS();
}

void TestFlashFullscreenForBrowserUI::DidChangeView(const pp::View& view) {
  layer_size_ = view.GetRect().size();
  if (normal_position_.IsEmpty())
    normal_position_ = view.GetRect();

  if (!graphics_3d_.is_null()) {
    graphics_3d_.ResizeBuffers(layer_size_.width(), layer_size_.height());
    RequestPaint();
  }

  view_change_event_.Signal();
}

void TestFlashFullscreenForBrowserUI::SimulateUserGesture() {
  pp::Point plugin_center(
      normal_position_.x() + normal_position_.width() / 2,
      normal_position_.y() + normal_position_.height() / 2);
  pp::Point mouse_movement;
  pp::MouseInputEvent input_event(
      instance_,
      PP_INPUTEVENT_TYPE_MOUSEDOWN,
      NowInTimeTicks(),  // time_stamp
      0,  // modifiers
      PP_INPUTEVENT_MOUSEBUTTON_LEFT,
      plugin_center,
      1,  // click_count
      mouse_movement);

  testing_interface_->SimulateInputEvent(instance_->pp_instance(),
                                         input_event.pp_resource());
}

bool TestFlashFullscreenForBrowserUI::GotError() {
  return !error_.empty();
}

std::string TestFlashFullscreenForBrowserUI::Error() {
  std::string last_error = error_;
  error_.clear();
  return last_error;
}

void TestFlashFullscreenForBrowserUI::FailFullscreenTest(
    const std::string& error) {
  error_ = error;
  view_change_event_.Signal();
}

bool TestFlashFullscreenForBrowserUI::HandleInputEvent(
    const pp::InputEvent& event) {
  if (event.GetType() != PP_INPUTEVENT_TYPE_MOUSEDOWN &&
      event.GetType() != PP_INPUTEVENT_TYPE_MOUSEUP &&
      event.GetType() != PP_INPUTEVENT_TYPE_CHAR) {
    return false;
  }

  if (request_fullscreen_) {
    instance_->ClearInputEventRequest(PP_INPUTEVENT_CLASS_MOUSE);
    if (screen_mode_.IsFullscreen()) {
      FailFullscreenTest(
          ReportError("IsFullscreen() before fullscreen transition", true));
      return false;
    }
    request_fullscreen_ = false;
    if (!screen_mode_.SetFullscreen(true)) {
      FailFullscreenTest(
          ReportError("SetFullscreen(true) in normal", false));
      return false;
    }
    // DidChangeView() will complete the transition to fullscreen.
    return false;
  }

  ++num_trigger_events_;
  RequestPaint();

  return true;
}

void TestFlashFullscreenForBrowserUI::RequestPaint() {
  if (swap_pending_)
    needs_paint_ = true;
  else
    DoPaint();
}

void TestFlashFullscreenForBrowserUI::DoPaint() {
  if (num_trigger_events_ == 0)
    opengl_es2_->ClearColor(graphics_3d_.pp_resource(), 0.0f, 1.0f, 0.0f, 1.0f);
  else if (num_trigger_events_ % 2)
    opengl_es2_->ClearColor(graphics_3d_.pp_resource(), 1.0f, 0.0f, 0.0f, 1.0f);
  else
    opengl_es2_->ClearColor(graphics_3d_.pp_resource(), 0.0f, 0.0f, 1.0f, 1.0f);

  opengl_es2_->Clear(graphics_3d_.pp_resource(), GL_COLOR_BUFFER_BIT);
  swap_pending_ = true;
  graphics_3d_.SwapBuffers(
      callback_factory_.NewCallback(&TestFlashFullscreenForBrowserUI::DidSwap));
}

void TestFlashFullscreenForBrowserUI::DidSwap(int32_t last_compositor_result) {
  swap_pending_ = false;
  if (needs_paint_) {
    needs_paint_ = false;
    DoPaint();
  }
}
