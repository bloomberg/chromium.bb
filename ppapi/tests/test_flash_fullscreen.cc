// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ppapi/tests/test_flash_fullscreen.h"

#include <stdio.h>
#include <string.h>
#include <string>

#include "ppapi/c/private/ppb_flash_fullscreen.h"
#include "ppapi/cpp/graphics_2d.h"
#include "ppapi/cpp/instance.h"
#include "ppapi/cpp/module.h"
#include "ppapi/cpp/point.h"
#include "ppapi/cpp/private/flash_fullscreen.h"
#include "ppapi/cpp/rect.h"
#include "ppapi/cpp/size.h"
#include "ppapi/tests/test_utils.h"
#include "ppapi/tests/testing_instance.h"

REGISTER_TEST_CASE(FlashFullscreen);

namespace {

bool IsFullscreenView(const pp::Rect& position,
                      const pp::Rect& clip,
                      const pp::Size& screen_size) {
  return (position.point() == pp::Point(0, 0) &&
          position.size() == screen_size &&
          clip.point() == pp::Point(0, 0) &&
          clip.size() == screen_size);
}

}  // namespace

TestFlashFullscreen::TestFlashFullscreen(TestingInstance* instance)
    : TestCase(instance),
      screen_mode_(instance),
      fullscreen_pending_(false),
      normal_pending_(false),
      fullscreen_event_(instance->pp_instance()),
      normal_event_(instance->pp_instance()) {
  screen_mode_.GetScreenSize(&screen_size_);
}

bool TestFlashFullscreen::Init() {
  return CheckTestingInterface();
}

void TestFlashFullscreen::RunTests(const std::string& filter) {
  RUN_TEST(GetScreenSize, filter);
  RUN_TEST(NormalToFullscreenToNormal, filter);
}

std::string TestFlashFullscreen::TestGetScreenSize() {
  if (screen_size_.width() < 320 || screen_size_.width() > 2560)
    return ReportError("screen_size.width()", screen_size_.width());
  if (screen_size_.height() < 200 || screen_size_.height() > 2048)
    return ReportError("screen_size.height()", screen_size_.height());
  PASS();
}

std::string TestFlashFullscreen::TestNormalToFullscreenToNormal() {
  // 0. Start in normal mode.
  if (screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() at start", true);

  // 1. Switch to fullscreen.
  // The transition is asynchronous and ends at the next DidChangeView().
  // No graphics devices can be bound while in transition.
  fullscreen_pending_ = true;
  if (!screen_mode_.SetFullscreen(true))
    return ReportError("SetFullscreen(true) in normal", false);
  pp::Graphics2D graphics2d_fullscreen(instance_, pp::Size(10, 10), false);
  if (graphics2d_fullscreen.is_null())
    return "Failed to create graphics2d_fullscreen";
  // The out-of-process proxy is asynchronous, so testing for the following
  // conditions is flaky and can only be done reliably in-process.
  if (!testing_interface_->IsOutOfProcess()) {
    if (instance_->BindGraphics(graphics2d_fullscreen))
      return ReportError("BindGraphics() in fullscreen transition", true);
    if (screen_mode_.IsFullscreen())
      return ReportError("IsFullscreen() in fullscreen transtion", true);
  }

  // DidChangeView() will call the callback once in fullscreen mode.
  fullscreen_event_.Wait();
  if (fullscreen_pending_)
    return "fullscreen_pending_ has not been reset";
  if (!screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() in fullscreen", false);
  if (!instance_->BindGraphics(graphics2d_fullscreen))
    return ReportError("BindGraphics() in fullscreen", false);

  // 2. Stay in fullscreen. No change.
  if (!screen_mode_.SetFullscreen(true))
    return ReportError("SetFullscreen(true) in fullscreen", false);
  if (!screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() in fullscreen^2", false);

  // 3. Switch to normal.
  // The transition is synchronous in-process and asynchornous out-of-process
  // because proxied IsFullscreen saves a roundtrip by relying on information
  // communicated via a previous call to DidChangeView.
  // Graphics devices can be bound right away.
  normal_pending_ = true;
  if (!screen_mode_.SetFullscreen(false))
    return ReportError("SetFullscreen(false) in fullscreen", false);
  pp::Graphics2D graphics2d_normal(instance_, pp::Size(15, 15), false);
  if (graphics2d_normal.is_null())
    return "Failed to create graphics2d_normal";
  if (!instance_->BindGraphics(graphics2d_normal))
    return ReportError("BindGraphics() in normal transition", false);
  if (testing_interface_->IsOutOfProcess()) {
    if (!screen_mode_.IsFullscreen())
      return ReportError("IsFullscreen() in normal transition", false);
    normal_event_.Wait();
    if (normal_pending_)
      return "normal_pending_ has not been reset";
  }
  if (screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() in normal", true);

  // 4. Stay in normal. No change.
  if (!screen_mode_.SetFullscreen(false))
    return ReportError("SetFullscreen(false) in normal", false);
  if (screen_mode_.IsFullscreen())
    return ReportError("IsFullscreen() in normal^2", true);

  PASS();
}

// Transition to fullscreen is asynchornous ending at DidChangeView.
// Transition to normal is synchronous in-process and asynchronous
// out-of-process ending at DidChangeView.
void TestFlashFullscreen::DidChangeView(const pp::View& view) {
  pp::Rect position = view.GetRect();
  pp::Rect clip = view.GetClipRect();
  if (fullscreen_pending_ && IsFullscreenView(position, clip, screen_size_)) {
    fullscreen_pending_ = false;
    fullscreen_event_.Signal();
  } else if (normal_pending_ &&
             !IsFullscreenView(position, clip, screen_size_)) {
    normal_pending_ = false;
    if (testing_interface_->IsOutOfProcess())
      normal_event_.Signal();
  }
}
