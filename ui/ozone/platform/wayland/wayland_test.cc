// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_test.h"

namespace ui {

WaylandTest::WaylandTest()
    : window(&delegate, &display, gfx::Rect(0, 0, 800, 600)) {}

WaylandTest::~WaylandTest() {}

void WaylandTest::SetUp() {
  ASSERT_TRUE(server.Start());
  ASSERT_TRUE(display.Initialize());
  ASSERT_TRUE(window.Initialize());
  wl_display_roundtrip(display.display());

  server.Pause();

  surface = server.GetObject<wl::MockSurface>(window.GetWidget());
  ASSERT_TRUE(surface);
  initialized = true;
}

void WaylandTest::TearDown() {
  server.Resume();
  if (initialized)
    wl_display_roundtrip(display.display());
}

void WaylandTest::Sync() {
  server.Resume();
  wl_display_roundtrip(display.display());
  server.Pause();
}

}  // namespace ui
