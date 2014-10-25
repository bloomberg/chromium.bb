// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_flash_fullscreen_for_browser_ui.h"

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
      num_trigger_events_(0),
      callback_factory_(this) {
  // This plugin should not be removed after this TestCase passes because
  // browser UI testing requires it to remain and to be interactive.
  instance_->set_remove_plugin(false);
}

TestFlashFullscreenForBrowserUI::~TestFlashFullscreenForBrowserUI() {
}

bool TestFlashFullscreenForBrowserUI::Init() {
  return CheckTestingInterface();
}

void TestFlashFullscreenForBrowserUI::RunTests(const std::string& filter) {
  RUN_TEST(EnterFullscreen, filter);
}

std::string TestFlashFullscreenForBrowserUI::TestEnterFullscreen() {
  if (screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() at start", true);

  view_change_event_.Reset();
  if (!screen_mode_.SetFullscreen(true))
    return ReportError("SetFullscreen(true) in normal", false);
  // DidChangeView() will call the callback once in fullscreen mode.
  view_change_event_.Wait();

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
  compositor_ = pp::Compositor(instance_);
  instance_->BindGraphics(compositor_);
  layer_size_ = view.GetRect().size();
  color_layer_ = compositor_.AddLayer();
  Paint(PP_OK);

  view_change_event_.Signal();
}

bool TestFlashFullscreenForBrowserUI::HandleInputEvent(
    const pp::InputEvent& event) {
  if (event.GetType() != PP_INPUTEVENT_TYPE_MOUSEDOWN &&
      event.GetType() != PP_INPUTEVENT_TYPE_CHAR) {
    return false;
  }

  ++num_trigger_events_;

  return true;
}

void TestFlashFullscreenForBrowserUI::Paint(int32_t last_compositor_result) {
  if (num_trigger_events_ == 0)
    color_layer_.SetColor(0.0f, 1.0f, 0.0f, 1.0f, layer_size_);
  else if (num_trigger_events_ % 2)
    color_layer_.SetColor(1.0f, 0.0f, 0.0f, 1.0f, layer_size_);
  else
    color_layer_.SetColor(0.0f, 0.0f, 1.0f, 1.0f, layer_size_);

  compositor_.CommitLayers(
      callback_factory_.NewCallback(&TestFlashFullscreenForBrowserUI::Paint));
}
