// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_window.h"

#include <memory>
#include <utility>

#include <linux/input.h>
#include <wayland-server-core.h>
#include <xdg-shell-server-protocol.h>
#include <xdg-shell-unstable-v6-server-protocol.h>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/hit_test.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/test_region.h"
#include "ui/ozone/platform/wayland/test/test_touch.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"
#include "ui/platform_window/platform_window_handler/wm_move_resize_handler.h"
#include "ui/platform_window/platform_window_init_properties.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrEq;

namespace ui {

namespace {

struct PopupPosition {
  gfx::Rect anchor_rect;
  gfx::Size size;
  uint32_t anchor = 0;
  uint32_t gravity = 0;
  uint32_t constraint_adjustment = 0;
};

class ScopedWlArray {
 public:
  ScopedWlArray() { wl_array_init(&array_); }

  ScopedWlArray(ScopedWlArray&& rhs) {
    array_ = rhs.array_;
    // wl_array_init sets rhs.array_'s fields to nullptr, so that
    // the free() in wl_array_release() is a no-op.
    wl_array_init(&rhs.array_);
  }

  ~ScopedWlArray() { wl_array_release(&array_); }

  ScopedWlArray& operator=(ScopedWlArray&& rhs) {
    wl_array_release(&array_);
    array_ = rhs.array_;
    // wl_array_init sets rhs.array_'s fields to nullptr, so that
    // the free() in wl_array_release() is a no-op.
    wl_array_init(&rhs.array_);
    return *this;
  }

  wl_array* get() { return &array_; }

 private:
  wl_array array_;
};

}  // namespace

class WaylandWindowTest : public WaylandTest {
 public:
  WaylandWindowTest()
      : test_mouse_event_(ET_MOUSE_PRESSED,
                          gfx::Point(10, 15),
                          gfx::Point(10, 15),
                          ui::EventTimeStampFromSeconds(123456),
                          EF_LEFT_MOUSE_BUTTON | EF_RIGHT_MOUSE_BUTTON,
                          EF_LEFT_MOUSE_BUTTON) {}

  void SetUp() override {
    WaylandTest::SetUp();

    xdg_surface_ = surface_->xdg_surface();
    ASSERT_TRUE(xdg_surface_);
  }

 protected:
  void SendConfigureEvent(int width,
                          int height,
                          uint32_t serial,
                          struct wl_array* states) {
    // In xdg_shell_v6+, both surfaces send serial configure event and toplevel
    // surfaces send other data like states, heights and widths.
    if (GetParam() == kXdgShellV6) {
      zxdg_surface_v6_send_configure(xdg_surface_->resource(), serial);
      ASSERT_TRUE(xdg_surface_->xdg_toplevel());
      zxdg_toplevel_v6_send_configure(xdg_surface_->xdg_toplevel()->resource(),
                                      width, height, states);
    } else {
      xdg_surface_send_configure(xdg_surface_->resource(), serial);
      ASSERT_TRUE(xdg_surface_->xdg_toplevel());
      xdg_toplevel_send_configure(xdg_surface_->xdg_toplevel()->resource(),
                                  width, height, states);
    }
  }

  void SendConfigureEventPopup(gfx::AcceleratedWidget menu_widget,
                               const gfx::Rect bounds) {
    auto* popup = GetPopupByWidget(menu_widget);
    ASSERT_TRUE(popup);
    if (GetParam() == kXdgShellV6) {
      zxdg_popup_v6_send_configure(popup->resource(), bounds.x(), bounds.y(),
                                   bounds.width(), bounds.height());
    } else {
      xdg_popup_send_configure(popup->resource(), bounds.x(), bounds.y(),
                               bounds.width(), bounds.height());
    }
  }

  wl::MockXdgTopLevel* GetXdgToplevel() { return xdg_surface_->xdg_toplevel(); }

  void AddStateToWlArray(uint32_t state, wl_array* states) {
    *static_cast<uint32_t*>(wl_array_add(states, sizeof state)) = state;
  }

  ScopedWlArray InitializeWlArrayWithActivatedState() {
    ScopedWlArray states;
    AddStateToWlArray(XDG_TOPLEVEL_STATE_ACTIVATED, states.get());
    return states;
  }

  ScopedWlArray MakeStateArray(const std::vector<int32_t> states) {
    ScopedWlArray result;
    for (const auto state : states)
      AddStateToWlArray(state, result.get());
    return result;
  }

  std::unique_ptr<WaylandWindow> CreateWaylandWindowWithParams(
      PlatformWindowType type,
      gfx::AcceleratedWidget parent_widget,
      const gfx::Rect bounds,
      MockPlatformWindowDelegate* delegate) {
    PlatformWindowInitProperties properties;
    // TODO(msisov): use a fancy method to calculate position of a popup window.
    properties.bounds = bounds;
    properties.type = type;
    properties.parent_widget = parent_widget;

    auto window = WaylandWindow::Create(delegate, connection_.get(),
                                        std::move(properties));
    if (window)
      window->Show(false);
    return window;
  }

  void InitializeWithSupportedHitTestValues(std::vector<int>* hit_tests) {
    hit_tests->push_back(static_cast<int>(HTBOTTOM));
    hit_tests->push_back(static_cast<int>(HTBOTTOMLEFT));
    hit_tests->push_back(static_cast<int>(HTBOTTOMRIGHT));
    hit_tests->push_back(static_cast<int>(HTLEFT));
    hit_tests->push_back(static_cast<int>(HTRIGHT));
    hit_tests->push_back(static_cast<int>(HTTOP));
    hit_tests->push_back(static_cast<int>(HTTOPLEFT));
    hit_tests->push_back(static_cast<int>(HTTOPRIGHT));
  }

  void VerifyAndClearExpectations() {
    Mock::VerifyAndClearExpectations(xdg_surface_);
    Mock::VerifyAndClearExpectations(&delegate_);
  }

  void VerifyXdgPopupPosition(gfx::AcceleratedWidget menu_widget,
                              const PopupPosition& position) {
    auto* popup = GetPopupByWidget(menu_widget);
    ASSERT_TRUE(popup);

    EXPECT_EQ(popup->anchor_rect(), position.anchor_rect);
    EXPECT_EQ(popup->size(), position.size);
    EXPECT_EQ(popup->anchor(), position.anchor);
    EXPECT_EQ(popup->gravity(), position.gravity);
    EXPECT_EQ(popup->constraint_adjustment(), position.constraint_adjustment);
  }

  void VerifyCanDispatchMouseEvents(
      const std::vector<WaylandWindow*>& dispatching_windows,
      const std::vector<WaylandWindow*>& non_dispatching_windows) {
    for (auto* window : dispatching_windows)
      EXPECT_TRUE(window->CanDispatchEvent(&test_mouse_event_));
    for (auto* window : non_dispatching_windows)
      EXPECT_FALSE(window->CanDispatchEvent(&test_mouse_event_));
  }

  void VerifyCanDispatchTouchEvents(
      const std::vector<WaylandWindow*>& dispatching_windows,
      const std::vector<WaylandWindow*>& non_dispatching_windows) {
    PointerDetails pointer_details(EventPointerType::kTouch, 1);
    TouchEvent test_touch_event(ET_TOUCH_PRESSED, {1, 1}, base::TimeTicks(),
                                pointer_details);
    for (auto* window : dispatching_windows)
      EXPECT_TRUE(window->CanDispatchEvent(&test_touch_event));
    for (auto* window : non_dispatching_windows)
      EXPECT_FALSE(window->CanDispatchEvent(&test_touch_event));
  }

  void VerifyCanDispatchKeyEvents(
      const std::vector<WaylandWindow*>& dispatching_windows,
      const std::vector<WaylandWindow*>& non_dispatching_windows) {
    KeyEvent test_key_event(ET_KEY_PRESSED, VKEY_0, 0);
    for (auto* window : dispatching_windows)
      EXPECT_TRUE(window->CanDispatchEvent(&test_key_event));
    for (auto* window : non_dispatching_windows)
      EXPECT_FALSE(window->CanDispatchEvent(&test_key_event));
  }

  wl::TestXdgPopup* GetPopupByWidget(gfx::AcceleratedWidget widget) {
    wl::MockSurface* mock_surface = server_.GetObject<wl::MockSurface>(widget);
    if (mock_surface) {
      auto* mock_xdg_surface = mock_surface->xdg_surface();
      if (mock_xdg_surface)
        return mock_xdg_surface->xdg_popup();
    }
    return nullptr;
  }

  wl::MockXdgSurface* xdg_surface_;

  MouseEvent test_mouse_event_;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandWindowTest);
};

TEST_P(WaylandWindowTest, SetTitle) {
  EXPECT_CALL(*GetXdgToplevel(), SetTitle(StrEq("hello")));
  window_->SetTitle(base::ASCIIToUTF16("hello"));
}

TEST_P(WaylandWindowTest, MaximizeAndRestore) {
  const auto kNormalBounds = gfx::Rect{0, 0, 500, 300};
  const auto kMaximizedBounds = gfx::Rect{0, 0, 800, 600};

  // Make sure the window has normal state initially.
  EXPECT_CALL(delegate_, OnBoundsChanged(kNormalBounds));
  window_->SetBounds(kNormalBounds);
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());
  VerifyAndClearExpectations();

  auto active_maximized = MakeStateArray(
      {XDG_TOPLEVEL_STATE_ACTIVATED, XDG_TOPLEVEL_STATE_MAXIMIZED});
  EXPECT_CALL(*GetXdgToplevel(), SetMaximized());
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kMaximizedBounds.width(),
                                               kMaximizedBounds.height()));
  EXPECT_CALL(delegate_, OnActivationChanged(Eq(true)));
  EXPECT_CALL(delegate_, OnBoundsChanged(kMaximizedBounds));
  EXPECT_CALL(delegate_, OnWindowStateChanged(_)).Times(0);
  window_->Maximize();
  SendConfigureEvent(kMaximizedBounds.width(), kMaximizedBounds.height(), 1,
                     active_maximized.get());
  Sync();
  VerifyAndClearExpectations();

  auto inactive_maximized = MakeStateArray({XDG_TOPLEVEL_STATE_MAXIMIZED});
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kMaximizedBounds.width(),
                                               kMaximizedBounds.height()));
  EXPECT_CALL(delegate_, OnActivationChanged(Eq(false)));
  EXPECT_CALL(delegate_, OnBoundsChanged(_)).Times(0);
  SendConfigureEvent(kMaximizedBounds.width(), kMaximizedBounds.height(), 2,
                     inactive_maximized.get());
  Sync();
  VerifyAndClearExpectations();

  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kMaximizedBounds.width(),
                                               kMaximizedBounds.height()));
  EXPECT_CALL(delegate_, OnActivationChanged(Eq(true)));
  EXPECT_CALL(delegate_, OnBoundsChanged(_)).Times(0);
  SendConfigureEvent(kMaximizedBounds.width(), kMaximizedBounds.height(), 3,
                     active_maximized.get());
  Sync();
  VerifyAndClearExpectations();

  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kNormalBounds.width(),
                                               kNormalBounds.height()));
  EXPECT_CALL(delegate_, OnWindowStateChanged(_)).Times(0);
  EXPECT_CALL(delegate_, OnActivationChanged(_)).Times(0);
  EXPECT_CALL(delegate_, OnBoundsChanged(kNormalBounds));
  EXPECT_CALL(*GetXdgToplevel(), UnsetMaximized());
  window_->Restore();
  // Reinitialize wl_array, which removes previous old states.
  auto active = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(0, 0, 4, active.get());
  Sync();
}

TEST_P(WaylandWindowTest, Minimize) {
  ScopedWlArray states;

  // Make sure the window is initialized to normal state from the beginning.
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());
  SendConfigureEvent(0, 0, 1, states.get());
  Sync();

  EXPECT_CALL(*GetXdgToplevel(), SetMinimized());
  EXPECT_CALL(delegate_, OnWindowStateChanged(_)).Times(0);
  window_->Minimize();
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMinimized);

  // Reinitialize wl_array, which removes previous old states.
  states = ScopedWlArray();
  SendConfigureEvent(0, 0, 2, states.get());
  Sync();

  // Wayland compositor doesn't notify clients about minimized state, but rather
  // if a window is not activated. Thus, a WaylandSurface marks itself as being
  // minimized and and sets state to minimized. Thus, the state mustn't change
  // after the configuration event is sent.
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMinimized);

  // Send one additional empty configuration event (which means the surface is
  // not maximized, fullscreen or activated) to ensure, WaylandWindow stays in
  // the same minimized state and doesn't notify its delegate.
  EXPECT_CALL(delegate_, OnWindowStateChanged(_)).Times(0);
  SendConfigureEvent(0, 0, 3, states.get());
  Sync();

  // And one last time to ensure the behaviour.
  SendConfigureEvent(0, 0, 4, states.get());
  Sync();
}

TEST_P(WaylandWindowTest, SetFullscreenAndRestore) {
  // Make sure the window is initialized to normal state from the beginning.
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());

  ScopedWlArray states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(0, 0, 1, states.get());
  Sync();

  AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN, states.get());

  EXPECT_CALL(*GetXdgToplevel(), SetFullscreen());
  EXPECT_CALL(delegate_, OnWindowStateChanged(_)).Times(0);
  window_->ToggleFullscreen();
  // Make sure than WaylandWindow manually handles fullscreen states. Check the
  // comment in the WaylandWindow::ToggleFullscreen.
  EXPECT_EQ(window_->GetPlatformWindowState(),
            PlatformWindowState::kFullScreen);
  SendConfigureEvent(0, 0, 2, states.get());
  Sync();

  EXPECT_CALL(*GetXdgToplevel(), UnsetFullscreen());
  EXPECT_CALL(delegate_, OnWindowStateChanged(_)).Times(0);
  window_->Restore();
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kNormal);
  // Reinitialize wl_array, which removes previous old states.
  states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(0, 0, 3, states.get());
  Sync();
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kNormal);
}

TEST_P(WaylandWindowTest, StartWithFullscreen) {
  MockPlatformWindowDelegate delegate;
  PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(0, 0, 100, 100);
  properties.type = PlatformWindowType::kWindow;
  // We need to create a window avoid calling Show() on it as it is what upper
  // views layer does - when Widget initialize DesktopWindowTreeHost, the Show()
  // is called later down the road, but Maximize may be called earlier. We
  // cannot process them and set a pending state instead, because ShellSurface
  // is not created by that moment.
  auto window = WaylandWindow::Create(&delegate, connection_.get(),
                                      std::move(properties));

  Sync();

  // Make sure the window is initialized to normal state from the beginning.
  EXPECT_EQ(PlatformWindowState::kNormal, window->GetPlatformWindowState());

  // The state must not be changed to the fullscreen before the surface is
  // activated.
  auto* mock_surface = server_.GetObject<wl::MockSurface>(window->GetWidget());
  EXPECT_FALSE(mock_surface->xdg_surface());
  EXPECT_CALL(delegate, OnWindowStateChanged(_)).Times(0);
  window->ToggleFullscreen();
  // The state of the window must already be fullscreen one.
  EXPECT_EQ(window->GetPlatformWindowState(), PlatformWindowState::kFullScreen);

  Sync();

  // We mustn't receive any state changes if that does not differ from the last
  // state.
  EXPECT_CALL(delegate, OnWindowStateChanged(_)).Times(0);

  // Activate the surface.
  ScopedWlArray states = InitializeWlArrayWithActivatedState();
  AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN, states.get());
  SendConfigureEvent(0, 0, 1, states.get());

  Sync();

  // It must be still the same state.
  EXPECT_EQ(window->GetPlatformWindowState(), PlatformWindowState::kFullScreen);
}

TEST_P(WaylandWindowTest, StartMaximized) {
  MockPlatformWindowDelegate delegate;
  PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(0, 0, 100, 100);
  properties.type = PlatformWindowType::kWindow;
  // We need to create a window avoid calling Show() on it as it is what upper
  // views layer does - when Widget initialize DesktopWindowTreeHost, the Show()
  // is called later down the road, but Maximize may be called earlier. We
  // cannot process them and set a pending state instead, because ShellSurface
  // is not created by that moment.
  auto window = WaylandWindow::Create(&delegate, connection_.get(),
                                      std::move(properties));

  Sync();

  // Make sure the window is initialized to normal state from the beginning.
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());

  // The state must not be changed to the fullscreen before the surface is
  // activated.
  auto* mock_surface = server_.GetObject<wl::MockSurface>(window->GetWidget());
  EXPECT_FALSE(mock_surface->xdg_surface());
  EXPECT_CALL(delegate_, OnWindowStateChanged(_)).Times(0);

  window_->Maximize();
  // The state of the window must already be fullscreen one.
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMaximized);

  Sync();

  // Once the surface will be activated, the window state mustn't be changed
  // and retain the same.
  EXPECT_CALL(delegate_, OnWindowStateChanged(_)).Times(0);
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMaximized);

  // Activate the surface.
  ScopedWlArray states = InitializeWlArrayWithActivatedState();
  AddStateToWlArray(XDG_TOPLEVEL_STATE_MAXIMIZED, states.get());
  SendConfigureEvent(0, 0, 1, states.get());

  Sync();

  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMaximized);
}

TEST_P(WaylandWindowTest, CompositorSideStateChanges) {
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kNormal);
  auto normal_bounds = window_->GetBounds();

  ScopedWlArray states = InitializeWlArrayWithActivatedState();
  AddStateToWlArray(XDG_TOPLEVEL_STATE_MAXIMIZED, states.get());
  SendConfigureEvent(2000, 2000, 1, states.get());

  EXPECT_CALL(delegate_,
              OnWindowStateChanged(Eq(PlatformWindowState::kMaximized)))
      .Times(1);
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, 2000, 2000));

  Sync();

  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMaximized);

  // Unmaximize
  states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(0, 0, 2, states.get());

  EXPECT_CALL(delegate_, OnWindowStateChanged(Eq(PlatformWindowState::kNormal)))
      .Times(1);
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, normal_bounds.width(),
                                               normal_bounds.height()));

  // Now, set to fullscreen.
  AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN, states.get());
  SendConfigureEvent(2005, 2005, 3, states.get());
  EXPECT_CALL(delegate_,
              OnWindowStateChanged(Eq(PlatformWindowState::kFullScreen)))
      .Times(1);
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, 2005, 2005));

  Sync();

  // Unfullscreen
  states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(0, 0, 4, states.get());

  EXPECT_CALL(delegate_, OnWindowStateChanged(Eq(PlatformWindowState::kNormal)))
      .Times(1);
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, normal_bounds.width(),
                                               normal_bounds.height()));

  Sync();

  // Now, maximize, fullscreen and restore.
  states = InitializeWlArrayWithActivatedState();
  AddStateToWlArray(XDG_TOPLEVEL_STATE_MAXIMIZED, states.get());
  SendConfigureEvent(2000, 2000, 1, states.get());

  EXPECT_CALL(delegate_,
              OnWindowStateChanged(Eq(PlatformWindowState::kMaximized)))
      .Times(1);
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, 2000, 2000));

  Sync();

  AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN, states.get());
  SendConfigureEvent(2005, 2005, 1, states.get());

  EXPECT_CALL(delegate_,
              OnWindowStateChanged(Eq(PlatformWindowState::kFullScreen)))
      .Times(1);
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, 2005, 2005));

  // Restore
  states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(0, 0, 4, states.get());

  EXPECT_CALL(delegate_, OnWindowStateChanged(Eq(PlatformWindowState::kNormal)))
      .Times(1);
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, normal_bounds.width(),
                                               normal_bounds.height()));

  Sync();
}

TEST_P(WaylandWindowTest, SetMaximizedFullscreenAndRestore) {
  const auto kNormalBounds = gfx::Rect{0, 0, 500, 300};
  const auto kMaximizedBounds = gfx::Rect{0, 0, 800, 600};

  // Make sure the window has normal state initially.
  EXPECT_CALL(delegate_, OnBoundsChanged(kNormalBounds));
  window_->SetBounds(kNormalBounds);
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());
  VerifyAndClearExpectations();

  auto active_maximized = MakeStateArray(
      {XDG_TOPLEVEL_STATE_ACTIVATED, XDG_TOPLEVEL_STATE_MAXIMIZED});
  EXPECT_CALL(*GetXdgToplevel(), SetMaximized());
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kMaximizedBounds.width(),
                                               kMaximizedBounds.height()));
  EXPECT_CALL(delegate_, OnActivationChanged(Eq(true)));
  EXPECT_CALL(delegate_, OnBoundsChanged(kMaximizedBounds));
  EXPECT_CALL(delegate_, OnWindowStateChanged(_)).Times(0);
  window_->Maximize();
  // State changes are synchronous.
  EXPECT_EQ(PlatformWindowState::kMaximized, window_->GetPlatformWindowState());
  SendConfigureEvent(kMaximizedBounds.width(), kMaximizedBounds.height(), 2,
                     active_maximized.get());
  Sync();
  // Verify that the state has not been changed.
  EXPECT_EQ(PlatformWindowState::kMaximized, window_->GetPlatformWindowState());
  VerifyAndClearExpectations();

  EXPECT_CALL(*GetXdgToplevel(), SetFullscreen());
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kMaximizedBounds.width(),
                                               kMaximizedBounds.height()));
  EXPECT_CALL(delegate_, OnBoundsChanged(_)).Times(0);
  EXPECT_CALL(delegate_, OnWindowStateChanged(_)).Times(0);
  window_->ToggleFullscreen();
  // State changes are synchronous.
  EXPECT_EQ(PlatformWindowState::kFullScreen,
            window_->GetPlatformWindowState());
  AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN, active_maximized.get());
  SendConfigureEvent(kMaximizedBounds.width(), kMaximizedBounds.height(), 3,
                     active_maximized.get());
  Sync();
  // Verify that the state has not been changed.
  EXPECT_EQ(PlatformWindowState::kFullScreen,
            window_->GetPlatformWindowState());
  VerifyAndClearExpectations();

  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kNormalBounds.width(),
                                               kNormalBounds.height()));
  EXPECT_CALL(*GetXdgToplevel(), UnsetFullscreen());
  EXPECT_CALL(*GetXdgToplevel(), UnsetMaximized());
  EXPECT_CALL(delegate_, OnBoundsChanged(kNormalBounds));
  EXPECT_CALL(delegate_, OnWindowStateChanged(_)).Times(0);
  window_->Restore();
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());
  // Reinitialize wl_array, which removes previous old states.
  auto active = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(0, 0, 4, active.get());
  Sync();
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());
}

TEST_P(WaylandWindowTest, RestoreBoundsAfterMaximize) {
  const gfx::Rect current_bounds = window_->GetBounds();

  ScopedWlArray states = InitializeWlArrayWithActivatedState();

  gfx::Rect restored_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_TRUE(restored_bounds.IsEmpty());
  gfx::Rect bounds = window_->GetBounds();

  const gfx::Rect maximized_bounds = gfx::Rect(0, 0, 1024, 768);
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(maximized_bounds)));
  window_->Maximize();
  AddStateToWlArray(XDG_TOPLEVEL_STATE_MAXIMIZED, states.get());
  SendConfigureEvent(maximized_bounds.width(), maximized_bounds.height(), 1,
                     states.get());
  Sync();
  restored_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_EQ(bounds, restored_bounds);

  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(current_bounds)));
  // Both in XdgV5 and XdgV6, surfaces implement SetWindowGeometry method.
  // Thus, using a toplevel object in XdgV6 case is not right thing. Use a
  // surface here instead.
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, current_bounds.width(),
                                               current_bounds.height()));
  window_->Restore();
  // Reinitialize wl_array, which removes previous old states.
  states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(0, 0, 2, states.get());
  Sync();
  bounds = window_->GetBounds();
  EXPECT_EQ(bounds, restored_bounds);
  restored_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_EQ(restored_bounds, gfx::Rect());
}

TEST_P(WaylandWindowTest, RestoreBoundsAfterFullscreen) {
  const gfx::Rect current_bounds = window_->GetBounds();

  ScopedWlArray states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(0, 0, 1, states.get());
  Sync();

  gfx::Rect restored_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_EQ(restored_bounds, gfx::Rect());
  gfx::Rect bounds = window_->GetBounds();

  const gfx::Rect fullscreen_bounds = gfx::Rect(0, 0, 1280, 720);
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(fullscreen_bounds)));
  window_->ToggleFullscreen();
  AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN, states.get());
  SendConfigureEvent(fullscreen_bounds.width(), fullscreen_bounds.height(), 2,
                     states.get());
  Sync();
  restored_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_EQ(bounds, restored_bounds);

  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(current_bounds)));
  // Both in XdgV5 and XdgV6, surfaces implement SetWindowGeometry method.
  // Thus, using a toplevel object in XdgV6 case is not right thing. Use a
  // surface here instead.
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, current_bounds.width(),
                                               current_bounds.height()));
  window_->Restore();
  // Reinitialize wl_array, which removes previous old states.
  states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(0, 0, 3, states.get());
  Sync();
  bounds = window_->GetBounds();
  EXPECT_EQ(bounds, restored_bounds);
  restored_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_EQ(restored_bounds, gfx::Rect());
}

TEST_P(WaylandWindowTest, RestoreBoundsAfterMaximizeAndFullscreen) {
  const gfx::Rect current_bounds = window_->GetBounds();

  ScopedWlArray states = InitializeWlArrayWithActivatedState();

  gfx::Rect restored_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_EQ(restored_bounds, gfx::Rect());
  gfx::Rect bounds = window_->GetBounds();

  const gfx::Rect maximized_bounds = gfx::Rect(0, 0, 1024, 768);
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(maximized_bounds)));
  window_->Maximize();
  AddStateToWlArray(XDG_TOPLEVEL_STATE_MAXIMIZED, states.get());
  SendConfigureEvent(maximized_bounds.width(), maximized_bounds.height(), 1,
                     states.get());
  Sync();
  restored_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_EQ(bounds, restored_bounds);

  const gfx::Rect fullscreen_bounds = gfx::Rect(0, 0, 1280, 720);
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(fullscreen_bounds)));
  window_->ToggleFullscreen();
  AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN, states.get());
  SendConfigureEvent(fullscreen_bounds.width(), fullscreen_bounds.height(), 2,
                     states.get());
  Sync();
  gfx::Rect fullscreen_restore_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_EQ(restored_bounds, fullscreen_restore_bounds);

  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(maximized_bounds)));
  window_->Maximize();
  // Reinitialize wl_array, which removes previous old states.
  states = InitializeWlArrayWithActivatedState();
  AddStateToWlArray(XDG_TOPLEVEL_STATE_MAXIMIZED, states.get());
  SendConfigureEvent(maximized_bounds.width(), maximized_bounds.height(), 3,
                     states.get());
  Sync();
  restored_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_EQ(restored_bounds, fullscreen_restore_bounds);

  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(current_bounds)));
  // Both in XdgV5 and XdgV6, surfaces implement SetWindowGeometry method.
  // Thus, using a toplevel object in XdgV6 case is not right thing. Use a
  // surface here instead.
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, current_bounds.width(),
                                               current_bounds.height()));
  window_->Restore();
  // Reinitialize wl_array, which removes previous old states.
  states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(0, 0, 4, states.get());
  Sync();
  bounds = window_->GetBounds();
  EXPECT_EQ(bounds, restored_bounds);
  restored_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_EQ(restored_bounds, gfx::Rect());
}

TEST_P(WaylandWindowTest, SendsBoundsOnRequest) {
  const gfx::Rect initial_bounds = window_->GetBounds();

  const gfx::Rect new_bounds = gfx::Rect(0, 0, initial_bounds.width() + 10,
                                         initial_bounds.height() + 10);
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(new_bounds)));
  window_->SetBounds(new_bounds);

  ScopedWlArray states = InitializeWlArrayWithActivatedState();

  // First case is when Wayland sends a configure event with 0,0 height and
  // width.
  EXPECT_CALL(*xdg_surface_,
              SetWindowGeometry(0, 0, new_bounds.width(), new_bounds.height()))
      .Times(2);
  SendConfigureEvent(0, 0, 2, states.get());
  Sync();

  // Restored bounds should keep empty value.
  gfx::Rect restored_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_EQ(restored_bounds, gfx::Rect());

  // Second case is when Wayland sends a configure event with 1, 1 height and
  // width. It looks more like a bug in Gnome Shell with Wayland as long as the
  // documentation says it must be set to 0, 0, when wayland requests bounds.
  SendConfigureEvent(0, 0, 3, states.get());
  Sync();

  // Restored bounds should keep empty value.
  restored_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_EQ(restored_bounds, gfx::Rect());
}

TEST_P(WaylandWindowTest, CanDispatchMouseEventDefault) {
  EXPECT_FALSE(window_->CanDispatchEvent(&test_mouse_event_));
}

TEST_P(WaylandWindowTest, CanDispatchMouseEventFocus) {
  // SetPointerFocus(true) requires a WaylandPointer.
  wl_seat_send_capabilities(server_.seat()->resource(),
                            WL_SEAT_CAPABILITY_POINTER);
  Sync();
  ASSERT_TRUE(connection_->pointer());
  window_->SetPointerFocus(true);
  EXPECT_TRUE(window_->CanDispatchEvent(&test_mouse_event_));
}

TEST_P(WaylandWindowTest, CanDispatchMouseEventUnfocus) {
  EXPECT_FALSE(window_->has_pointer_focus());
  EXPECT_FALSE(window_->CanDispatchEvent(&test_mouse_event_));
}

ACTION_P(CloneEvent, ptr) {
  *ptr = Event::Clone(*arg0);
}

TEST_P(WaylandWindowTest, DispatchEvent) {
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));
  window_->DispatchEvent(&test_mouse_event_);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  auto* mouse_event = event->AsMouseEvent();
  EXPECT_EQ(mouse_event->location_f(), test_mouse_event_.location_f());
  EXPECT_EQ(mouse_event->root_location_f(),
            test_mouse_event_.root_location_f());
  EXPECT_EQ(mouse_event->time_stamp(), test_mouse_event_.time_stamp());
  EXPECT_EQ(mouse_event->button_flags(), test_mouse_event_.button_flags());
  EXPECT_EQ(mouse_event->changed_button_flags(),
            test_mouse_event_.changed_button_flags());
}

TEST_P(WaylandWindowTest, ConfigureEvent) {
  ScopedWlArray states;

  // The surface must react on each configure event and send bounds to its
  // delegate.

  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(gfx::Rect(0, 0, 1000, 1000))));
  // Responding to a configure event, the window geometry in here must respect
  // the sizing negotiations specified by the configure event.
  // |xdg_surface_| must receive the following calls in both xdg_shell_v5 and
  // xdg_shell_v6. Other calls like SetTitle or SetMaximized are recieved by
  // xdg_toplevel in xdg_shell_v6 and by xdg_surface_ in xdg_shell_v5.
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, 1000, 1000)).Times(1);
  EXPECT_CALL(*xdg_surface_, AckConfigure(12));
  SendConfigureEvent(1000, 1000, 12, states.get());

  Sync();

  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(gfx::Rect(0, 0, 1500, 1000))));
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, 1500, 1000)).Times(1);
  EXPECT_CALL(*xdg_surface_, AckConfigure(13));
  SendConfigureEvent(1500, 1000, 13, states.get());
}

TEST_P(WaylandWindowTest, ConfigureEventWithNulledSize) {
  ScopedWlArray states;

  // If Wayland sends configure event with 0 width and 0 size, client should
  // call back with desired sizes. In this case, that's the actual size of
  // the window.
  SendConfigureEvent(0, 0, 14, states.get());
  // |xdg_surface_| must receive the following calls in both xdg_shell_v5 and
  // xdg_shell_v6. Other calls like SetTitle or SetMaximized are recieved by
  // xdg_toplevel in xdg_shell_v6 and by xdg_surface_ in xdg_shell_v5.
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, 800, 600));
  EXPECT_CALL(*xdg_surface_, AckConfigure(14));
}

TEST_P(WaylandWindowTest, OnActivationChanged) {
  {
    ScopedWlArray states = InitializeWlArrayWithActivatedState();
    EXPECT_CALL(delegate_, OnActivationChanged(Eq(true)));
    SendConfigureEvent(0, 0, 1, states.get());
    Sync();
  }

  ScopedWlArray states;
  EXPECT_CALL(delegate_, OnActivationChanged(Eq(false)));
  SendConfigureEvent(0, 0, 2, states.get());
  Sync();
}

TEST_P(WaylandWindowTest, OnAcceleratedWidgetDestroy) {
  window_.reset();
}

TEST_P(WaylandWindowTest, CanCreateMenuWindow) {
  MockPlatformWindowDelegate menu_window_delegate;

  // SetPointerFocus(true) requires a WaylandPointer.
  wl_seat_send_capabilities(
      server_.seat()->resource(),
      WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_TOUCH);
  Sync();
  ASSERT_TRUE(connection_->pointer() && connection_->touch());
  window_->SetPointerFocus(true);

  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, gfx::kNullAcceleratedWidget,
      gfx::Rect(0, 0, 10, 10), &menu_window_delegate);
  EXPECT_TRUE(menu_window);

  Sync();

  window_->SetPointerFocus(false);
  window_->set_touch_focus(false);

  // Given that there is no parent passed and we don't have any focused windows,
  // Wayland must still create a window.
  menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, gfx::kNullAcceleratedWidget,
      gfx::Rect(0, 0, 10, 10), &menu_window_delegate);
  EXPECT_TRUE(menu_window);

  Sync();

  window_->set_touch_focus(true);

  menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, gfx::kNullAcceleratedWidget,
      gfx::Rect(0, 0, 10, 10), &menu_window_delegate);
  EXPECT_TRUE(menu_window);

  Sync();
}

TEST_P(WaylandWindowTest, CreateAndDestroyNestedMenuWindow) {
  MockPlatformWindowDelegate menu_window_delegate;
  gfx::AcceleratedWidget menu_window_widget;
  EXPECT_CALL(menu_window_delegate, OnAcceleratedWidgetAvailable(_))
      .WillOnce(SaveArg<0>(&menu_window_widget));

  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, widget_, gfx::Rect(0, 0, 10, 10),
      &menu_window_delegate);
  EXPECT_TRUE(menu_window);
  ASSERT_NE(menu_window_widget, gfx::kNullAcceleratedWidget);

  Sync();

  MockPlatformWindowDelegate nested_menu_window_delegate;
  std::unique_ptr<WaylandWindow> nested_menu_window =
      CreateWaylandWindowWithParams(
          PlatformWindowType::kMenu, menu_window_widget,
          gfx::Rect(20, 0, 10, 10), &nested_menu_window_delegate);
  EXPECT_TRUE(nested_menu_window);

  Sync();
}

TEST_P(WaylandWindowTest, DispatchesLocatedEventsToCapturedWindow) {
  MockPlatformWindowDelegate menu_window_delegate;
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, widget_, gfx::Rect(10, 10, 10, 10),
      &menu_window_delegate);
  EXPECT_TRUE(menu_window);

  wl_seat_send_capabilities(server_.seat()->resource(),
                            WL_SEAT_CAPABILITY_POINTER);
  Sync();
  ASSERT_TRUE(connection_->pointer());
  window_->SetPointerFocus(true);

  // Make sure the events are handled by the window that has the pointer focus.
  VerifyCanDispatchMouseEvents({window_.get()}, {menu_window.get()});

  // The |window_| that has the pointer focus must receive the event.
  EXPECT_CALL(menu_window_delegate, DispatchEvent(_)).Times(0);
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  // The event is send in local surface coordinates of the |window|.
  wl_pointer_send_motion(server_.seat()->pointer()->resource(), 1002,
                         wl_fixed_from_double(10.75),
                         wl_fixed_from_double(20.375));

  Sync();

  ASSERT_TRUE(event->IsLocatedEvent());
  EXPECT_EQ(event->AsLocatedEvent()->location(), gfx::Point(10, 20));

  // Set capture to menu window now.
  menu_window->SetCapture();

  // It's still the |window_| that can dispatch the events, but it will reroute
  // the event to correct window and fix the location.
  VerifyCanDispatchMouseEvents({window_.get()}, {menu_window.get()});

  // The |window_| that has the pointer focus must receive the event.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(0);
  std::unique_ptr<Event> event2;
  EXPECT_CALL(menu_window_delegate, DispatchEvent(_))
      .WillOnce(CloneEvent(&event2));

  // The event is send in local surface coordinates of the |window|.
  wl_pointer_send_motion(server_.seat()->pointer()->resource(), 1002,
                         wl_fixed_from_double(10.75),
                         wl_fixed_from_double(20.375));

  Sync();

  ASSERT_TRUE(event2->IsLocatedEvent());
  EXPECT_EQ(event2->AsLocatedEvent()->location(), gfx::Point(0, 10));

  // The event is send in local surface coordinates of the |window|.
  wl_pointer_send_motion(server_.seat()->pointer()->resource(), 1002,
                         wl_fixed_from_double(2.75),
                         wl_fixed_from_double(8.375));

  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(0);
  std::unique_ptr<Event> event3;
  EXPECT_CALL(menu_window_delegate, DispatchEvent(_))
      .WillOnce(CloneEvent(&event3));

  Sync();

  ASSERT_TRUE(event3->IsLocatedEvent());
  EXPECT_EQ(event3->AsLocatedEvent()->location(), gfx::Point(-8, -2));

  // If nested menu window is added, the events are still correctly translated
  // to the captured window.
  MockPlatformWindowDelegate nested_menu_window_delegate;
  std::unique_ptr<WaylandWindow> nested_menu_window =
      CreateWaylandWindowWithParams(
          PlatformWindowType::kMenu, menu_window->GetWidget(),
          gfx::Rect(15, 18, 10, 10), &nested_menu_window_delegate);
  EXPECT_TRUE(nested_menu_window);

  Sync();

  window_->SetPointerFocus(false);
  nested_menu_window->SetPointerFocus(true);

  // The event is processed by the window that has the pointer focus, but
  // dispatched by the window that has the capture.
  VerifyCanDispatchMouseEvents({nested_menu_window.get()},
                               {window_.get(), menu_window.get()});
  EXPECT_TRUE(menu_window->HasCapture());

  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(0);
  EXPECT_CALL(nested_menu_window_delegate, DispatchEvent(_)).Times(0);
  std::unique_ptr<Event> event4;
  EXPECT_CALL(menu_window_delegate, DispatchEvent(_))
      .WillOnce(CloneEvent(&event4));

  // The event is send in local surface coordinates of the |nested_menu_window|.
  wl_pointer_send_motion(server_.seat()->pointer()->resource(), 1002,
                         wl_fixed_from_double(2.75),
                         wl_fixed_from_double(8.375));

  Sync();

  ASSERT_TRUE(event4->IsLocatedEvent());
  EXPECT_EQ(event4->AsLocatedEvent()->location(), gfx::Point(7, 16));

  menu_window.reset();
}

// Tests that the event grabber gets the events processed by its toplevel parent
// window iff they belong to the same "family". Otherwise, events mustn't be
// rerouted from another toplevel window to the event grabber.
TEST_P(WaylandWindowTest,
       DispatchesLocatedEventsToCapturedWindowInTheSameStack) {
  MockPlatformWindowDelegate menu_window_delegate;
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, widget_, gfx::Rect(30, 40, 20, 50),
      &menu_window_delegate);
  EXPECT_TRUE(menu_window);

  // Second toplevel window has the same bounds as the |window_|.
  MockPlatformWindowDelegate toplevel_window2_delegate;
  std::unique_ptr<WaylandWindow> toplevel_window2 =
      CreateWaylandWindowWithParams(
          PlatformWindowType::kWindow, gfx::kNullAcceleratedWidget,
          window_->GetBounds(), &toplevel_window2_delegate);
  EXPECT_TRUE(toplevel_window2);

  wl_seat_send_capabilities(server_.seat()->resource(),
                            WL_SEAT_CAPABILITY_POINTER);
  Sync();
  ASSERT_TRUE(connection_->pointer());
  window_->SetPointerFocus(true);

  // Make sure the events are handled by the window that has the pointer focus.
  VerifyCanDispatchMouseEvents({window_.get()},
                               {menu_window.get(), toplevel_window2.get()});

  menu_window->SetCapture();

  // The |menu_window| that has capture must receive the event.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(0);
  EXPECT_CALL(toplevel_window2_delegate, DispatchEvent(_)).Times(0);
  std::unique_ptr<Event> event;
  EXPECT_CALL(menu_window_delegate, DispatchEvent(_))
      .WillOnce(CloneEvent(&event));

  // The event is send in local surface coordinates of the |window|.
  wl_pointer_send_motion(server_.seat()->pointer()->resource(), 1002,
                         wl_fixed_from_double(10.75),
                         wl_fixed_from_double(20.375));

  Sync();

  ASSERT_TRUE(event->IsLocatedEvent());
  EXPECT_EQ(event->AsLocatedEvent()->location(), gfx::Point(-20, -20));

  // Now, pretend that the second toplevel window gets the pointer focus - the
  // event grabber must be disragerder now.
  window_->SetPointerFocus(false);
  toplevel_window2->SetPointerFocus(true);

  VerifyCanDispatchMouseEvents({toplevel_window2.get()},
                               {menu_window.get(), window_.get()});

  // The |toplevel_window2| that has capture and must receive the event.
  EXPECT_CALL(delegate_, DispatchEvent(_)).Times(0);
  EXPECT_CALL(menu_window_delegate, DispatchEvent(_)).Times(0);
  event.reset();
  EXPECT_CALL(toplevel_window2_delegate, DispatchEvent(_))
      .WillOnce(CloneEvent(&event));

  // The event is send in local surface coordinates of the |toplevel_window2|
  // (they're basically the same as the |window| has.)
  wl_pointer_send_motion(server_.seat()->pointer()->resource(), 1002,
                         wl_fixed_from_double(10.75),
                         wl_fixed_from_double(20.375));

  Sync();

  ASSERT_TRUE(event->IsLocatedEvent());
  EXPECT_EQ(event->AsLocatedEvent()->location(), gfx::Point(10, 20));
}

TEST_P(WaylandWindowTest, DispatchesKeyboardEventToToplevelWindow) {
  MockPlatformWindowDelegate menu_window_delegate;
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, widget_, gfx::Rect(10, 10, 10, 10),
      &menu_window_delegate);
  EXPECT_TRUE(menu_window);

  wl_seat_send_capabilities(server_.seat()->resource(),
                            WL_SEAT_CAPABILITY_KEYBOARD);
  Sync();
  ASSERT_TRUE(connection_->keyboard());
  menu_window->set_keyboard_focus(true);

  // Even though the menu window has the keyboard focus, the keyboard events are
  // dispatched by the root parent wayland window in the end.
  VerifyCanDispatchKeyEvents({menu_window.get()}, {window_.get()});
  EXPECT_CALL(menu_window_delegate, DispatchEvent(_)).Times(0);
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  wl_keyboard_send_key(server_.seat()->keyboard()->resource(), 2, 0, 30 /* a */,
                       WL_KEYBOARD_KEY_STATE_PRESSED);

  Sync();

  ASSERT_TRUE(event->IsKeyEvent());

  // Setting capture doesn't affect the kbd events.
  menu_window->SetCapture();
  VerifyCanDispatchKeyEvents({menu_window.get()}, {window_.get()});

  wl_keyboard_send_key(server_.seat()->keyboard()->resource(), 2, 0, 30 /* a */,
                       WL_KEYBOARD_KEY_STATE_PRESSED);

  EXPECT_CALL(menu_window_delegate, DispatchEvent(_)).Times(0);
  event.reset();
  EXPECT_CALL(delegate_, DispatchEvent(_)).WillOnce(CloneEvent(&event));

  Sync();

  ASSERT_TRUE(event->IsKeyEvent());

  menu_window.reset();
}

// Tests that event is processed by the surface that has the focus. More
// extensive tests are located in wayland touch/keyboard/pointer unittests.
TEST_P(WaylandWindowTest, CanDispatchEvent) {
  MockPlatformWindowDelegate menu_window_delegate;
  gfx::AcceleratedWidget menu_window_widget;
  EXPECT_CALL(menu_window_delegate, OnAcceleratedWidgetAvailable(_))
      .WillOnce(SaveArg<0>(&menu_window_widget));

  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, widget_, gfx::Rect(0, 0, 10, 10),
      &menu_window_delegate);
  EXPECT_TRUE(menu_window);

  Sync();

  MockPlatformWindowDelegate nested_menu_window_delegate;
  std::unique_ptr<WaylandWindow> nested_menu_window =
      CreateWaylandWindowWithParams(
          PlatformWindowType::kMenu, menu_window_widget,
          gfx::Rect(20, 0, 10, 10), &nested_menu_window_delegate);
  EXPECT_TRUE(nested_menu_window);

  Sync();

  wl_seat_send_capabilities(server_.seat()->resource(),
                            WL_SEAT_CAPABILITY_POINTER |
                                WL_SEAT_CAPABILITY_KEYBOARD |
                                WL_SEAT_CAPABILITY_TOUCH);
  Sync();
  ASSERT_TRUE(connection_->pointer());
  ASSERT_TRUE(connection_->touch());
  ASSERT_TRUE(connection_->keyboard());

  uint32_t serial = 0;

  // Test that CanDispatchEvent is set correctly.
  wl::MockSurface* toplevel_surface =
      server_.GetObject<wl::MockSurface>(widget_);
  wl_pointer_send_enter(server_.seat()->pointer()->resource(), ++serial,
                        toplevel_surface->resource(), 0, 0);

  Sync();

  // Only |window_| can dispatch MouseEvents.
  VerifyCanDispatchMouseEvents({window_.get()},
                               {menu_window.get(), nested_menu_window.get()});
  VerifyCanDispatchTouchEvents(
      {}, {window_.get(), menu_window.get(), nested_menu_window.get()});
  VerifyCanDispatchKeyEvents(
      {}, {window_.get(), menu_window.get(), nested_menu_window.get()});

  struct wl_array empty;
  wl_array_init(&empty);
  wl_keyboard_send_enter(server_.seat()->keyboard()->resource(), ++serial,
                         toplevel_surface->resource(), &empty);

  Sync();

  // Only |window_| can dispatch MouseEvents and KeyEvents.
  VerifyCanDispatchMouseEvents({window_.get()},
                               {menu_window.get(), nested_menu_window.get()});
  VerifyCanDispatchTouchEvents(
      {}, {window_.get(), menu_window.get(), nested_menu_window.get()});
  VerifyCanDispatchKeyEvents({window_.get()},
                             {menu_window.get(), nested_menu_window.get()});

  wl_touch_send_down(server_.seat()->touch()->resource(), ++serial, 0,
                     toplevel_surface->resource(), 0 /* id */,
                     wl_fixed_from_int(50), wl_fixed_from_int(100));

  Sync();

  // Only |window_| can dispatch MouseEvents and KeyEvents.
  VerifyCanDispatchMouseEvents({window_.get()},
                               {menu_window.get(), nested_menu_window.get()});
  VerifyCanDispatchTouchEvents({window_.get()},
                               {menu_window.get(), nested_menu_window.get()});
  VerifyCanDispatchKeyEvents({window_.get()},
                             {menu_window.get(), nested_menu_window.get()});

  wl::MockSurface* menu_window_surface =
      server_.GetObject<wl::MockSurface>(menu_window->GetWidget());

  wl_pointer_send_leave(server_.seat()->pointer()->resource(), ++serial,
                        toplevel_surface->resource());
  wl_pointer_send_enter(server_.seat()->pointer()->resource(), ++serial,
                        menu_window_surface->resource(), 0, 0);
  wl_touch_send_up(server_.seat()->touch()->resource(), ++serial, 1000,
                   0 /* id */);
  wl_keyboard_send_leave(server_.seat()->keyboard()->resource(), ++serial,
                         toplevel_surface->resource());

  Sync();

  // Only |menu_window| can dispatch MouseEvents.
  VerifyCanDispatchMouseEvents({menu_window.get()},
                               {window_.get(), nested_menu_window.get()});
  VerifyCanDispatchTouchEvents(
      {}, {window_.get(), menu_window.get(), nested_menu_window.get()});
  VerifyCanDispatchKeyEvents(
      {}, {window_.get(), menu_window.get(), nested_menu_window.get()});

  wl::MockSurface* nested_menu_window_surface =
      server_.GetObject<wl::MockSurface>(nested_menu_window->GetWidget());

  wl_pointer_send_leave(server_.seat()->pointer()->resource(), ++serial,
                        menu_window_surface->resource());
  wl_pointer_send_enter(server_.seat()->pointer()->resource(), ++serial,
                        nested_menu_window_surface->resource(), 0, 0);

  Sync();

  // Only |nested_menu_window| can dispatch MouseEvents.
  VerifyCanDispatchMouseEvents({nested_menu_window.get()},
                               {window_.get(), menu_window.get()});
  VerifyCanDispatchTouchEvents(
      {}, {window_.get(), menu_window.get(), nested_menu_window.get()});
  VerifyCanDispatchKeyEvents(
      {}, {window_.get(), menu_window.get(), nested_menu_window.get()});
}

TEST_P(WaylandWindowTest, DispatchWindowMove) {
  EXPECT_CALL(*GetXdgToplevel(), Move(_));
  ui::GetWmMoveResizeHandler(*window_)->DispatchHostWindowDragMovement(HTCAPTION, gfx::Point());
}

// Makes sure hit tests are converted into right edges.
TEST_P(WaylandWindowTest, DispatchWindowResize) {
  std::vector<int> hit_test_values;
  InitializeWithSupportedHitTestValues(&hit_test_values);

  auto* wm_move_resize_handler = ui::GetWmMoveResizeHandler(*window_);

  for (const int value : hit_test_values) {
    {
      uint32_t direction = wl::IdentifyDirection(*(connection_.get()), value);
      EXPECT_CALL(*GetXdgToplevel(), Resize(_, Eq(direction)));
      wm_move_resize_handler->DispatchHostWindowDragMovement(value,
                                                             gfx::Point());
    }
  }
}

// Tests WaylandWindow repositions menu windows to be relative to parent window
// in a right way.
TEST_P(WaylandWindowTest, AdjustPopupBounds) {
  PopupPosition menu_window_positioner, nested_menu_window_positioner;

  if (GetParam() == kXdgShellV6) {
    menu_window_positioner = {
        gfx::Rect(439, 46, 1, 30), gfx::Size(287, 409),
        ZXDG_POSITIONER_V6_ANCHOR_BOTTOM | ZXDG_POSITIONER_V6_ANCHOR_RIGHT,
        ZXDG_POSITIONER_V6_GRAVITY_BOTTOM | ZXDG_POSITIONER_V6_GRAVITY_RIGHT,
        ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_X |
            ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_Y};

    nested_menu_window_positioner = {
        gfx::Rect(4, 80, 279, 1), gfx::Size(305, 99),
        ZXDG_POSITIONER_V6_ANCHOR_TOP | ZXDG_POSITIONER_V6_ANCHOR_RIGHT,
        ZXDG_POSITIONER_V6_GRAVITY_BOTTOM | ZXDG_POSITIONER_V6_GRAVITY_RIGHT,
        ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_X |
            ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_SLIDE_Y};
  } else {
    menu_window_positioner = {gfx::Rect(439, 46, 1, 30), gfx::Size(287, 409),
                              XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT,
                              XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT,
                              XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_X |
                                  XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y};

    nested_menu_window_positioner = {
        gfx::Rect(4, 80, 279, 1), gfx::Size(305, 99),
        XDG_POSITIONER_ANCHOR_TOP_RIGHT, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT,
        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X |
            XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_SLIDE_Y};
  }

  auto* toplevel_window = window_.get();
  toplevel_window->SetBounds(gfx::Rect(0, 0, 739, 574));

  // Case 1: the top menu window is positioned normally.
  MockPlatformWindowDelegate menu_window_delegate;
  gfx::Rect menu_window_bounds(gfx::Point(440, 76),
                               menu_window_positioner.size);
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, toplevel_window->GetWidget(),
      menu_window_bounds, &menu_window_delegate);
  EXPECT_TRUE(menu_window);

  Sync();

  gfx::AcceleratedWidget menu_window_widget = menu_window->GetWidget();
  VerifyXdgPopupPosition(menu_window_widget, menu_window_positioner);

  EXPECT_CALL(menu_window_delegate, OnBoundsChanged(_)).Times(0);
  SendConfigureEventPopup(menu_window_widget, menu_window_bounds);

  Sync();

  EXPECT_EQ(menu_window->GetBounds(), menu_window_bounds);

  // Case 2: the nested menu window is positioned normally.
  MockPlatformWindowDelegate nested_menu_window_delegate;
  gfx::Rect nested_menu_window_bounds(gfx::Point(723, 156),
                                      nested_menu_window_positioner.size);
  std::unique_ptr<WaylandWindow> nested_menu_window =
      CreateWaylandWindowWithParams(
          PlatformWindowType::kMenu, menu_window_widget,
          nested_menu_window_bounds, &nested_menu_window_delegate);
  EXPECT_TRUE(nested_menu_window);

  Sync();

  gfx::AcceleratedWidget nested_menu_window_widget =
      nested_menu_window->GetWidget();
  VerifyXdgPopupPosition(nested_menu_window_widget,
                         nested_menu_window_positioner);

  EXPECT_CALL(nested_menu_window_delegate, OnBoundsChanged(_)).Times(0);
  const gfx::Point origin(nested_menu_window_positioner.anchor_rect.x() +
                              nested_menu_window_positioner.anchor_rect.width(),
                          nested_menu_window_positioner.anchor_rect.y());
  gfx::Rect calculated_nested_bounds = nested_menu_window_bounds;
  calculated_nested_bounds.set_origin(origin);
  SendConfigureEventPopup(nested_menu_window_widget, calculated_nested_bounds);

  Sync();

  EXPECT_EQ(nested_menu_window->GetBounds(), nested_menu_window_bounds);

  // Case 3: imagine the menu window was positioned near to the right edge of a
  // display. Nothing changes in the way how WaylandWindow calculates bounds,
  // because the Wayland compositor does not provide global location of windows.
  // Though, the compositor can reposition the window (flip along x or y axis or
  // slide along those axis). WaylandWindow just needs to correctly translate
  // bounds from relative to parent to be relative to screen. The Wayland
  // compositor does not reposition the menu, because it fits the screen, but
  // the nested menu window is repositioned to the left.
  EXPECT_CALL(
      nested_menu_window_delegate,
      OnBoundsChanged(gfx::Rect({139, 156}, nested_menu_window_bounds.size())));
  calculated_nested_bounds.set_origin({-301, 80});
  SendConfigureEventPopup(nested_menu_window_widget, calculated_nested_bounds);

  Sync();

  // Case 4: imagine the top level window was moved down to the bottom edge of a
  // display and only tab strip with 3-dot menu buttons left visible. In this
  // case, Chromium also does not know about that and positions the window
  // normally (normal bounds are sent), but the Wayland compositor flips the top
  // menu window along y-axis and fixes bounds of a top level window so that it
  // is located (from the Chromium point of view) below origin of the menu
  // window.
  EXPECT_CALL(delegate_, OnBoundsChanged(
                             gfx::Rect({0, 363}, window_->GetBounds().size())));
  EXPECT_CALL(menu_window_delegate,
              OnBoundsChanged(gfx::Rect({440, 0}, menu_window_bounds.size())));
  SendConfigureEventPopup(menu_window_widget,
                          gfx::Rect({440, -363}, menu_window_bounds.size()));

  Sync();

  // The nested menu window is also repositioned accordingly, but it's not
  // Wayland compositor reposition, but rather reposition from the Chromium
  // side. Thus, we have to check that anchor rect is correct.
  nested_menu_window.reset();
  nested_menu_window_bounds.set_origin({723, 258});
  nested_menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, menu_window_widget, nested_menu_window_bounds,
      &nested_menu_window_delegate);
  EXPECT_TRUE(nested_menu_window);

  Sync();

  nested_menu_window_widget = nested_menu_window->GetWidget();
  // We must get the anchor on gfx::Point(4, 258).
  nested_menu_window_positioner.anchor_rect.set_origin({4, 258});
  VerifyXdgPopupPosition(nested_menu_window_widget,
                         nested_menu_window_positioner);

  Sync();

  EXPECT_CALL(nested_menu_window_delegate, OnBoundsChanged(_)).Times(0);
  calculated_nested_bounds.set_origin({283, 258});
  SendConfigureEventPopup(nested_menu_window_widget, calculated_nested_bounds);

  Sync();

  // Case 5: this case involves case 4. Thus, it concerns only the nested menu
  // window. imagine that the top menu window is flipped along y-axis and
  // positioned near to the right side of a display. The nested menu window is
  // flipped along x-axis by the compositor and WaylandWindow must calculate
  // bounds back to be relative to display correctly. If the window is near to
  // the left edge of a display, nothing is going to change, and the origin will
  // be the same as in the previous case.
  EXPECT_CALL(
      nested_menu_window_delegate,
      OnBoundsChanged(gfx::Rect({149, 258}, nested_menu_window_bounds.size())));
  calculated_nested_bounds.set_origin({-291, 258});
  SendConfigureEventPopup(nested_menu_window_widget, calculated_nested_bounds);

  Sync();

  // Case 6: imagine the top level window was moved back to normal position. In
  // this case, the Wayland compositor positions the menu window normally and
  // the WaylandWindow repositions the top level window back to 0,0 (which had
  // an offset to compensate the position of the menu window fliped along
  // y-axis. It just has had negative y value, which is wrong for Chromium.
  EXPECT_CALL(delegate_,
              OnBoundsChanged(gfx::Rect({0, 0}, window_->GetBounds().size())));
  EXPECT_CALL(menu_window_delegate,
              OnBoundsChanged(gfx::Rect({440, 76}, menu_window_bounds.size())));
  SendConfigureEventPopup(menu_window_widget,
                          gfx::Rect({440, 76}, menu_window_bounds.size()));

  Sync();

  VerifyAndClearExpectations();
}

ACTION_P(VerifyRegion, ptr) {
  wl::TestRegion* region = wl::GetUserDataAs<wl::TestRegion>(arg0);
  EXPECT_EQ(*ptr, region->getBounds());
}

TEST_P(WaylandWindowTest, SetOpaqueRegion) {
  wl::MockSurface* mock_surface =
      server_.GetObject<wl::MockSurface>(window_->GetWidget());

  gfx::Rect new_bounds(0, 0, 500, 600);
  auto state_array = MakeStateArray({XDG_TOPLEVEL_STATE_ACTIVATED});
  SendConfigureEvent(new_bounds.width(), new_bounds.height(), 1,
                     state_array.get());

  SkIRect rect =
      SkIRect::MakeXYWH(0, 0, new_bounds.width(), new_bounds.height());
  EXPECT_CALL(*mock_surface, SetOpaqueRegion(_)).WillOnce(VerifyRegion(&rect));

  Sync();

  VerifyAndClearExpectations();

  new_bounds.set_size(gfx::Size(1000, 534));
  SendConfigureEvent(new_bounds.width(), new_bounds.height(), 2,
                     state_array.get());

  rect = SkIRect::MakeXYWH(0, 0, new_bounds.width(), new_bounds.height());
  EXPECT_CALL(*mock_surface, SetOpaqueRegion(_)).WillOnce(VerifyRegion(&rect));

  Sync();

  VerifyAndClearExpectations();
}

TEST_P(WaylandWindowTest, OnCloseRequest) {
  EXPECT_CALL(delegate_, OnCloseRequest());

  if (GetParam() == kXdgShellV6)
    zxdg_toplevel_v6_send_close(xdg_surface_->xdg_toplevel()->resource());
  else
    xdg_toplevel_send_close(xdg_surface_->xdg_toplevel()->resource());

  Sync();
}

TEST_P(WaylandWindowTest, SubsurfaceSimpleParent) {
  VerifyAndClearExpectations();

  std::unique_ptr<WaylandWindow> second_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kWindow, gfx::kNullAcceleratedWidget,
      gfx::Rect(0, 0, 640, 480), &delegate_);
  EXPECT_TRUE(second_window);

  // Test case 1: if the subsurface is provided with a parent widget, it must
  // always use that as a parent.
  gfx::Rect subsurface_bounds(gfx::Point(15, 15), gfx::Size(10, 10));
  std::unique_ptr<WaylandWindow> subsuface_window =
      CreateWaylandWindowWithParams(PlatformWindowType::kTooltip,
                                    window_->GetWidget(), subsurface_bounds,
                                    &delegate_);
  EXPECT_TRUE(subsuface_window);

  // The subsurface mustn't take the focused window as a parent, but use the
  // provided one.
  second_window->SetPointerFocus(true);
  subsuface_window->Show(false);

  Sync();

  auto* mock_surface_subsurface =
      server_.GetObject<wl::MockSurface>(subsuface_window->GetWidget());
  auto* test_subsurface = mock_surface_subsurface->sub_surface();

  EXPECT_EQ(test_subsurface->position(), subsurface_bounds.origin());
  EXPECT_FALSE(test_subsurface->sync());

  auto* parent_resource =
      server_.GetObject<wl::MockSurface>(window_->GetWidget())->resource();
  EXPECT_EQ(parent_resource, test_subsurface->parent_resource());

  // Test case 2: the subsurface must use the focused window as its parent.
  subsuface_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kTooltip, gfx::kNullAcceleratedWidget,
      subsurface_bounds, &delegate_);
  EXPECT_TRUE(subsuface_window);

  // The tooltip must take the focused window.
  second_window->SetPointerFocus(true);
  subsuface_window->Show(false);

  Sync();

  // Get new surface after recreating the WaylandSubsurface.
  mock_surface_subsurface =
      server_.GetObject<wl::MockSurface>(subsuface_window->GetWidget());
  test_subsurface = mock_surface_subsurface->sub_surface();

  auto* second_parent_resource =
      server_.GetObject<wl::MockSurface>(second_window->GetWidget())
          ->resource();
  EXPECT_EQ(second_parent_resource, test_subsurface->parent_resource());

  subsuface_window->Hide();

  Sync();

  // The subsurface must take the focused window.
  second_window->SetPointerFocus(false);
  window_->SetPointerFocus(true);
  subsuface_window->Show(false);

  Sync();

  // The subsurface is invalidated on each Hide call.
  test_subsurface = mock_surface_subsurface->sub_surface();

  // The |window_|'s resource must be the parent resource.
  EXPECT_EQ(parent_resource, test_subsurface->parent_resource());

  window_->SetPointerFocus(false);
}

// Case 1: When the menu bounds are positive and there is a positive,
// non-zero anchor width
TEST_P(WaylandWindowTest, NestedPopupMenu) {
  VerifyAndClearExpectations();

  gfx::Rect menu_window_bounds(gfx::Rect(4, 20, 8, 20));
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, window_->GetWidget(), menu_window_bounds,
      &delegate_);
  EXPECT_TRUE(menu_window);

  VerifyAndClearExpectations();

  gfx::Rect nestedPopup_bounds(gfx::Rect(10, 30, 40, 20));
  std::unique_ptr<WaylandWindow> nestedPopup_window =
      CreateWaylandWindowWithParams(PlatformWindowType::kPopup,
                                    menu_window->GetWidget(),
                                    nestedPopup_bounds, &delegate_);
  EXPECT_TRUE(nestedPopup_window);

  VerifyAndClearExpectations();

  nestedPopup_window->SetPointerFocus(true);

  Sync();

  auto* mock_surface_nested_popup =
      GetPopupByWidget(nestedPopup_window->GetWidget());

  ASSERT_TRUE(mock_surface_nested_popup);

  auto anchor_width = (mock_surface_nested_popup->anchor_rect()).width();
  EXPECT_EQ(4, anchor_width);

  nestedPopup_window->SetPointerFocus(false);
}

// Case 2: When the menu bounds are positive and there is a negative or
// zero anchor width
TEST_P(WaylandWindowTest, NestedPopupMenu1) {
  VerifyAndClearExpectations();

  gfx::Rect menu_window_bounds(gfx::Rect(6, 20, 8, 20));
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, window_->GetWidget(), menu_window_bounds,
      &delegate_);
  EXPECT_TRUE(menu_window);

  VerifyAndClearExpectations();

  gfx::Rect nestedPopup_bounds(gfx::Rect(10, 30, 10, 20));
  std::unique_ptr<WaylandWindow> nestedPopup_window =
      CreateWaylandWindowWithParams(PlatformWindowType::kPopup,
                                    menu_window->GetWidget(),
                                    nestedPopup_bounds, &delegate_);
  EXPECT_TRUE(nestedPopup_window);

  VerifyAndClearExpectations();

  nestedPopup_window->SetPointerFocus(true);

  Sync();

  auto* mock_surface_nested_popup =
      GetPopupByWidget(nestedPopup_window->GetWidget());

  ASSERT_TRUE(mock_surface_nested_popup);

  auto anchor_width = (mock_surface_nested_popup->anchor_rect()).width();
  EXPECT_EQ(1, anchor_width);

  nestedPopup_window->SetPointerFocus(false);
}

// Case 3: When the menu bounds are negative and there is a positive,
// non-zero anchor width
TEST_P(WaylandWindowTest, NestedPopupMenu2) {
  VerifyAndClearExpectations();

  gfx::Rect menu_window_bounds(gfx::Rect(10, 20, 40, 20));
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, window_->GetWidget(), menu_window_bounds,
      &delegate_);
  EXPECT_TRUE(menu_window);

  VerifyAndClearExpectations();

  gfx::Rect nestedPopup_bounds(gfx::Rect(5, 30, 21, 20));
  std::unique_ptr<WaylandWindow> nestedPopup_window =
      CreateWaylandWindowWithParams(PlatformWindowType::kPopup,
                                    menu_window->GetWidget(),
                                    nestedPopup_bounds, &delegate_);
  EXPECT_TRUE(nestedPopup_window);

  VerifyAndClearExpectations();

  nestedPopup_window->SetPointerFocus(true);

  Sync();

  auto* mock_surface_nested_popup =
      GetPopupByWidget(nestedPopup_window->GetWidget());

  ASSERT_TRUE(mock_surface_nested_popup);

  auto anchor_width = (mock_surface_nested_popup->anchor_rect()).width();
  EXPECT_EQ(8, anchor_width);

  nestedPopup_window->SetPointerFocus(false);
}

// Case 4: When the menu bounds are negative and there is a negative,
// zero anchor width
TEST_P(WaylandWindowTest, NestedPopupMenu3) {
  VerifyAndClearExpectations();

  gfx::Rect menu_window_bounds(gfx::Rect(10, 20, 20, 20));
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, window_->GetWidget(), menu_window_bounds,
      &delegate_);
  EXPECT_TRUE(menu_window);

  VerifyAndClearExpectations();

  gfx::Rect nestedPopup_bounds(gfx::Rect(5, 30, 21, 20));
  std::unique_ptr<WaylandWindow> nestedPopup_window =
      CreateWaylandWindowWithParams(PlatformWindowType::kPopup,
                                    menu_window->GetWidget(),
                                    nestedPopup_bounds, &delegate_);
  EXPECT_TRUE(nestedPopup_window);

  VerifyAndClearExpectations();

  nestedPopup_window->SetPointerFocus(true);

  Sync();

  auto* mock_surface_nested_popup =
      GetPopupByWidget(nestedPopup_window->GetWidget());

  ASSERT_TRUE(mock_surface_nested_popup);

  auto anchor_width = (mock_surface_nested_popup->anchor_rect()).width();

  EXPECT_EQ(1, anchor_width);

  nestedPopup_window->SetPointerFocus(false);
}

TEST_P(WaylandWindowTest, SubsurfaceNestedParent) {
  VerifyAndClearExpectations();

  gfx::Rect menu_window_bounds(gfx::Point(10, 10), gfx::Size(100, 100));
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, window_->GetWidget(), menu_window_bounds,
      &delegate_);
  EXPECT_TRUE(menu_window);

  VerifyAndClearExpectations();

  gfx::Rect subsurface_bounds(gfx::Point(15, 15), gfx::Size(10, 10));
  std::unique_ptr<WaylandWindow> subsuface_window =
      CreateWaylandWindowWithParams(PlatformWindowType::kTooltip,
                                    menu_window->GetWidget(), subsurface_bounds,
                                    &delegate_);
  EXPECT_TRUE(subsuface_window);

  VerifyAndClearExpectations();

  menu_window->SetPointerFocus(true);

  subsuface_window->Show(false);

  Sync();

  auto* mock_surface_subsurface =
      server_.GetObject<wl::MockSurface>(subsuface_window->GetWidget());
  auto* test_subsurface = mock_surface_subsurface->sub_surface();

  auto new_origin = subsurface_bounds.origin() -
                    menu_window_bounds.origin().OffsetFromOrigin();
  EXPECT_EQ(test_subsurface->position(), new_origin);

  menu_window->SetPointerFocus(false);
}

TEST_P(WaylandWindowTest, OnSizeConstraintsChanged) {
  const bool kBooleans[] = {false, true};
  for (bool has_min_size : kBooleans) {
    for (bool has_max_size : kBooleans) {
      base::Optional<gfx::Size> min_size =
          has_min_size ? base::Optional<gfx::Size>(gfx::Size(100, 200))
                       : base::nullopt;
      base::Optional<gfx::Size> max_size =
          has_max_size ? base::Optional<gfx::Size>(gfx::Size(300, 400))
                       : base::nullopt;
      EXPECT_CALL(delegate_, GetMinimumSizeForWindow())
          .WillOnce(Return(min_size));
      EXPECT_CALL(delegate_, GetMaximumSizeForWindow())
          .WillOnce(Return(max_size));

      EXPECT_CALL(*GetXdgToplevel(), SetMinSize(100, 200))
          .Times(has_min_size ? 1 : 0);
      EXPECT_CALL(*GetXdgToplevel(), SetMaxSize(300, 400))
          .Times(has_max_size ? 1 : 0);

      window_->SizeConstraintsChanged();
      Sync();

      VerifyAndClearExpectations();
    }
  }
}

TEST_P(WaylandWindowTest, DestroysCreatesSurfaceOnHideShow) {
  MockPlatformWindowDelegate delegate;
  auto window = CreateWaylandWindowWithParams(
      PlatformWindowType::kWindow, gfx::kNullAcceleratedWidget,
      gfx::Rect(0, 0, 100, 100), &delegate);
  ASSERT_TRUE(window);

  Sync();

  auto* mock_surface = server_.GetObject<wl::MockSurface>(window->GetWidget());
  EXPECT_TRUE(mock_surface->xdg_surface());
  EXPECT_TRUE(mock_surface->xdg_surface()->xdg_toplevel());

  Sync();

  window->Hide();

  Sync();

  EXPECT_FALSE(mock_surface->xdg_surface());

  window->Show(false);

  Sync();

  EXPECT_TRUE(mock_surface->xdg_surface());
  EXPECT_TRUE(mock_surface->xdg_surface()->xdg_toplevel());
}

TEST_P(WaylandWindowTest, DestroysCreatesPopupsOnHideShow) {
  MockPlatformWindowDelegate delegate;
  auto window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, window_->GetWidget(), gfx::Rect(0, 0, 50, 50),
      &delegate);
  ASSERT_TRUE(window);

  Sync();

  auto* mock_surface = server_.GetObject<wl::MockSurface>(window->GetWidget());
  EXPECT_TRUE(mock_surface->xdg_surface());
  EXPECT_TRUE(mock_surface->xdg_surface()->xdg_popup());

  Sync();

  window->Hide();

  Sync();

  EXPECT_FALSE(mock_surface->xdg_surface());

  window->Show(false);

  Sync();

  EXPECT_TRUE(mock_surface->xdg_surface());
  EXPECT_TRUE(mock_surface->xdg_surface()->xdg_popup());
}

// Tests that if the window gets hidden and shown again, the title, app id and
// size constraints remain the same.
TEST_P(WaylandWindowTest, SetsPropertiesOnShow) {
  constexpr char kAppId[] = "wayland_test";
  const base::string16 kTitle(base::UTF8ToUTF16("WaylandWindowTest"));

  PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(0, 0, 100, 100);
  properties.type = PlatformWindowType::kWindow;
  properties.wm_class_class = kAppId;

  MockPlatformWindowDelegate delegate;
  auto window = WaylandWindow::Create(&delegate, connection_.get(),
                                      std::move(properties));
  DCHECK(window);
  window->Show(false);

  Sync();

  auto* mock_surface = server_.GetObject<wl::MockSurface>(window->GetWidget());
  auto* mock_xdg_toplevel = mock_surface->xdg_surface()->xdg_toplevel();

  // Only app id must be set now.
  EXPECT_EQ(std::string(kAppId), mock_xdg_toplevel->app_id());
  EXPECT_TRUE(mock_xdg_toplevel->title().empty());
  EXPECT_TRUE(mock_xdg_toplevel->min_size().IsEmpty());
  EXPECT_TRUE(mock_xdg_toplevel->max_size().IsEmpty());

  // Now, propagate size constraints and title.
  base::Optional<gfx::Size> min_size(gfx::Size(1, 1));
  base::Optional<gfx::Size> max_size(gfx::Size(100, 100));
  EXPECT_CALL(delegate, GetMinimumSizeForWindow()).WillOnce(Return(min_size));
  EXPECT_CALL(delegate, GetMaximumSizeForWindow()).WillOnce(Return(max_size));

  EXPECT_CALL(*mock_xdg_toplevel,
              SetMinSize(min_size.value().width(), min_size.value().height()));
  EXPECT_CALL(*mock_xdg_toplevel,
              SetMaxSize(max_size.value().width(), max_size.value().height()));
  EXPECT_CALL(*mock_xdg_toplevel, SetTitle(base::UTF16ToUTF8(kTitle)));

  window->SetTitle(kTitle);
  window->SizeConstraintsChanged();

  Sync();

  window->Hide();

  Sync();

  window->Show(false);

  Sync();

  mock_xdg_toplevel = mock_surface->xdg_surface()->xdg_toplevel();

  // We can't mock all those methods above as long as the xdg_toplevel is
  // created and destroyed on each show and hide call. However, it is the same
  // WaylandSurface object that cached the values we set and must restore them
  // on Show().
  EXPECT_EQ(mock_xdg_toplevel->min_size(), min_size.value());
  EXPECT_EQ(mock_xdg_toplevel->max_size(), max_size.value());
  EXPECT_EQ(std::string(kAppId), mock_xdg_toplevel->app_id());
  EXPECT_EQ(mock_xdg_toplevel->title(), base::UTF16ToUTF8(kTitle));
}

// Tests that a popup window is created using the serial of button press events
// as required by the Wayland protocol spec.
TEST_P(WaylandWindowTest, CreatesPopupOnButtonPressSerial) {
  wl_seat_send_capabilities(
      server_.seat()->resource(),
      WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);

  Sync();

  constexpr uint32_t enter_serial = 1;
  constexpr uint32_t button_press_serial = 2;
  constexpr uint32_t button_release_serial = 3;

  wl::MockSurface* toplevel_surface =
      server_.GetObject<wl::MockSurface>(window_->GetWidget());
  struct wl_array empty;
  wl_array_init(&empty);
  wl_keyboard_send_enter(server_.seat()->keyboard()->resource(), enter_serial,
                         toplevel_surface->resource(), &empty);

  // Send two events - button down and button up.
  wl_pointer_send_button(server_.seat()->pointer()->resource(),
                         button_press_serial, 1002, BTN_LEFT,
                         WL_POINTER_BUTTON_STATE_PRESSED);
  wl_pointer_send_button(server_.seat()->pointer()->resource(),
                         button_release_serial, 1004, BTN_LEFT,
                         WL_POINTER_BUTTON_STATE_RELEASED);
  Sync();

  // Create a popup window and verify the client used correct serial.
  MockPlatformWindowDelegate delegate;
  auto popup = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, window_->GetWidget(), gfx::Rect(0, 0, 50, 50),
      &delegate);
  ASSERT_TRUE(popup);

  Sync();

  auto* test_popup = GetPopupByWidget(popup->GetWidget());
  ASSERT_TRUE(test_popup);
  EXPECT_NE(test_popup->grab_serial(), button_release_serial);
  EXPECT_EQ(test_popup->grab_serial(), button_press_serial);
}

// Tests that a popup window is created using the serial of touch down events
// as required by the Wayland protocol spec.
TEST_P(WaylandWindowTest, CreatesPopupOnTouchDownSerial) {
  wl_seat_send_capabilities(
      server_.seat()->resource(),
      WL_SEAT_CAPABILITY_TOUCH | WL_SEAT_CAPABILITY_KEYBOARD);

  Sync();

  constexpr uint32_t enter_serial = 1;
  constexpr uint32_t touch_down_serial = 2;
  constexpr uint32_t touch_up_serial = 3;

  wl::MockSurface* toplevel_surface =
      server_.GetObject<wl::MockSurface>(window_->GetWidget());
  struct wl_array empty;
  wl_array_init(&empty);
  wl_keyboard_send_enter(server_.seat()->keyboard()->resource(), enter_serial,
                         toplevel_surface->resource(), &empty);

  // Send two events - touch down and touch up.
  wl_touch_send_down(server_.seat()->touch()->resource(), touch_down_serial, 0,
                     surface_->resource(), 0 /* id */, wl_fixed_from_int(50),
                     wl_fixed_from_int(100));
  wl_touch_send_up(server_.seat()->touch()->resource(), touch_up_serial, 1000,
                   0 /* id */);

  Sync();

  // Create a popup window and verify the client used correct serial.
  MockPlatformWindowDelegate delegate;
  auto popup = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, window_->GetWidget(), gfx::Rect(0, 0, 50, 50),
      &delegate);
  ASSERT_TRUE(popup);

  Sync();

  auto* test_popup = GetPopupByWidget(popup->GetWidget());
  ASSERT_TRUE(test_popup);
  EXPECT_NE(test_popup->grab_serial(), touch_up_serial);
  EXPECT_EQ(test_popup->grab_serial(), touch_down_serial);
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandWindowTest,
                         ::testing::Values(kXdgShellStable));
INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandWindowTest,
                         ::testing::Values(kXdgShellV6));

}  // namespace ui
