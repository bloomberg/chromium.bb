// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/run_loop.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/wayland_surface_factory.h"
#include "ui/ozone/platform/wayland/wayland_test.h"
#include "ui/ozone/platform/wayland/wayland_window.h"
#include "ui/ozone/public/surface_ozone_canvas.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"

using ::testing::Expectation;
using ::testing::SaveArg;
using ::testing::_;

namespace ui {

class WaylandSurfaceFactoryTest : public WaylandTest {
 public:
  WaylandSurfaceFactoryTest() : surface_factory(connection_proxy_.get()) {}

  ~WaylandSurfaceFactoryTest() override {}

  void SetUp() override {
    WaylandTest::SetUp();

    auto connection_ptr = connection_->BindInterface();
    connection_proxy_->SetWaylandConnection(std::move(connection_ptr));

    canvas = surface_factory.CreateCanvasForWidget(widget_);
    ASSERT_TRUE(canvas);

    // Wait until initialization and mojo calls go through.
    base::RunLoop().RunUntilIdle();
  }

  void TearDown() override {
    canvas.reset();

    // The mojo call to destroy shared buffer goes after canvas is destroyed.
    // Wait until it's done.
    base::RunLoop().RunUntilIdle();
  }

 protected:
  WaylandSurfaceFactory surface_factory;
  std::unique_ptr<SurfaceOzoneCanvas> canvas;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandSurfaceFactoryTest);
};

TEST_P(WaylandSurfaceFactoryTest, Canvas) {
  canvas->ResizeCanvas(window_->GetBounds().size());
  canvas->GetSurface();
  canvas->PresentCanvas(gfx::Rect(5, 10, 20, 15));

  // Wait until the mojo calls are done.
  base::RunLoop().RunUntilIdle();

  Expectation damage = EXPECT_CALL(*surface_, Damage(5, 10, 20, 15));
  wl_resource* buffer_resource = nullptr;
  Expectation attach = EXPECT_CALL(*surface_, Attach(_, 0, 0))
                           .WillOnce(SaveArg<0>(&buffer_resource));
  EXPECT_CALL(*surface_, Commit()).After(damage, attach);

  Sync();

  ASSERT_TRUE(buffer_resource);
  wl_shm_buffer* buffer = wl_shm_buffer_get(buffer_resource);
  ASSERT_TRUE(buffer);
  EXPECT_EQ(wl_shm_buffer_get_width(buffer), 800);
  EXPECT_EQ(wl_shm_buffer_get_height(buffer), 600);

  // TODO(forney): We could check that the contents match something drawn to the
  // SkSurface above.
}

TEST_P(WaylandSurfaceFactoryTest, CanvasResize) {
  canvas->ResizeCanvas(window_->GetBounds().size());
  canvas->GetSurface();
  canvas->ResizeCanvas(gfx::Size(100, 50));
  canvas->GetSurface();
  canvas->PresentCanvas(gfx::Rect(0, 0, 100, 50));

  base::RunLoop().RunUntilIdle();

  Expectation damage = EXPECT_CALL(*surface_, Damage(0, 0, 100, 50));
  wl_resource* buffer_resource = nullptr;
  Expectation attach = EXPECT_CALL(*surface_, Attach(_, 0, 0))
                           .WillOnce(SaveArg<0>(&buffer_resource));
  EXPECT_CALL(*surface_, Commit()).After(damage, attach);

  Sync();

  ASSERT_TRUE(buffer_resource);
  wl_shm_buffer* buffer = wl_shm_buffer_get(buffer_resource);
  ASSERT_TRUE(buffer);
  EXPECT_EQ(wl_shm_buffer_get_width(buffer), 100);
  EXPECT_EQ(wl_shm_buffer_get_height(buffer), 50);
}

INSTANTIATE_TEST_SUITE_P(XdgVersionV5Test,
                         WaylandSurfaceFactoryTest,
                         ::testing::Values(kXdgShellV5));
INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandSurfaceFactoryTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
