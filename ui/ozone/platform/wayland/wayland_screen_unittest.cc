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
#include "ui/platform_window/platform_window_init_properties.h"

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

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override {
    display_ = new_display;
  }

  void OnDisplayRemoved(const display::Display& old_display) override {
    display_ = old_display;
  }

  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
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

    EXPECT_TRUE(output_manager_->IsPrimaryOutputReady());
    platform_screen_ = output_manager_->CreateWaylandScreen(connection_.get());
  }

 protected:
  std::unique_ptr<WaylandWindow> CreateWaylandWindowWithProperties(
      const gfx::Rect& bounds,
      PlatformWindowType window_type,
      gfx::AcceleratedWidget parent_widget,
      MockPlatformWindowDelegate* delegate) {
    auto window = std::make_unique<WaylandWindow>(delegate, connection_.get());
    PlatformWindowInitProperties properties;
    properties.bounds = bounds;
    properties.type = window_type;
    properties.parent_widget = parent_widget;
    EXPECT_TRUE(window->Initialize(std::move(properties)));
    return window;
  }

  wl::MockOutput* output_ = nullptr;
  WaylandOutputManager* output_manager_ = nullptr;

  std::unique_ptr<WaylandScreen> platform_screen_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandScreenTest);
};

// Tests whether a primary output has been initialized before PlatformScreen is
// created.
TEST_P(WaylandScreenTest, OutputBaseTest) {
  // IsPrimaryOutputReady and PlatformScreen creation is done in the
  // initialization part of the tests.

  // Ensure there is only one display, which is the primary one.
  auto& all_displays = platform_screen_->GetAllDisplays();
  EXPECT_EQ(all_displays.size(), kNumberOfDisplays);

  // Ensure the size property of the primary display.
  EXPECT_EQ(platform_screen_->GetPrimaryDisplay().bounds(),
            gfx::Rect(0, 0, kOutputWidth, kOutputHeight));
}

TEST_P(WaylandScreenTest, MultipleOutputsAddedAndRemoved) {
  TestDisplayObserver observer;
  platform_screen_->AddObserver(&observer);

  // Add a second display.
  server_.CreateAndInitializeOutput();

  Sync();

  // Ensure that second display is not a primary one and have a different id.
  int64_t added_display_id = observer.GetDisplay().id();
  EXPECT_NE(platform_screen_->GetPrimaryDisplay().id(), added_display_id);

  // Remove the second output.
  output_manager_->RemoveWaylandOutput(added_display_id);

  Sync();

  // Ensure that removed display has correct id.
  int64_t removed_display_id = observer.GetDisplay().id();
  EXPECT_EQ(added_display_id, removed_display_id);

  // Create another display again.
  server_.CreateAndInitializeOutput();

  Sync();

  // The newly added display is not a primary yet.
  added_display_id = observer.GetDisplay().id();
  EXPECT_NE(platform_screen_->GetPrimaryDisplay().id(), added_display_id);

  // Make sure the geometry changes are sent by syncing one more time again.
  Sync();

  int64_t old_primary_display_id = platform_screen_->GetPrimaryDisplay().id();
  output_manager_->RemoveWaylandOutput(old_primary_display_id);

  // Ensure that previously added display is now a primary one.
  EXPECT_EQ(platform_screen_->GetPrimaryDisplay().id(), added_display_id);
  // Ensure that the removed display was the one, which was a primary display.
  EXPECT_EQ(observer.GetDisplay().id(), old_primary_display_id);

  platform_screen_->RemoveObserver(&observer);
}

TEST_P(WaylandScreenTest, OutputPropertyChanges) {
  TestDisplayObserver observer;
  platform_screen_->AddObserver(&observer);

  const gfx::Rect new_rect(0, 0, 800, 600);
  wl_output_send_geometry(output_->resource(), new_rect.x(), new_rect.y(),
                          0 /* physical_width */, 0 /* physical_height */,
                          0 /* subpixel */, "unkown_make", "unknown_model",
                          0 /* transform */);
  wl_output_send_mode(output_->resource(), WL_OUTPUT_MODE_CURRENT,
                      new_rect.width(), new_rect.height(), 0 /* refresh */);
  wl_output_send_done(output_->resource());

  Sync();

  uint32_t changed_values = 0;
  changed_values |= display::DisplayObserver::DISPLAY_METRIC_BOUNDS;
  changed_values |= display::DisplayObserver::DISPLAY_METRIC_WORK_AREA;
  EXPECT_EQ(observer.GetAndClearChangedMetrics(), changed_values);
  EXPECT_EQ(observer.GetDisplay().bounds(), new_rect);

  const float new_scale_value = 2.0f;
  wl_output_send_scale(output_->resource(), new_scale_value);
  wl_output_send_done(output_->resource());

  Sync();

  changed_values = 0;
  changed_values |=
      display::DisplayObserver::DISPLAY_METRIC_DEVICE_SCALE_FACTOR;
  EXPECT_EQ(observer.GetAndClearChangedMetrics(), changed_values);
  EXPECT_EQ(observer.GetDisplay().device_scale_factor(), new_scale_value);

  platform_screen_->RemoveObserver(&observer);
}

TEST_P(WaylandScreenTest, GetAcceleratedWidgetAtScreenPoint) {
  // If there is no focused window (focus is set whenever a pointer enters any
  // of the windows), there must be kNullAcceleratedWidget returned. There is no
  // real way to determine what window is located on a certain screen point in
  // Wayland.
  gfx::AcceleratedWidget widget_at_screen_point =
      platform_screen_->GetAcceleratedWidgetAtScreenPoint(gfx::Point(10, 10));
  EXPECT_EQ(widget_at_screen_point, gfx::kNullAcceleratedWidget);

  // Set a focus to the main window. Now, that focused window must be returned.
  window_->set_pointer_focus(true);
  widget_at_screen_point =
      platform_screen_->GetAcceleratedWidgetAtScreenPoint(gfx::Point(10, 10));
  EXPECT_EQ(widget_at_screen_point, window_->GetWidget());

  // Getting a widget at a screen point outside its bounds, must result in a
  // null widget.
  const gfx::Rect window_bounds = window_->GetBounds();
  widget_at_screen_point = platform_screen_->GetAcceleratedWidgetAtScreenPoint(
      gfx::Point(window_bounds.width() + 1, window_bounds.height() + 1));
  EXPECT_EQ(widget_at_screen_point, gfx::kNullAcceleratedWidget);

  MockPlatformWindowDelegate delegate;
  std::unique_ptr<WaylandWindow> menu_window =
      CreateWaylandWindowWithProperties(
          gfx::Rect(window_->GetBounds().width() - 10,
                    window_->GetBounds().height() - 10, 100, 100),
          PlatformWindowType::kPopup, window_->GetWidget(), &delegate);

  Sync();

  // Imagine the mouse enters a menu window, which is located on top of the main
  // window, and gathers focus.
  window_->set_pointer_focus(false);
  menu_window->set_pointer_focus(true);
  widget_at_screen_point =
      platform_screen_->GetAcceleratedWidgetAtScreenPoint(gfx::Point(
          menu_window->GetBounds().x() + 1, menu_window->GetBounds().y() + 1));
  EXPECT_EQ(widget_at_screen_point, menu_window->GetWidget());

  // Whenever a mouse pointer leaves the menu window, the accelerated widget
  // of that focused window must be returned.
  window_->set_pointer_focus(true);
  menu_window->set_pointer_focus(false);
  widget_at_screen_point =
      platform_screen_->GetAcceleratedWidgetAtScreenPoint(gfx::Point(0, 0));
  EXPECT_EQ(widget_at_screen_point, window_->GetWidget());

  // Reset the focus to avoid crash on dtor as long as there is no real pointer
  // object.
  window_->set_pointer_focus(false);
}

INSTANTIATE_TEST_CASE_P(XdgVersionV5Test,
                        WaylandScreenTest,
                        ::testing::Values(kXdgShellV5));
INSTANTIATE_TEST_CASE_P(XdgVersionV6Test,
                        WaylandScreenTest,
                        ::testing::Values(kXdgShellV6));

}  // namespace ui
