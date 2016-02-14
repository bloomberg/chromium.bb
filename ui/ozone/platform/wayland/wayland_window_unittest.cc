// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wayland-server-core.h>
#include <xdg-shell-unstable-v5-server-protocol.h>

#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/fake_server.h"
#include "ui/ozone/platform/wayland/mock_platform_window_delegate.h"
#include "ui/ozone/platform/wayland/wayland_display.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

using ::testing::Eq;
using ::testing::Mock;
using ::testing::SaveArg;
using ::testing::StrEq;
using ::testing::_;

namespace ui {

TEST(WaylandWindowInitializeTest, Initialize) {
  wl::FakeServer server;
  ASSERT_TRUE(server.Start());
  WaylandDisplay display;
  ASSERT_TRUE(display.Initialize());
  MockPlatformWindowDelegate delegate;
  gfx::AcceleratedWidget widget = gfx::kNullAcceleratedWidget;
  EXPECT_CALL(delegate, OnAcceleratedWidgetAvailable(_, _))
      .WillOnce(SaveArg<0>(&widget));
  WaylandWindow window(&delegate, &display, gfx::Rect(0, 0, 800, 600));
  ASSERT_TRUE(window.Initialize());
  EXPECT_EQ(widget, window.GetWidget());
  wl_display_roundtrip(display.display());

  server.Pause();

  EXPECT_TRUE(server.GetObject<wl::MockSurface>(window.GetWidget()));
  server.Resume();
}

class WaylandWindowTest : public testing::Test {
 public:
  WaylandWindowTest()
      : window(&delegate, &display, gfx::Rect(0, 0, 800, 600)) {}

  void SetUp() override {
    ASSERT_TRUE(server.Start());
    ASSERT_TRUE(display.Initialize());
    ASSERT_TRUE(window.Initialize());
    wl_display_roundtrip(display.display());

    server.Pause();

    auto surface = server.GetObject<wl::MockSurface>(window.GetWidget());
    ASSERT_TRUE(surface);
    xdg_surface = surface->xdg_surface.get();
    ASSERT_TRUE(xdg_surface);
    initialized = true;
  }

  void TearDown() override {
    server.Resume();
    if (initialized)
      wl_display_roundtrip(display.display());
  }

  void Sync() {
    server.Resume();
    wl_display_roundtrip(display.display());
    server.Pause();
  }

 private:
  wl::FakeServer server;
  bool initialized = false;

 protected:
  WaylandDisplay display;

  MockPlatformWindowDelegate delegate;
  WaylandWindow window;

  wl::MockXdgSurface* xdg_surface;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandWindowTest);
};

TEST_F(WaylandWindowTest, SetTitle) {
  EXPECT_CALL(*xdg_surface, SetTitle(StrEq("hello")));
  window.SetTitle(base::ASCIIToUTF16("hello"));
}

TEST_F(WaylandWindowTest, Maximize) {
  EXPECT_CALL(*xdg_surface, SetMaximized());
  window.Maximize();
}

TEST_F(WaylandWindowTest, Minimize) {
  EXPECT_CALL(*xdg_surface, SetMinimized());
  window.Minimize();
}

TEST_F(WaylandWindowTest, Restore) {
  EXPECT_CALL(*xdg_surface, UnsetMaximized());
  window.Restore();
}

TEST_F(WaylandWindowTest, ConfigureEvent) {
  wl_array states;
  wl_array_init(&states);
  xdg_surface_send_configure(xdg_surface->resource(), 1000, 1000, &states, 12);
  xdg_surface_send_configure(xdg_surface->resource(), 1500, 1000, &states, 13);

  // Make sure that the implementation does not call OnBoundsChanged for each
  // configure event if it receives multiple in a row.
  EXPECT_CALL(delegate, OnBoundsChanged(_)).Times(0);
  Sync();
  Mock::VerifyAndClearExpectations(&delegate);

  EXPECT_CALL(delegate, OnBoundsChanged(Eq(gfx::Rect(0, 0, 1500, 1000))));
  EXPECT_CALL(*xdg_surface, AckConfigure(13));
  window.ApplyPendingBounds();
}

}  // namespace ui
