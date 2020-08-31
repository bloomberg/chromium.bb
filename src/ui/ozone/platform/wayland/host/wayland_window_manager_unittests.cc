// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"

namespace ui {

namespace {

constexpr gfx::Rect kDefaultBounds(0, 0, 100, 100);

}  // namespace

class WaylandWindowManagerTest : public WaylandTest {
 public:
  WaylandWindowManagerTest() {}
  WaylandWindowManagerTest(const WaylandWindowManagerTest&) = delete;
  WaylandWindowManagerTest& operator=(const WaylandWindowManagerTest&) = delete;

  void SetUp() override {
    WaylandTest::SetUp();

    manager_ = connection_->wayland_window_manager();
    DCHECK(manager_);
  }

 protected:
  std::unique_ptr<WaylandWindow> CreateWaylandWindowWithParams(
      PlatformWindowType type,
      const gfx::Rect bounds,
      MockPlatformWindowDelegate* delegate) {
    PlatformWindowInitProperties properties;
    properties.bounds = bounds;
    properties.type = type;
    auto window = WaylandWindow::Create(delegate, connection_.get(),
                                        std::move(properties));
    if (window)
      window->Show(false);
    return window;
  }

  WaylandWindowManager* manager_ = nullptr;
};

TEST_P(WaylandWindowManagerTest, GetWindow) {
  MockPlatformWindowDelegate delegate;

  auto window1 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate);
  gfx::AcceleratedWidget window1_widget = window1->GetWidget();

  auto window2 = CreateWaylandWindowWithParams(
      PlatformWindowType::kWindow, gfx::Rect(0, 0, 300, 300), &delegate);

  EXPECT_TRUE(window1.get() == manager_->GetWindow(window1->GetWidget()));
  EXPECT_TRUE(window2.get() == manager_->GetWindow(window2->GetWidget()));

  window1.reset();

  EXPECT_FALSE(manager_->GetWindow(window1_widget));
}

TEST_P(WaylandWindowManagerTest, GetWindowWithLargestBounds) {
  MockPlatformWindowDelegate delegate;

  auto window1 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate);
  // There is also default window create by the WaylandTest. Thus, make bounds
  // of this window large enough.
  auto window2 = CreateWaylandWindowWithParams(
      PlatformWindowType::kWindow, gfx::Rect(0, 0, 3000, 3000), &delegate);

  EXPECT_TRUE(window2.get() == manager_->GetWindowWithLargestBounds());
}

TEST_P(WaylandWindowManagerTest, GetCurrentFocusedWindow) {
  MockPlatformWindowDelegate delegate;

  wl_seat_send_capabilities(server_.seat()->resource(),
                            WL_SEAT_CAPABILITY_POINTER);

  Sync();

  auto window1 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate);
  // When window is shown, it automatically gets keyboard focus. Reset it.
  window_->set_keyboard_focus(false);
  window1->set_keyboard_focus(false);

  Sync();

  EXPECT_FALSE(manager_->GetCurrentFocusedWindow());

  auto* pointer = server_.seat()->pointer();
  ASSERT_TRUE(pointer);

  wl::MockSurface* surface =
      server_.GetObject<wl::MockSurface>(window1->GetWidget());
  wl_pointer_send_enter(pointer->resource(), 1, surface->resource(), 0, 0);

  Sync();

  EXPECT_FALSE(manager_->GetCurrentKeyboardFocusedWindow());
  EXPECT_TRUE(window1.get() == manager_->GetCurrentFocusedWindow());

  wl_pointer_send_leave(pointer->resource(), 2, surface->resource());

  Sync();

  EXPECT_FALSE(manager_->GetCurrentFocusedWindow());
}

TEST_P(WaylandWindowManagerTest, GetCurrentKeyboardFocusedWindow) {
  MockPlatformWindowDelegate delegate;

  wl_seat_send_capabilities(server_.seat()->resource(),
                            WL_SEAT_CAPABILITY_KEYBOARD);

  Sync();

  auto window1 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate);
  // When window is shown, it automatically gets keyboard focus. Reset it.
  window_->set_keyboard_focus(false);
  window1->set_keyboard_focus(false);

  Sync();

  EXPECT_FALSE(manager_->GetCurrentKeyboardFocusedWindow());

  auto* keyboard = server_.seat()->keyboard();
  ASSERT_TRUE(keyboard);

  wl::MockSurface* surface =
      server_.GetObject<wl::MockSurface>(window1->GetWidget());

  struct wl_array empty;
  wl_array_init(&empty);
  wl_keyboard_send_enter(keyboard->resource(), 1, surface->resource(), &empty);

  Sync();

  EXPECT_FALSE(manager_->GetCurrentFocusedWindow());
  EXPECT_TRUE(window1.get() == manager_->GetCurrentKeyboardFocusedWindow());

  wl_keyboard_send_leave(keyboard->resource(), 2, surface->resource());

  Sync();

  EXPECT_FALSE(manager_->GetCurrentKeyboardFocusedWindow());
}

TEST_P(WaylandWindowManagerTest, GetWindowsOnOutput) {
  MockPlatformWindowDelegate delegate;

  wl::TestOutput* output = server_.CreateAndInitializeOutput();

  auto window1 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate);

  auto window2 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate);

  Sync();

  wl::MockSurface* surface =
      server_.GetObject<wl::MockSurface>(window1->GetWidget());
  ASSERT_TRUE(surface);
  wl_surface_send_enter(surface->resource(), output->resource());

  Sync();

  auto entered_outputs_window1 = window1->entered_outputs_ids();
  EXPECT_EQ(1u, entered_outputs_window1.size());

  uint32_t output_id = *entered_outputs_window1.begin();

  auto windows_on_output = manager_->GetWindowsOnOutput(output_id);
  EXPECT_EQ(1u, windows_on_output.size());

  EXPECT_TRUE(window1.get() == *windows_on_output.begin());

  surface = server_.GetObject<wl::MockSurface>(window2->GetWidget());
  ASSERT_TRUE(surface);
  wl_surface_send_enter(surface->resource(), output->resource());

  Sync();

  windows_on_output = manager_->GetWindowsOnOutput(output_id);
  EXPECT_EQ(2u, windows_on_output.size());
}

TEST_P(WaylandWindowManagerTest, GetAllWindows) {
  MockPlatformWindowDelegate delegate;

  // There is a default window created by WaylandTest.
  auto windows = manager_->GetAllWindows();
  EXPECT_EQ(1u, windows.size());

  window_.reset();

  auto window1 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate);

  auto window2 = CreateWaylandWindowWithParams(PlatformWindowType::kWindow,
                                               kDefaultBounds, &delegate);

  windows = manager_->GetAllWindows();
  EXPECT_EQ(2u, windows.size());
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandWindowManagerTest,
                         ::testing::Values(kXdgShellStable));

INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandWindowManagerTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
