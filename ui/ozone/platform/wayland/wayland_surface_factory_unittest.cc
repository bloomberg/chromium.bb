// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/ozone/platform/wayland/fake_server.h"
#include "ui/ozone/platform/wayland/mock_platform_window_delegate.h"
#include "ui/ozone/platform/wayland/wayland_display.h"
#include "ui/ozone/platform/wayland/wayland_surface_factory.h"
#include "ui/ozone/platform/wayland/wayland_window.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

using ::testing::Expectation;
using ::testing::Mock;
using ::testing::SaveArg;
using ::testing::_;

namespace ui {

class WaylandSurfaceFactoryTest : public testing::Test {
 public:
  WaylandSurfaceFactoryTest()
      : surface_factory(&display),
        window(&delegate, &display, gfx::Rect(0, 0, 80, 60)) {}

  ~WaylandSurfaceFactoryTest() override {}

  void SetUp() override {
    ASSERT_TRUE(server.Start());
    ASSERT_TRUE(display.Initialize());
    ASSERT_TRUE(window.Initialize());
    wl_display_roundtrip(display.display());

    server.Pause();

    surface = server.GetObject<wl::MockSurface>(window.GetWidget());
    ASSERT_TRUE(surface);
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
  WaylandSurfaceFactory surface_factory;
  MockPlatformWindowDelegate delegate;
  WaylandWindow window;
  wl::MockSurface* surface;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandSurfaceFactoryTest);
};

TEST_F(WaylandSurfaceFactoryTest, Canvas) {
  auto canvas = surface_factory.CreateCanvasForWidget(window.GetWidget());
  ASSERT_TRUE(canvas);

  canvas->GetSurface();
  canvas->PresentCanvas(gfx::Rect(5, 10, 20, 15));

  Expectation damage = EXPECT_CALL(*surface, Damage(5, 10, 20, 15));
  wl_resource* buffer_resource = nullptr;
  Expectation attach = EXPECT_CALL(*surface, Attach(_, 0, 0))
                           .WillOnce(SaveArg<0>(&buffer_resource));
  EXPECT_CALL(*surface, Commit()).After(damage, attach);

  Sync();

  ASSERT_TRUE(buffer_resource);
  wl_shm_buffer* buffer = wl_shm_buffer_get(buffer_resource);
  ASSERT_TRUE(buffer);
  EXPECT_EQ(wl_shm_buffer_get_width(buffer), 80);
  EXPECT_EQ(wl_shm_buffer_get_height(buffer), 60);

  // TODO(forney): We could check that the contents match something drawn to the
  // SkSurface above.
}

TEST_F(WaylandSurfaceFactoryTest, CanvasResize) {
  auto canvas = surface_factory.CreateCanvasForWidget(window.GetWidget());
  ASSERT_TRUE(canvas);

  canvas->GetSurface();
  canvas->ResizeCanvas(gfx::Size(100, 50));
  canvas->GetSurface();
  canvas->PresentCanvas(gfx::Rect(0, 0, 100, 50));

  Expectation damage = EXPECT_CALL(*surface, Damage(0, 0, 100, 50));
  wl_resource* buffer_resource = nullptr;
  Expectation attach = EXPECT_CALL(*surface, Attach(_, 0, 0))
                           .WillOnce(SaveArg<0>(&buffer_resource));
  EXPECT_CALL(*surface, Commit()).After(damage, attach);

  Sync();

  ASSERT_TRUE(buffer_resource);
  wl_shm_buffer* buffer = wl_shm_buffer_get(buffer_resource);
  ASSERT_TRUE(buffer);
  EXPECT_EQ(wl_shm_buffer_get_width(buffer), 100);
  EXPECT_EQ(wl_shm_buffer_get_height(buffer), 50);
}

}  // namespace ui
