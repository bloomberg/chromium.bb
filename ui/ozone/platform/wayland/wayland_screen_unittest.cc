// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wayland-server.h>

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/display_observer.h"
#include "ui/ozone/platform/wayland/fake_server.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/wayland_screen.h"
#include "ui/ozone/platform/wayland/wayland_test.h"

namespace ui {

namespace {

constexpr uint32_t kNumberOfDisplays = 1;
constexpr uint32_t kOutputWidth = 1024;
constexpr uint32_t kOutputHeight = 768;

class TestDisplayObserver : public display::DisplayObserver {
 public:
  TestDisplayObserver() {}
  ~TestDisplayObserver() override {}

  display::Display GetDisplay() { return std::move(display_); }
  uint32_t GetAndClearChangedMetrics() {
    uint32_t changed_metrics = changed_metrics_;
    changed_metrics_ = 0;
    return changed_metrics;
  }

  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    DCHECK(changed_metrics_ == 0);
    changed_metrics_ = changed_metrics;
    display_ = display;
  }

 private:
  uint32_t changed_metrics_ = 0;
  display::Display display_;

  DISALLOW_COPY_AND_ASSIGN(TestDisplayObserver);
};

}  // namespace

class WaylandScreenTest : public WaylandTest {
 public:
  WaylandScreenTest() {}
  ~WaylandScreenTest() override {}

  void SetUp() override {
    output_ = server_.output();
    output_->SetRect(gfx::Rect(0, 0, kOutputWidth, kOutputHeight));

    WaylandTest::SetUp();

    output_manager_ = connection_->wayland_output_manager();
    ASSERT_TRUE(output_manager_);
  }

 protected:
  wl::MockOutput* output_ = nullptr;
  WaylandOutputManager* output_manager_ = nullptr;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandScreenTest);
};

// Tests whether a primary output has been initialized before PlatformScreen is
// created.
TEST_P(WaylandScreenTest, OutputBaseTest) {
  EXPECT_TRUE(output_manager_->IsPrimaryOutputReady());

  std::unique_ptr<WaylandScreen> platform_screen =
      output_manager_->CreateWaylandScreen();

  // Ensure there is only one display, which is the primary one.
  auto& all_displays = platform_screen->GetAllDisplays();
  EXPECT_EQ(all_displays.size(), kNumberOfDisplays);

  // Ensure the size property of the primary display.
  EXPECT_EQ(platform_screen->GetPrimaryDisplay().bounds(),
            gfx::Rect(0, 0, kOutputWidth, kOutputHeight));
}

TEST_P(WaylandScreenTest, OutputPropertyChanges) {
  std::unique_ptr<WaylandScreen> platform_screen =
      output_manager_->CreateWaylandScreen();
  TestDisplayObserver observer;
  platform_screen->AddObserver(&observer);

  const gfx::Rect new_rect(0, 0, 800, 600);
  wl_output_send_geometry(output_->resource(), new_rect.x(), new_rect.y(),
                          0 /* physical_width */, 0 /* physical_height */,
                          0 /* subpixel */, "unkown_make", "unknown_model",
                          0 /* transform */);
  wl_output_send_mode(output_->resource(), WL_OUTPUT_MODE_CURRENT,
                      new_rect.width(), new_rect.height(), 0 /* refresh */);

  Sync();

  uint32_t changed_values = 0;
  changed_values |= display::DisplayObserver::DISPLAY_METRIC_BOUNDS;
  changed_values |= display::DisplayObserver::DISPLAY_METRIC_WORK_AREA;
  EXPECT_EQ(observer.GetAndClearChangedMetrics(), changed_values);
  EXPECT_EQ(observer.GetDisplay().bounds(), new_rect);

  const float new_scale_value = 2.0f;
  wl_output_send_scale(output_->resource(), new_scale_value);

  Sync();

  changed_values = 0;
  changed_values |=
      display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;
  EXPECT_EQ(observer.GetAndClearChangedMetrics(), changed_values);
  EXPECT_EQ(observer.GetDisplay().device_scale_factor(), new_scale_value);
}

INSTANTIATE_TEST_CASE_P(XdgVersionV5Test,
                        WaylandScreenTest,
                        ::testing::Values(kXdgShellV5));
INSTANTIATE_TEST_CASE_P(XdgVersionV6Test,
                        WaylandScreenTest,
                        ::testing::Values(kXdgShellV6));

}  // namespace ui
