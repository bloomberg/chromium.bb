// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_window.h"

#include <memory>
#include <utility>

#include <cursor-shapes-unstable-v1-client-protocol.h>
#include <linux/input.h>
#include <wayland-server-core.h>
#include <xdg-shell-server-protocol.h>
#include <xdg-shell-unstable-v6-server-protocol.h>

#include "base/environment.h"
#include "base/files/file_util.h"
#include "base/memory/scoped_refptr.h"
#include "base/nix/xdg_util.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_command_line.h"
#include "build/chromeos_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/cursor/mojom/cursor_type.mojom-shared.h"
#include "ui/base/hit_test.h"
#include "ui/base/ui_base_types.h"
#include "ui/display/display.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/gfx/overlay_transform.h"
#include "ui/ozone/common/bitmap_cursor.h"
#include "ui/ozone/common/features.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"
#include "ui/ozone/platform/wayland/host/wayland_connection_test_api.h"
#include "ui/ozone/platform/wayland/host/wayland_output.h"
#include "ui/ozone/platform/wayland/host/wayland_output_manager.h"
#include "ui/ozone/platform/wayland/host/wayland_seat.h"
#include "ui/ozone/platform/wayland/host/wayland_subsurface.h"
#include "ui/ozone/platform/wayland/host/wayland_zcr_cursor_shapes.h"
#include "ui/ozone/platform/wayland/mojom/wayland_overlay_config.mojom.h"
#include "ui/ozone/platform/wayland/test/mock_pointer.h"
#include "ui/ozone/platform/wayland/test/mock_surface.h"
#include "ui/ozone/platform/wayland/test/test_keyboard.h"
#include "ui/ozone/platform/wayland/test/test_output.h"
#include "ui/ozone/platform/wayland/test/test_region.h"
#include "ui/ozone/platform/wayland/test/test_touch.h"
#include "ui/ozone/platform/wayland/test/test_wayland_server_thread.h"
#include "ui/ozone/platform/wayland/test/wayland_test.h"
#include "ui/ozone/public/ozone_switches.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_init_properties.h"
#include "ui/platform_window/wm/wm_move_resize_handler.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Eq;
using ::testing::InvokeWithoutArgs;
using ::testing::Mock;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::StrEq;
using ::testing::Values;

namespace ui {

namespace {

constexpr float kDefaultCursorScale = 1.f;

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

base::ScopedFD MakeFD() {
  base::FilePath temp_path;
  EXPECT_TRUE(base::CreateTemporaryFile(&temp_path));
  auto file =
      base::File(temp_path, base::File::FLAG_READ | base::File::FLAG_WRITE |
                                base::File::FLAG_CREATE_ALWAYS);
  return base::ScopedFD(file.TakePlatformFile());
}

class MockZcrCursorShapes : public WaylandZcrCursorShapes {
 public:
  MockZcrCursorShapes() : WaylandZcrCursorShapes(nullptr, nullptr) {}
  MockZcrCursorShapes(const MockZcrCursorShapes&) = delete;
  MockZcrCursorShapes& operator=(const MockZcrCursorShapes&) = delete;
  ~MockZcrCursorShapes() override = default;

  MOCK_METHOD(void, SetCursorShape, (int32_t), (override));
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

  WaylandWindowTest(const WaylandWindowTest&) = delete;
  WaylandWindowTest& operator=(const WaylandWindowTest&) = delete;

  void SetUp() override {
    disabled_features_.push_back(
        ui::kWaylandSurfaceSubmissionInPixelCoordinates);

    WaylandTest::SetUp();

    xdg_surface_ = surface_->xdg_surface();
    ASSERT_TRUE(xdg_surface_);
  }

 protected:
  void SendConfigureEventPopup(WaylandWindow* menu_window,
                               const gfx::Rect bounds) {
    auto* popup = GetTestXdgPopupByWindow(menu_window);
    ASSERT_TRUE(popup);
    if (GetParam().shell_version == wl::ShellVersion::kV6) {
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
                                        std::move(properties), true, true);

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

  MockZcrCursorShapes* InstallMockZcrCursorShapes() {
    auto zcr_cursor_shapes = std::make_unique<MockZcrCursorShapes>();
    MockZcrCursorShapes* mock_cursor_shapes = zcr_cursor_shapes.get();
    WaylandConnectionTestApi test_api(connection_.get());
    test_api.SetZcrCursorShapes(std::move(zcr_cursor_shapes));
    return mock_cursor_shapes;
  }

  void VerifyAndClearExpectations() {
    Mock::VerifyAndClearExpectations(xdg_surface_);
    Mock::VerifyAndClearExpectations(&delegate_);
  }

  void VerifyXdgPopupPosition(WaylandWindow* menu_window,
                              const PopupPosition& position) {
    auto* popup = GetTestXdgPopupByWindow(menu_window);
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

  wl::TestXdgPopup* GetTestXdgPopupByWindow(WaylandWindow* window) {
    wl::MockSurface* mock_surface = server_.GetObject<wl::MockSurface>(
        window->root_surface()->GetSurfaceId());
    if (mock_surface) {
      auto* mock_xdg_surface = mock_surface->xdg_surface();
      if (mock_xdg_surface)
        return mock_xdg_surface->xdg_popup();
    }
    return nullptr;
  }

  wl::MockXdgSurface* xdg_surface_;

  MouseEvent test_mouse_event_;
};

TEST_P(WaylandWindowTest, SetTitle) {
  EXPECT_CALL(*GetXdgToplevel(), SetTitle(StrEq("hello")));
  window_->SetTitle(u"hello");
}

TEST_P(WaylandWindowTest, UpdateVisualSizeConfiguresWaylandWindow) {
  const auto kNormalBounds = gfx::Rect{0, 0, 500, 300};
  uint32_t serial = 0;
  auto state = InitializeWlArrayWithActivatedState();

  window_->set_update_visual_size_immediately(false);
  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());

  // Configure event makes Wayland update bounds, but does not change toplevel
  // input region, opaque region or window geometry immediately. Such actions
  // are postponed to UpdateVisualSize();
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kNormalBounds)));
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kNormalBounds.width(),
                                               kNormalBounds.height()))
      .Times(0);
  EXPECT_CALL(*xdg_surface_, AckConfigure(1)).Times(0);
  EXPECT_CALL(*mock_surface, SetOpaqueRegion(_)).Times(0);
  EXPECT_CALL(*mock_surface, SetInputRegion(_)).Times(0);
  SendConfigureEvent(xdg_surface_, kNormalBounds.width(),
                     kNormalBounds.height(), ++serial, state.get());

  Sync();

  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kNormalBounds.width(),
                                               kNormalBounds.height()));
  EXPECT_CALL(*xdg_surface_, AckConfigure(1));
  EXPECT_CALL(*mock_surface, SetOpaqueRegion(_));
  EXPECT_CALL(*mock_surface, SetInputRegion(_));
  window_->UpdateVisualSize(kNormalBounds.size(), 1.0f);
}

// WaylandSurface state changes are sent to wayland compositor when
// ApplyPendingState() is called.
TEST_P(WaylandWindowTest, ApplyPendingStatesAndCommit) {
  window_->set_update_visual_size_immediately(false);
  window_->set_apply_pending_state_on_update_visual_size(false);

  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());

  // Set*() calls do not send wl_surface requests.
  EXPECT_CALL(*mock_surface, SetOpaqueRegion(_)).Times(0);
  EXPECT_CALL(*mock_surface, SetInputRegion(_)).Times(0);
  EXPECT_CALL(*mock_surface, SetBufferScale(2)).Times(0);

  std::vector<gfx::Rect> region_px = {gfx::Rect{0, 0, 500, 300}};
  window_->root_surface()->SetOpaqueRegion(&region_px);
  window_->root_surface()->SetInputRegion(region_px.data());
  window_->root_surface()->SetSurfaceBufferScale(2);

  Sync();

  // ApplyPendingState() generates wl_surface requests and Commit() causes a
  // wayland connection flush.
  EXPECT_CALL(*mock_surface, SetOpaqueRegion(_)).Times(1);
  EXPECT_CALL(*mock_surface, SetInputRegion(_)).Times(1);
  EXPECT_CALL(*mock_surface, SetBufferScale(2)).Times(1);
  EXPECT_CALL(*mock_surface, Commit()).Times(1);

  window_->root_surface()->ApplyPendingState();
  window_->root_surface()->Commit();

  Sync();
}

// Checks that decoration insets do not change final bounds and that
// WaylandToplevelWindow::HandleToplevelConfigure does correct rounding when
// some sides of insets divides by 2 with remainder.
TEST_P(WaylandWindowTest, SetDecorationInsets) {
  const auto kNormalBounds = gfx::Rect{0, 0, 956, 556};
  const auto kHiDpiScale = 2;
  const auto kHiDpiBounds = gfx::ScaleToRoundedRect(kNormalBounds, kHiDpiScale);

  window_->SetBounds(kNormalBounds);

  uint32_t serial = 0;
  auto state = InitializeWlArrayWithActivatedState();

  // Creating an output with scale 2.
  wl::TestOutput* output = server_.output();

  // Send the window to |output1|.
  wl::MockSurface* surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());
  ASSERT_TRUE(surface);
  wl_surface_send_enter(surface->resource(), output->resource());

  Sync();

  // Set insets for normal DPI.
  const gfx::Insets kDecorationInsets = {24, 28, 32, 28};
  auto bounds_with_insets = kNormalBounds;
  bounds_with_insets.Inset(kDecorationInsets);
  EXPECT_CALL(delegate_, OnBoundsChanged(_)).Times(0);
  EXPECT_CALL(*xdg_surface_,
              SetWindowGeometry(bounds_with_insets.x(), bounds_with_insets.y(),
                                bounds_with_insets.width(),
                                bounds_with_insets.height()));
  window_->SetDecorationInsets(&kDecorationInsets);
  // Setting the decoration insets does not trigger the immediate update of the
  // window geometry.  Emulate updating the visual size (sending the frame
  // update) for that.
  window_->UpdateVisualSize(kNormalBounds.size(), 1.0f);

  Sync();

  EXPECT_CALL(delegate_, OnBoundsChanged(_)).Times(0);
  EXPECT_CALL(*xdg_surface_,
              SetWindowGeometry(bounds_with_insets.x(), bounds_with_insets.y(),
                                bounds_with_insets.width(),
                                bounds_with_insets.height()));
  SendConfigureEvent(xdg_surface_, bounds_with_insets.width(),
                     bounds_with_insets.height(), ++serial, state.get());

  Sync();

  // Change scale.  This is the only time when we expect the bounds to change.
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kHiDpiBounds))).Times(1);
  output->SetScale(kHiDpiScale);
  output->Flush();

  Sync();

  // Pretend we are already rendering using new scale.
  window_->root_surface()->SetSurfaceBufferScale(kHiDpiScale);

  // Set new insets so that rounding does not result in integer.
  const gfx::Insets kDecorationInsets_2x = {48, 55, 63, 55};
  EXPECT_CALL(*xdg_surface_,
              SetWindowGeometry(bounds_with_insets.x(), bounds_with_insets.y(),
                                bounds_with_insets.width(),
                                bounds_with_insets.height()));
  window_->SetDecorationInsets(&kDecorationInsets_2x);
  // Setting the decoration insets does not trigger the immediate update of the
  // window geometry.  Emulate updating the visual size (sending the frame
  // update) for that.
  window_->UpdateVisualSize(kHiDpiBounds.size(), kHiDpiScale);

  Sync();

  // Now send configure events many times - bounds mustn't change.
  for (size_t i = 0; i < 10; i++) {
    EXPECT_CALL(delegate_, OnBoundsChanged(_)).Times(0);
    EXPECT_CALL(*xdg_surface_, SetWindowGeometry(bounds_with_insets.x(),
                                                 bounds_with_insets.y(),
                                                 bounds_with_insets.width(),
                                                 bounds_with_insets.height()));
    SendConfigureEvent(xdg_surface_, bounds_with_insets.width(),
                       bounds_with_insets.height(), ++serial, state.get());

    Sync();
  }
}

TEST_P(WaylandWindowTest, ShuffledUpdateVisualSizeOrder) {
  const auto kNormalBounds1 = gfx::Rect{0, 0, 500, 300};
  const auto kNormalBounds2 = gfx::Rect{0, 0, 800, 600};
  const auto kNormalBounds3 = gfx::Rect{0, 0, 700, 400};
  uint32_t serial = 1;

  window_->set_update_visual_size_immediately(false);

  // Send 3 configures and only ack the second one, the first pending configure
  // is cleared. The second can still be ack'ed.
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kNormalBounds1.width(),
                                               kNormalBounds1.height()))
      .Times(0);
  EXPECT_CALL(*xdg_surface_, AckConfigure(2)).Times(0);
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kNormalBounds2.width(),
                                               kNormalBounds2.height()));
  EXPECT_CALL(*xdg_surface_, AckConfigure(3));
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kNormalBounds3.width(),
                                               kNormalBounds3.height()));
  EXPECT_CALL(*xdg_surface_, AckConfigure(4));

  auto state = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(xdg_surface_, kNormalBounds1.width(),
                     kNormalBounds1.height(), ++serial, state.get());
  state = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(xdg_surface_, kNormalBounds2.width(),
                     kNormalBounds2.height(), ++serial, state.get());
  state = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(xdg_surface_, kNormalBounds3.width(),
                     kNormalBounds3.height(), ++serial, state.get());
  Sync();

  window_->UpdateVisualSize(kNormalBounds2.size(), 1.0f);
  window_->UpdateVisualSize(kNormalBounds1.size(), 1.0f);
  window_->UpdateVisualSize(kNormalBounds3.size(), 1.0f);
}

TEST_P(WaylandWindowTest, MismatchUpdateVisualSize) {
  const auto kNormalBounds1 = gfx::Rect{0, 0, 500, 300};
  const auto kNormalBounds2 = gfx::Rect{0, 0, 800, 600};
  const auto kNormalBounds3 = gfx::Rect{0, 0, 700, 400};
  uint32_t serial = 1;

  window_->set_update_visual_size_immediately(false);
  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());

  // UpdateVisualSize with different size from configure events does not
  // acknowledge toplevel configure.
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(_, _, _, _)).Times(0);
  EXPECT_CALL(*xdg_surface_, AckConfigure(_)).Times(0);
  EXPECT_CALL(*mock_surface, SetOpaqueRegion(_));
  EXPECT_CALL(*mock_surface, SetInputRegion(_));

  auto state = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(xdg_surface_, kNormalBounds1.width(),
                     kNormalBounds1.height(), ++serial, state.get());
  state = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(xdg_surface_, kNormalBounds2.width(),
                     kNormalBounds2.height(), ++serial, state.get());
  state = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(xdg_surface_, kNormalBounds3.width(),
                     kNormalBounds3.height(), ++serial, state.get());
  Sync();

  window_->UpdateVisualSize({100, 100}, 1.0f);
}

TEST_P(WaylandWindowTest, UpdateVisualSizeClearsPreviousUnackedConfigures) {
  const auto kNormalBounds1 = gfx::Rect{0, 0, 500, 300};
  const auto kNormalBounds2 = gfx::Rect{0, 0, 800, 600};
  const auto kNormalBounds3 = gfx::Rect{0, 0, 700, 400};
  uint32_t serial = 1;
  auto state = InitializeWlArrayWithActivatedState();

  window_->set_update_visual_size_immediately(false);

  // Send 3 configures and only ack the second one, the first pending configure
  // is cleared. The second can still be ack'ed.
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kNormalBounds1.width(),
                                               kNormalBounds1.height()))
      .Times(0);
  EXPECT_CALL(*xdg_surface_, AckConfigure(2)).Times(0);
  SendConfigureEvent(xdg_surface_, kNormalBounds1.width(),
                     kNormalBounds1.height(), ++serial, state.get());
  Sync();

  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kNormalBounds2.width(),
                                               kNormalBounds2.height()));
  EXPECT_CALL(*xdg_surface_, AckConfigure(3));
  state = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(xdg_surface_, kNormalBounds2.width(),
                     kNormalBounds2.height(), ++serial, state.get());
  Sync();

  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kNormalBounds3.width(),
                                               kNormalBounds3.height()));
  EXPECT_CALL(*xdg_surface_, AckConfigure(4));
  state = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(xdg_surface_, kNormalBounds3.width(),
                     kNormalBounds3.height(), ++serial, state.get());
  Sync();

  window_->UpdateVisualSize(kNormalBounds2.size(), 1.0f);
  window_->UpdateVisualSize(kNormalBounds1.size(), 1.0f);
  window_->UpdateVisualSize(kNormalBounds3.size(), 1.0f);
}

TEST_P(WaylandWindowTest, MaximizeAndRestore) {
  const auto kNormalBounds = gfx::Rect{0, 0, 500, 300};
  const auto kMaximizedBounds = gfx::Rect{0, 0, 800, 600};

  uint32_t serial = 0;

  // Make sure the window has normal state initially.
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kNormalBounds)));
  window_->SetBounds(kNormalBounds);
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());
  VerifyAndClearExpectations();

  // Deactivate the surface.
  auto empty_state = MakeStateArray({});
  SendConfigureEvent(xdg_surface_, 0, 0, ++serial, empty_state.get());

  Sync();

  auto active_maximized = MakeStateArray(
      {XDG_TOPLEVEL_STATE_ACTIVATED, XDG_TOPLEVEL_STATE_MAXIMIZED});
  EXPECT_CALL(*GetXdgToplevel(), SetMaximized());
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kMaximizedBounds.width(),
                                               kMaximizedBounds.height()));
  EXPECT_CALL(delegate_, OnActivationChanged(Eq(true)));
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kMaximizedBounds)));
  // Emulate a piece of behaviour of BrowserDesktopWindowTreeHostLinux, which is
  // the real delegate.  Its OnWindowStateChanged() may (through some chain of
  // calls) invoke SetWindowGeometry(), but that should not happen during the
  // change of the window state.
  // See https://crbug.com/1223005.
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _))
      .Times(1)
      .WillOnce(
          testing::Invoke([this]() { window_->SetDecorationInsets({}); }));
  window_->Maximize();
  SendConfigureEvent(xdg_surface_, kMaximizedBounds.width(),
                     kMaximizedBounds.height(), ++serial,
                     active_maximized.get());
  Sync();
  VerifyAndClearExpectations();

  auto inactive_maximized = MakeStateArray({XDG_TOPLEVEL_STATE_MAXIMIZED});
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kMaximizedBounds.width(),
                                               kMaximizedBounds.height()));
  EXPECT_CALL(delegate_, OnActivationChanged(Eq(false)));
  EXPECT_CALL(delegate_, OnBoundsChanged(_)).Times(0);
  SendConfigureEvent(xdg_surface_, kMaximizedBounds.width(),
                     kMaximizedBounds.height(), ++serial,
                     inactive_maximized.get());
  Sync();
  VerifyAndClearExpectations();

  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kMaximizedBounds.width(),
                                               kMaximizedBounds.height()));
  EXPECT_CALL(delegate_, OnActivationChanged(Eq(true)));
  EXPECT_CALL(delegate_, OnBoundsChanged(_)).Times(0);
  SendConfigureEvent(xdg_surface_, kMaximizedBounds.width(),
                     kMaximizedBounds.height(), ++serial,
                     active_maximized.get());
  Sync();
  VerifyAndClearExpectations();

  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kNormalBounds.width(),
                                               kNormalBounds.height()));
  // Emulate a piece of behaviour of BrowserDesktopWindowTreeHostLinux, which is
  // the real delegate.  Its OnWindowStateChanged() may (through some chain of
  // calls) invoke SetWindowGeometry(), but that should not happen during the
  // change of the window state.
  // See https://crbug.com/1223005.
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _))
      .Times(1)
      .WillOnce(
          testing::Invoke([this]() { window_->SetDecorationInsets({}); }));
  EXPECT_CALL(delegate_, OnActivationChanged(_)).Times(0);
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kNormalBounds)));
  EXPECT_CALL(*GetXdgToplevel(), UnsetMaximized());
  window_->Restore();
  // Reinitialize wl_array, which removes previous old states.
  auto active = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(xdg_surface_, 0, 0, ++serial, active.get());
  Sync();
}

TEST_P(WaylandWindowTest, Minimize) {
  ScopedWlArray states;

  // Make sure the window is initialized to normal state from the beginning.
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());
  SendConfigureEvent(xdg_surface_, 0, 0, 1, states.get());
  Sync();

  EXPECT_CALL(*GetXdgToplevel(), SetMinimized());
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(1);
  window_->Minimize();
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMinimized);

  // Reinitialize wl_array, which removes previous old states.
  states = ScopedWlArray();
  SendConfigureEvent(xdg_surface_, 0, 0, 2, states.get());
  Sync();

  // Wayland compositor doesn't notify clients about minimized state, but rather
  // if a window is not activated. Thus, a WaylandToplevelWindow marks itself as
  // being minimized and and sets state to minimized. Thus, the state mustn't
  // change after the configuration event is sent.
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMinimized);

  // Send one additional empty configuration event (which means the surface is
  // not maximized, fullscreen or activated) to ensure, WaylandWindow stays in
  // the same minimized state, but the delegate is always notified.
  //
  // TODO(tonikito): Improve filtering of delegate notification here.
  ui::PlatformWindowState state;
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _))
      .WillRepeatedly(DoAll(SaveArg<0>(&state), InvokeWithoutArgs([&]() {
                              EXPECT_EQ(state, PlatformWindowState::kMinimized);
                            })));
  SendConfigureEvent(xdg_surface_, 0, 0, 3, states.get());
  Sync();

  // And one last time to ensure the behaviour.
  SendConfigureEvent(xdg_surface_, 0, 0, 4, states.get());
  Sync();
}

TEST_P(WaylandWindowTest, SetFullscreenAndRestore) {
  // Make sure the window is initialized to normal state from the beginning.
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());

  ScopedWlArray states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(xdg_surface_, 0, 0, 1, states.get());
  Sync();

  AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN, states.get());

  EXPECT_CALL(*GetXdgToplevel(), SetFullscreen());
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(1);
  window_->ToggleFullscreen();
  // Make sure than WaylandWindow manually handles fullscreen states. Check the
  // comment in the WaylandWindow::ToggleFullscreen.
  EXPECT_EQ(window_->GetPlatformWindowState(),
            PlatformWindowState::kFullScreen);
  SendConfigureEvent(xdg_surface_, 0, 0, 2, states.get());
  Sync();

  EXPECT_CALL(*GetXdgToplevel(), UnsetFullscreen());
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(1);
  window_->Restore();
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kNormal);
  // Reinitialize wl_array, which removes previous old states.
  states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(xdg_surface_, 0, 0, 3, states.get());
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
                                      std::move(properties), true, true);

  Sync();

  // Make sure the window is initialized to normal state from the beginning.
  EXPECT_EQ(PlatformWindowState::kNormal, window->GetPlatformWindowState());

  // The state must not be changed to the fullscreen before the surface is
  // activated.
  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window->root_surface()->GetSurfaceId());
  EXPECT_FALSE(mock_surface->xdg_surface());
  EXPECT_CALL(delegate, OnWindowStateChanged(_, _)).Times(0);
  window->ToggleFullscreen();
  // The state of the window must already be fullscreen one.
  EXPECT_EQ(window->GetPlatformWindowState(), PlatformWindowState::kFullScreen);

  Sync();

  // We mustn't receive any state changes if that does not differ from the last
  // state.
  EXPECT_CALL(delegate, OnWindowStateChanged(_, _)).Times(0);

  // Activate the surface.
  ScopedWlArray states = InitializeWlArrayWithActivatedState();
  AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN, states.get());
  SendConfigureEvent(xdg_surface_, 0, 0, 1, states.get());

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
                                      std::move(properties), true, true);

  Sync();

  // Make sure the window is initialized to normal state from the beginning.
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());

  // The state gets changed to maximize and the delegate notified.
  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window->root_surface()->GetSurfaceId());
  EXPECT_FALSE(mock_surface->xdg_surface());
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(1);

  window_->Maximize();
  // The state of the window must already be fullscreen one.
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMaximized);

  Sync();

  // Window show state should be already up to date, so delegate is not
  // notified.
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(0);
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMaximized);

  // Activate the surface.
  ScopedWlArray states = InitializeWlArrayWithActivatedState();
  AddStateToWlArray(XDG_TOPLEVEL_STATE_MAXIMIZED, states.get());
  SendConfigureEvent(xdg_surface_, 0, 0, 1, states.get());

  Sync();

  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMaximized);
}

TEST_P(WaylandWindowTest, CompositorSideStateChanges) {
  // Real insets used by default on HiDPI.
  const auto kInsets = gfx::Insets{38, 44, 55, 44};
  const auto kNormalBounds = window_->GetBounds();

  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kNormal);

  // Set nonzero insets and ensure that they are only used when the window has
  // normal state.
  // See https://crbug.com/1274629
  window_->SetDecorationInsets(&kInsets);

  ScopedWlArray states = InitializeWlArrayWithActivatedState();
  AddStateToWlArray(XDG_TOPLEVEL_STATE_MAXIMIZED, states.get());
  SendConfigureEvent(xdg_surface_, 2000, 2000, 1, states.get());

  EXPECT_CALL(delegate_,
              OnWindowStateChanged(_, Eq(PlatformWindowState::kMaximized)))
      .Times(1);
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, 2000, 2000));

  Sync();

  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMaximized);

  // Unmaximize
  states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(xdg_surface_, 0, 0, 2, states.get());

  EXPECT_CALL(delegate_,
              OnWindowStateChanged(_, Eq(PlatformWindowState::kNormal)))
      .Times(1);
  EXPECT_CALL(*xdg_surface_,
              SetWindowGeometry(
                  kInsets.left(), kInsets.top(),
                  kNormalBounds.width() - (kInsets.left() + kInsets.right()),
                  kNormalBounds.height() - (kInsets.top() + kInsets.bottom())));

  Sync();

  // Now, set to fullscreen.
  AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN, states.get());
  SendConfigureEvent(xdg_surface_, 2005, 2005, 3, states.get());
  EXPECT_CALL(delegate_,
              OnWindowStateChanged(_, Eq(PlatformWindowState::kFullScreen)))
      .Times(1);
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, 2005, 2005));

  Sync();

  // Unfullscreen
  states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(xdg_surface_, 0, 0, 4, states.get());

  EXPECT_CALL(delegate_,
              OnWindowStateChanged(_, Eq(PlatformWindowState::kNormal)))
      .Times(1);
  EXPECT_CALL(*xdg_surface_,
              SetWindowGeometry(
                  kInsets.left(), kInsets.top(),
                  kNormalBounds.width() - (kInsets.left() + kInsets.right()),
                  kNormalBounds.height() - (kInsets.top() + kInsets.bottom())));

  Sync();

  // Now, maximize, fullscreen and restore.
  states = InitializeWlArrayWithActivatedState();
  AddStateToWlArray(XDG_TOPLEVEL_STATE_MAXIMIZED, states.get());
  SendConfigureEvent(xdg_surface_, 2000, 2000, 1, states.get());

  EXPECT_CALL(delegate_,
              OnWindowStateChanged(_, Eq(PlatformWindowState::kMaximized)))
      .Times(1);
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, 2000, 2000));

  Sync();

  AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN, states.get());
  SendConfigureEvent(xdg_surface_, 2005, 2005, 1, states.get());

  EXPECT_CALL(delegate_,
              OnWindowStateChanged(_, Eq(PlatformWindowState::kFullScreen)))
      .Times(1);
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, 2005, 2005));

  // Restore
  states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(xdg_surface_, 0, 0, 4, states.get());

  EXPECT_CALL(delegate_,
              OnWindowStateChanged(_, Eq(PlatformWindowState::kNormal)))
      .Times(1);
  EXPECT_CALL(*xdg_surface_,
              SetWindowGeometry(
                  kInsets.left(), kInsets.top(),
                  kNormalBounds.width() - (kInsets.left() + kInsets.right()),
                  kNormalBounds.height() - (kInsets.top() + kInsets.bottom())));

  Sync();
}

TEST_P(WaylandWindowTest, SetMaximizedFullscreenAndRestore) {
  const auto kNormalBounds = gfx::Rect{0, 0, 500, 300};
  const auto kMaximizedBounds = gfx::Rect{0, 0, 800, 600};

  uint32_t serial = 0;

  // Make sure the window has normal state initially.
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kNormalBounds)));
  window_->SetBounds(kNormalBounds);
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());
  VerifyAndClearExpectations();

  // Deactivate the surface.
  ScopedWlArray empty_state;
  SendConfigureEvent(xdg_surface_, 0, 0, ++serial, empty_state.get());
  Sync();

  auto active_maximized = MakeStateArray(
      {XDG_TOPLEVEL_STATE_ACTIVATED, XDG_TOPLEVEL_STATE_MAXIMIZED});
  EXPECT_CALL(*GetXdgToplevel(), SetMaximized());
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kMaximizedBounds.width(),
                                               kMaximizedBounds.height()));
  EXPECT_CALL(delegate_, OnActivationChanged(Eq(true)));
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kMaximizedBounds)));
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(1);
  window_->Maximize();
  // State changes are synchronous.
  EXPECT_EQ(PlatformWindowState::kMaximized, window_->GetPlatformWindowState());
  SendConfigureEvent(xdg_surface_, kMaximizedBounds.width(),
                     kMaximizedBounds.height(), ++serial,
                     active_maximized.get());
  Sync();
  // Verify that the state has not been changed.
  EXPECT_EQ(PlatformWindowState::kMaximized, window_->GetPlatformWindowState());
  VerifyAndClearExpectations();

  EXPECT_CALL(*GetXdgToplevel(), SetFullscreen());
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kMaximizedBounds.width(),
                                               kMaximizedBounds.height()));
  EXPECT_CALL(delegate_, OnBoundsChanged(_)).Times(0);
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(1);
  window_->ToggleFullscreen();
  // State changes are synchronous.
  EXPECT_EQ(PlatformWindowState::kFullScreen,
            window_->GetPlatformWindowState());
  AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN, active_maximized.get());
  SendConfigureEvent(xdg_surface_, kMaximizedBounds.width(),
                     kMaximizedBounds.height(), ++serial,
                     active_maximized.get());
  Sync();
  // Verify that the state has not been changed.
  EXPECT_EQ(PlatformWindowState::kFullScreen,
            window_->GetPlatformWindowState());
  VerifyAndClearExpectations();

  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, kNormalBounds.width(),
                                               kNormalBounds.height()));
  EXPECT_CALL(*GetXdgToplevel(), UnsetFullscreen());
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(kNormalBounds)));
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(1);
  window_->Restore();
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());
  // Reinitialize wl_array, which removes previous old states.
  auto active = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(xdg_surface_, 0, 0, ++serial, active.get());
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
  SendConfigureEvent(xdg_surface_, maximized_bounds.width(),
                     maximized_bounds.height(), 1, states.get());
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
  SendConfigureEvent(xdg_surface_, 0, 0, 2, states.get());
  Sync();
  bounds = window_->GetBounds();
  EXPECT_EQ(bounds, restored_bounds);
  restored_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_EQ(restored_bounds, gfx::Rect());
}

TEST_P(WaylandWindowTest, RestoreBoundsAfterFullscreen) {
  const gfx::Rect current_bounds = window_->GetBounds();

  ScopedWlArray states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(xdg_surface_, 0, 0, 1, states.get());
  Sync();

  gfx::Rect restored_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_EQ(restored_bounds, gfx::Rect());
  gfx::Rect bounds = window_->GetBounds();

  const gfx::Rect fullscreen_bounds = gfx::Rect(0, 0, 1280, 720);
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(fullscreen_bounds)));
  window_->ToggleFullscreen();
  AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN, states.get());
  SendConfigureEvent(xdg_surface_, fullscreen_bounds.width(),
                     fullscreen_bounds.height(), 2, states.get());
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
  SendConfigureEvent(xdg_surface_, 0, 0, 3, states.get());
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
  SendConfigureEvent(xdg_surface_, maximized_bounds.width(),
                     maximized_bounds.height(), 1, states.get());
  Sync();
  restored_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_EQ(bounds, restored_bounds);

  const gfx::Rect fullscreen_bounds = gfx::Rect(0, 0, 1280, 720);
  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(fullscreen_bounds)));
  window_->ToggleFullscreen();
  AddStateToWlArray(XDG_TOPLEVEL_STATE_FULLSCREEN, states.get());
  SendConfigureEvent(xdg_surface_, fullscreen_bounds.width(),
                     fullscreen_bounds.height(), 2, states.get());
  Sync();
  gfx::Rect fullscreen_restore_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_EQ(restored_bounds, fullscreen_restore_bounds);

  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(maximized_bounds)));
  window_->Maximize();
  // Reinitialize wl_array, which removes previous old states.
  states = InitializeWlArrayWithActivatedState();
  AddStateToWlArray(XDG_TOPLEVEL_STATE_MAXIMIZED, states.get());
  SendConfigureEvent(xdg_surface_, maximized_bounds.width(),
                     maximized_bounds.height(), 3, states.get());
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
  SendConfigureEvent(xdg_surface_, 0, 0, 4, states.get());
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
  SendConfigureEvent(xdg_surface_, 0, 0, 2, states.get());
  Sync();

  // Restored bounds should keep empty value.
  gfx::Rect restored_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_EQ(restored_bounds, gfx::Rect());

  // Second case is when Wayland sends a configure event with 1, 1 height and
  // width. It looks more like a bug in Gnome Shell with Wayland as long as the
  // documentation says it must be set to 0, 0, when wayland requests bounds.
  SendConfigureEvent(xdg_surface_, 0, 0, 3, states.get());
  Sync();

  // Restored bounds should keep empty value.
  restored_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_EQ(restored_bounds, gfx::Rect());
}

TEST_P(WaylandWindowTest, UpdateWindowRegion) {
  wl::MockSurface* mock_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());

  // Change bounds.
  const gfx::Rect initial_bounds = window_->GetBounds();
  const gfx::Rect new_bounds = gfx::Rect(0, 0, initial_bounds.width() + 10,
                                         initial_bounds.height() + 10);
  EXPECT_CALL(*mock_surface, SetOpaqueRegion(_)).Times(1);
  EXPECT_CALL(*mock_surface, SetInputRegion(_)).Times(1);
  window_->SetBounds(new_bounds);
  Sync();
  VerifyAndClearExpectations();
  EXPECT_EQ(mock_surface->opaque_region(), new_bounds);
  EXPECT_EQ(mock_surface->input_region(), new_bounds);

  // Maximize.
  ScopedWlArray states = InitializeWlArrayWithActivatedState();
  EXPECT_CALL(*mock_surface, SetOpaqueRegion(_)).Times(1);
  EXPECT_CALL(*mock_surface, SetInputRegion(_)).Times(1);
  const gfx::Rect maximized_bounds = gfx::Rect(0, 0, 1024, 768);
  window_->Maximize();
  AddStateToWlArray(XDG_TOPLEVEL_STATE_MAXIMIZED, states.get());
  SendConfigureEvent(xdg_surface_, maximized_bounds.width(),
                     maximized_bounds.height(), 1, states.get());
  Sync();
  VerifyAndClearExpectations();
  EXPECT_EQ(mock_surface->opaque_region(), maximized_bounds);
  EXPECT_EQ(mock_surface->input_region(), maximized_bounds);

  // Restore.
  const gfx::Rect restored_bounds = window_->GetRestoredBoundsInPixels();
  EXPECT_CALL(*mock_surface, SetOpaqueRegion(_)).Times(1);
  EXPECT_CALL(*mock_surface, SetInputRegion(_)).Times(1);
  window_->Restore();
  // Reinitialize wl_array, which removes previous old states.
  auto active = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(xdg_surface_, 0, 0, 2, active.get());
  Sync();
  VerifyAndClearExpectations();

  EXPECT_EQ(mock_surface->opaque_region(), restored_bounds);
  EXPECT_EQ(mock_surface->input_region(), restored_bounds);
}

TEST_P(WaylandWindowTest, CanDispatchMouseEventDefault) {
  EXPECT_FALSE(window_->CanDispatchEvent(&test_mouse_event_));
}

TEST_P(WaylandWindowTest, CanDispatchMouseEventFocus) {
  // SetPointerFocus(true) requires a WaylandPointer.
  wl_seat_send_capabilities(server_.seat()->resource(),
                            WL_SEAT_CAPABILITY_POINTER);
  Sync();
  ASSERT_TRUE(connection_->seat()->pointer());
  window_->SetPointerFocus(true);
  EXPECT_TRUE(window_->CanDispatchEvent(&test_mouse_event_));
}

TEST_P(WaylandWindowTest, CanDispatchMouseEventUnfocus) {
  EXPECT_FALSE(window_->has_pointer_focus());
  EXPECT_FALSE(window_->CanDispatchEvent(&test_mouse_event_));
}

TEST_P(WaylandWindowTest, SetCursorUsesZcrCursorShapesForCommonTypes) {
  MockZcrCursorShapes* mock_cursor_shapes = InstallMockZcrCursorShapes();

  // Verify some commonly-used cursors.
  EXPECT_CALL(*mock_cursor_shapes,
              SetCursorShape(ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_POINTER));
  auto pointer_cursor = base::MakeRefCounted<BitmapCursor>(
      mojom::CursorType::kPointer, kDefaultCursorScale);
  window_->SetCursor(pointer_cursor.get());

  EXPECT_CALL(*mock_cursor_shapes,
              SetCursorShape(ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_HAND));
  auto hand_cursor = base::MakeRefCounted<BitmapCursor>(
      mojom::CursorType::kHand, kDefaultCursorScale);
  window_->SetCursor(hand_cursor.get());

  EXPECT_CALL(*mock_cursor_shapes,
              SetCursorShape(ZCR_CURSOR_SHAPES_V1_CURSOR_SHAPE_TYPE_IBEAM));
  auto ibeam_cursor = base::MakeRefCounted<BitmapCursor>(
      mojom::CursorType::kIBeam, kDefaultCursorScale);
  window_->SetCursor(ibeam_cursor.get());
}

TEST_P(WaylandWindowTest, SetCursorCallsZcrCursorShapesOncePerCursor) {
  MockZcrCursorShapes* mock_cursor_shapes = InstallMockZcrCursorShapes();
  auto hand_cursor = base::MakeRefCounted<BitmapCursor>(
      mojom::CursorType::kHand, kDefaultCursorScale);
  // Setting the same cursor twice on the client only calls the server once.
  EXPECT_CALL(*mock_cursor_shapes, SetCursorShape(_)).Times(1);
  window_->SetCursor(hand_cursor.get());
  window_->SetCursor(hand_cursor.get());
}

TEST_P(WaylandWindowTest, SetCursorDoesNotUseZcrCursorShapesForNoneCursor) {
  MockZcrCursorShapes* mock_cursor_shapes = InstallMockZcrCursorShapes();
  EXPECT_CALL(*mock_cursor_shapes, SetCursorShape(_)).Times(0);
  auto none_cursor = base::MakeRefCounted<BitmapCursor>(
      mojom::CursorType::kNone, kDefaultCursorScale);
  window_->SetCursor(none_cursor.get());
}

TEST_P(WaylandWindowTest, SetCursorDoesNotUseZcrCursorShapesForCustomCursors) {
  MockZcrCursorShapes* mock_cursor_shapes = InstallMockZcrCursorShapes();

  // Custom cursors require bitmaps, so they do not use server-side cursors.
  EXPECT_CALL(*mock_cursor_shapes, SetCursorShape(_)).Times(0);
  auto custom_cursor =
      base::MakeRefCounted<BitmapCursor>(mojom::CursorType::kCustom, SkBitmap(),
                                         gfx::Point(), kDefaultCursorScale);
  window_->SetCursor(custom_cursor.get());
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
  SendConfigureEvent(xdg_surface_, 1000, 1000, 12, states.get());

  Sync();

  EXPECT_CALL(delegate_, OnBoundsChanged(Eq(gfx::Rect(0, 0, 1500, 1000))));
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, 1500, 1000)).Times(1);
  EXPECT_CALL(*xdg_surface_, AckConfigure(13));
  SendConfigureEvent(xdg_surface_, 1500, 1000, 13, states.get());
}

TEST_P(WaylandWindowTest, ConfigureEventWithNulledSize) {
  ScopedWlArray states;

  // If Wayland sends configure event with 0 width and 0 size, client should
  // call back with desired sizes. In this case, that's the actual size of
  // the window.
  SendConfigureEvent(xdg_surface_, 0, 0, 14, states.get());
  // |xdg_surface_| must receive the following calls in both xdg_shell_v5 and
  // xdg_shell_v6. Other calls like SetTitle or SetMaximized are recieved by
  // xdg_toplevel in xdg_shell_v6 and by xdg_surface_ in xdg_shell_v5.
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, 800, 600));
  EXPECT_CALL(*xdg_surface_, AckConfigure(14));
}

TEST_P(WaylandWindowTest, OnActivationChanged) {
  uint32_t serial = 0;

  // Deactivate the surface.
  ScopedWlArray empty_state;
  SendConfigureEvent(xdg_surface_, 0, 0, ++serial, empty_state.get());
  Sync();

  {
    ScopedWlArray states = InitializeWlArrayWithActivatedState();
    EXPECT_CALL(delegate_, OnActivationChanged(Eq(true)));
    SendConfigureEvent(xdg_surface_, 0, 0, ++serial, states.get());
    Sync();
  }

  ScopedWlArray states;
  EXPECT_CALL(delegate_, OnActivationChanged(Eq(false)));
  SendConfigureEvent(xdg_surface_, 0, 0, ++serial, states.get());
  Sync();
}

TEST_P(WaylandWindowTest, OnAcceleratedWidgetDestroy) {
  window_.reset();
}

TEST_P(WaylandWindowTest, CanCreateMenuWindow) {
  MockPlatformWindowDelegate menu_window_delegate;
  EXPECT_CALL(menu_window_delegate, GetMenuType())
      .WillRepeatedly(Return(MenuType::kRootContextMenu));

  // SetPointerFocus(true) requires a WaylandPointer.
  wl_seat_send_capabilities(
      server_.seat()->resource(),
      WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_TOUCH);
  Sync();
  ASSERT_TRUE(connection_->seat()->pointer() && connection_->seat()->touch());
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
  EXPECT_CALL(menu_window_delegate, GetMenuType())
      .WillRepeatedly(Return(MenuType::kRootContextMenu));

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
  EXPECT_CALL(menu_window_delegate, GetMenuType())
      .WillOnce(Return(MenuType::kRootContextMenu));
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, widget_, gfx::Rect(10, 10, 10, 10),
      &menu_window_delegate);
  EXPECT_TRUE(menu_window);

  wl_seat_send_capabilities(server_.seat()->resource(),
                            WL_SEAT_CAPABILITY_POINTER);
  Sync();
  ASSERT_TRUE(connection_->seat()->pointer());
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
  EXPECT_CALL(nested_menu_window_delegate, GetMenuType())
      .WillOnce(Return(MenuType::kRootContextMenu));
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
  EXPECT_CALL(menu_window_delegate, GetMenuType())
      .WillOnce(Return(MenuType::kRootContextMenu));
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
  ASSERT_TRUE(connection_->seat()->pointer());
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
  EXPECT_CALL(menu_window_delegate, GetMenuType())
      .WillOnce(Return(MenuType::kRootContextMenu));
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, widget_, gfx::Rect(10, 10, 10, 10),
      &menu_window_delegate);
  EXPECT_TRUE(menu_window);

  wl_seat_send_capabilities(server_.seat()->resource(),
                            WL_SEAT_CAPABILITY_KEYBOARD);
  Sync();
  ASSERT_TRUE(connection_->seat()->keyboard());
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
  EXPECT_CALL(menu_window_delegate, GetMenuType())
      .WillOnce(Return(MenuType::kRootContextMenu));

  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, widget_, gfx::Rect(0, 0, 10, 10),
      &menu_window_delegate);
  EXPECT_TRUE(menu_window);

  Sync();

  MockPlatformWindowDelegate nested_menu_window_delegate;
  EXPECT_CALL(nested_menu_window_delegate, GetMenuType())
      .WillOnce(Return(MenuType::kRootContextMenu));
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
  ASSERT_TRUE(connection_->seat()->pointer());
  ASSERT_TRUE(connection_->seat()->touch());
  ASSERT_TRUE(connection_->seat()->keyboard());

  uint32_t serial = 0;

  // Test that CanDispatchEvent is set correctly.
  wl::MockSurface* toplevel_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());
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

  wl::MockSurface* menu_window_surface = server_.GetObject<wl::MockSurface>(
      menu_window->root_surface()->GetSurfaceId());

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
      server_.GetObject<wl::MockSurface>(
          nested_menu_window->root_surface()->GetSurfaceId());

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
  wl_seat_send_capabilities(server_.seat()->resource(),
                            WL_SEAT_CAPABILITY_POINTER);
  Sync();
  ASSERT_TRUE(connection_->seat()->pointer());

  // Focus and press left mouse button, so that serial is sent to client.
  auto* pointer_resource = server_.seat()->pointer()->resource();
  wl_pointer_send_enter(pointer_resource, 1, surface_->resource(), 0, 0);
  wl_pointer_send_button(pointer_resource, 2, 1, BTN_LEFT,
                         WL_POINTER_BUTTON_STATE_PRESSED);
  Sync();

  EXPECT_CALL(*GetXdgToplevel(), Move(_));
  ui::GetWmMoveResizeHandler(*window_)->DispatchHostWindowDragMovement(
      HTCAPTION, gfx::Point());
}

// Makes sure hit tests are converted into right edges.
TEST_P(WaylandWindowTest, DispatchWindowResize) {
  std::vector<int> hit_test_values;
  InitializeWithSupportedHitTestValues(&hit_test_values);

  // Focus and press left mouse button, so that serial is sent to client.
  wl_seat_send_capabilities(server_.seat()->resource(),
                            WL_SEAT_CAPABILITY_POINTER);
  Sync();
  ASSERT_TRUE(connection_->seat()->pointer());

  auto* pointer_resource = server_.seat()->pointer()->resource();
  wl_pointer_send_enter(pointer_resource, 1, surface_->resource(), 0, 0);
  wl_pointer_send_button(pointer_resource, 2, 1, BTN_LEFT,
                         WL_POINTER_BUTTON_STATE_PRESSED);
  Sync();

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

TEST_P(WaylandWindowTest, ToplevelWindowUpdateWindowScale) {
  VerifyAndClearExpectations();

  // Surface scale must be 1 when no output has been entered by the window.
  EXPECT_EQ(1, window_->window_scale());

  // Creating an output with scale 1.
  wl::TestOutput* output1 = server_.CreateAndInitializeOutput();
  output1->SetRect(gfx::Rect(0, 0, 1920, 1080));
  output1->SetScale(1);
  Sync();

  // Creating an output with scale 2.
  wl::TestOutput* output2 = server_.CreateAndInitializeOutput();
  output2->SetRect(gfx::Rect(0, 0, 1920, 1080));
  output2->SetScale(2);
  Sync();

  // Send the window to |output1|.
  wl::MockSurface* surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());
  ASSERT_TRUE(surface);
  wl_surface_send_enter(surface->resource(), output1->resource());
  Sync();

  // The window's scale and bounds must remain unchanged.
  EXPECT_EQ(1, window_->window_scale());
  EXPECT_EQ(gfx::Rect(0, 0, 800, 600), window_->GetBounds());

  // Simulating drag process from |output1| to |output2|.
  wl_surface_send_enter(surface->resource(), output2->resource());
  wl_surface_send_leave(surface->resource(), output1->resource());
  Sync();

  // The window must change its scale and bounds to keep DIP bounds the same.
  EXPECT_EQ(2, window_->window_scale());
  EXPECT_EQ(gfx::Rect(0, 0, 1600, 1200), window_->GetBounds());
}

TEST_P(WaylandWindowTest, WaylandPopupSurfaceScale) {
  VerifyAndClearExpectations();

  // Creating an output with scale 1.
  wl::TestOutput* output1 = server_.CreateAndInitializeOutput();
  output1->SetRect(gfx::Rect(0, 0, 1920, 1080));
  output1->SetScale(1);
  Sync();

  // Creating an output with scale 2.
  wl::TestOutput* output2 = server_.CreateAndInitializeOutput();
  output2->SetRect(gfx::Rect(1920, 0, 1920, 1080));
  output2->SetScale(2);
  Sync();

  std::vector<PlatformWindowType> window_types{PlatformWindowType::kMenu,
                                               PlatformWindowType::kTooltip};
  for (const auto& type : window_types) {
    // Send the window to |output1|.
    wl::MockSurface* surface = server_.GetObject<wl::MockSurface>(
        window_->root_surface()->GetSurfaceId());
    ASSERT_TRUE(surface);
    wl_surface_send_enter(surface->resource(), output1->resource());
    Sync();

    // Creating a wayland_popup on |window_|.
    window_->SetPointerFocus(true);
    gfx::Rect wayland_popup_bounds(15, 15, 10, 10);
    auto wayland_popup = CreateWaylandWindowWithParams(
        type, window_->GetWidget(), wayland_popup_bounds, &delegate_);
    EXPECT_TRUE(wayland_popup);
    wayland_popup->Show(false);

    // the wayland_popup window should inherit its buffer scale from the focused
    // window.
    EXPECT_EQ(1, window_->window_scale());
    EXPECT_EQ(window_->window_scale(), wayland_popup->window_scale());
    EXPECT_EQ(wayland_popup_bounds, wayland_popup->GetBounds());
    wayland_popup->Hide();

    // Send the window to |output2|.
    wl_surface_send_enter(surface->resource(), output2->resource());
    wl_surface_send_leave(surface->resource(), output1->resource());
    Sync();

    EXPECT_EQ(2, window_->window_scale());
    wayland_popup->Show(false);

    Sync();

    // |wayland_popup|'s scale and bounds must change whenever its parents
    // scale is changed.
    EXPECT_EQ(window_->window_scale(), wayland_popup->window_scale());
    EXPECT_EQ(gfx::ScaleToRoundedRect(wayland_popup_bounds,
                                      wayland_popup->window_scale()),
              wayland_popup->GetBounds());

    wayland_popup->Hide();
    window_->SetPointerFocus(false);

    wl_surface_send_leave(surface->resource(), output2->resource());
    Sync();
  }
}

// Tests that WaylandPopup is able to translate provided bounds via
// PlatformWindowProperties using buffer scale it's going to use that the client
// is not able to determine before PlatformWindow is created. See
// WaylandPopup::OnInitialize for more details.
TEST_P(WaylandWindowTest, WaylandPopupInitialBufferScale) {
  VerifyAndClearExpectations();

  wl::TestOutput* main_output = server_.CreateAndInitializeOutput();
  main_output->SetRect(gfx::Rect(0, 0, 1920, 1080));
  main_output->SetScale(1);
  Sync();

  wl::TestOutput* secondary_output = server_.CreateAndInitializeOutput();
  secondary_output->SetRect(gfx::Rect(1921, 0, 1920, 1080));
  secondary_output->SetScale(1);
  Sync();

  wl::MockSurface* surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());
  ASSERT_TRUE(surface);

  struct {
    wl::TestOutput* output;
    const char* label;
  } screen[] = {{main_output, "main output"},
                {secondary_output, "secondary output"}};

  for (const auto& entered_output : screen) {
    wl_surface_send_enter(surface->resource(),
                          entered_output.output->resource());
    Sync();
    for (auto main_output_scale = 1; main_output_scale < 5;
         main_output_scale++) {
      for (auto secondary_output_scale = 1; secondary_output_scale < 5;
           secondary_output_scale++) {
        // Update scale factors first.
        main_output->SetScale(main_output_scale);
        secondary_output->SetScale(secondary_output_scale);

        main_output->Flush();
        secondary_output->Flush();
        Sync();

        gfx::Rect bounds_dip(15, 15, 10, 10);
        // DesktopWindowTreeHostPlatform uses the scale of the current display
        // of the parent window to translate initial bounds of the popup to
        // pixels.
        const auto effective_scale = entered_output.output->GetScale();
        gfx::Transform transform;
        transform.Scale(effective_scale, effective_scale);
        gfx::RectF rect_in_pixels = gfx::RectF(bounds_dip);
        transform.TransformRect(&rect_in_pixels);
        gfx::Rect wayland_popup_bounds = gfx::ToEnclosingRect(rect_in_pixels);

        std::unique_ptr<WaylandWindow> wayland_popup =
            CreateWaylandWindowWithParams(PlatformWindowType::kMenu,
                                          window_->GetWidget(),
                                          wayland_popup_bounds, &delegate_);
        EXPECT_TRUE(wayland_popup);

        wayland_popup->Show(false);

        gfx::Rect expected_bounds = wayland_popup_bounds;
        if (entered_output.output == secondary_output) {
          expected_bounds =
              gfx::ScaleToRoundedRect(bounds_dip, secondary_output_scale);
        }

        EXPECT_EQ(expected_bounds, wayland_popup->GetBounds())
            << " when the window is on " << entered_output.label
            << " that has scale " << entered_output.output->GetScale();
      }
    }
    wl_surface_send_leave(surface->resource(),
                          entered_output.output->resource());
    Sync();
  }
}

// Verifies that when the forced scale factor is set, bounds are always the same
// regardless of the scale of the display.
TEST_P(WaylandWindowTest, WaylandPopupInitialBufferScaleForcedScale) {
  VerifyAndClearExpectations();

  const auto forced_scale = 1.2;

  display::Display::SetForceDeviceScaleFactor(forced_scale);

  // Creating an output with scale 1.
  wl::TestOutput* main_output = server_.CreateAndInitializeOutput();
  main_output->SetRect(gfx::Rect(0, 0, 1920, 1080));
  main_output->SetScale(1);
  Sync();

  // Creating an output with scale 2.
  wl::TestOutput* secondary_output = server_.CreateAndInitializeOutput();
  secondary_output->SetRect(gfx::Rect(1921, 0, 1920, 1080));
  secondary_output->SetScale(2);
  Sync();

  // Send the window to |output1|.
  wl::MockSurface* surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());
  ASSERT_TRUE(surface);

  wl_surface_send_enter(surface->resource(), secondary_output->resource());
  Sync();

  const gfx::Rect bounds_dip{50, 50, 100, 100};
  const gfx::Rect expected_bounds_px =
      gfx::ScaleToRoundedRect(bounds_dip, forced_scale);

  // DesktopWindowTreeHostPlatform has always to use a primary display's
  // scale to translate initial bounds to pixels. However, the primary display
  // will use forced device scale factor and ignore wl_output's scale. Thus, use
  // primary output's scale to make initial bounds. This code snippet is a copy
  // of DesktopWindowTreeHostPlatform::ToPixelRect.
  gfx::Transform transform;
  transform.Scale(display::Display::GetForcedDeviceScaleFactor(),
                  display::Display::GetForcedDeviceScaleFactor());
  gfx::RectF rect_in_pixels = gfx::RectF(bounds_dip);
  transform.TransformRect(&rect_in_pixels);
  gfx::Rect wayland_popup_bounds = gfx::ToEnclosingRect(rect_in_pixels);

  std::unique_ptr<WaylandWindow> wayland_popup = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, window_->GetWidget(), wayland_popup_bounds,
      &delegate_);
  EXPECT_TRUE(wayland_popup);

  wayland_popup->Show(false);

  EXPECT_EQ(expected_bounds_px, wayland_popup->GetBounds());

  wl_surface_send_leave(surface->resource(), secondary_output->resource());
  Sync();
}

// Tests that a WaylandWindow uses the entered output with largest scale
// factor as the preferred output. If scale factors are equal, the very first
// entered display is used.
TEST_P(WaylandWindowTest, GetPreferredOutput) {
  VerifyAndClearExpectations();

  // Buffer scale must be 1 when no output has been entered by the window.
  EXPECT_EQ(1, window_->window_scale());

  // Creating an output with scale 1.
  wl::TestOutput* output1 = server_.CreateAndInitializeOutput();
  output1->SetRect(gfx::Rect(0, 0, 1920, 1080));
  Sync();

  // Creating an output with scale 2.
  wl::TestOutput* output2 = server_.CreateAndInitializeOutput();
  output2->SetRect(gfx::Rect(1921, 0, 1920, 1080));
  Sync();

  auto entered_outputs = window_->root_surface()->entered_outputs();
  EXPECT_EQ(0u, entered_outputs.size());

  // Send the window to |output1|.
  wl::MockSurface* surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());
  ASSERT_TRUE(surface);
  wl_surface_send_enter(surface->resource(), output1->resource());
  wl_surface_send_enter(surface->resource(), output2->resource());
  Sync();

  // The window entered two outputs.
  entered_outputs = window_->root_surface()->entered_outputs();
  EXPECT_EQ(2u, entered_outputs.size());

  // The window must prefer the output that it entered first.
  uint32_t expected_entered_output_id = *entered_outputs.begin();
  EXPECT_EQ(expected_entered_output_id, window_->GetPreferredEnteredOutputId());

  // Create the third output and pretend the window entered 3 outputs at the
  // same time.
  wl::TestOutput* output3 = server_.CreateAndInitializeOutput();
  output3->SetRect(gfx::Rect(0, 1081, 1920, 1080));
  Sync();

  wl_surface_send_enter(surface->resource(), output3->resource());
  Sync();

  // The window entered three outputs...
  entered_outputs = window_->root_surface()->entered_outputs();
  EXPECT_EQ(3u, entered_outputs.size());

  // but it still must prefer the output that it entered first.
  EXPECT_EQ(expected_entered_output_id, window_->GetPreferredEnteredOutputId());

  // Pretend that the output2 has scale factor equals to 2 now.
  output2->SetScale(2);
  output2->Flush();
  Sync();

  entered_outputs = window_->root_surface()->entered_outputs();
  EXPECT_EQ(3u, entered_outputs.size());

  // It must be the second entered output now.
  expected_entered_output_id = *(++entered_outputs.begin());
  auto* expected_entered_output =
      connection_->wayland_output_manager()->GetOutput(
          expected_entered_output_id);
  EXPECT_EQ(2, expected_entered_output->scale_factor());

  // The window_ must return the output with largest scale.
  EXPECT_EQ(expected_entered_output_id, window_->GetPreferredEnteredOutputId());

  // Now, the output1 changes its scale factor to 2 as well.
  output1->SetScale(2);
  output1->Flush();
  Sync();

  // It must be the very first output now.
  entered_outputs = window_->root_surface()->entered_outputs();
  expected_entered_output_id = *entered_outputs.begin();
  EXPECT_EQ(expected_entered_output_id, window_->GetPreferredEnteredOutputId());

  // Now, the output1 changes its scale factor back to 1.
  output1->SetScale(1);
  output1->Flush();
  Sync();

  // It must be the very the second output now.
  entered_outputs = window_->root_surface()->entered_outputs();
  expected_entered_output_id = *(++entered_outputs.begin());
  EXPECT_EQ(expected_entered_output_id, window_->GetPreferredEnteredOutputId());

  // All outputs have scale factor of 1. window_ prefers the output that
  // it entered first again.
  output2->SetScale(1);
  output2->Flush();
  Sync();

  entered_outputs = window_->root_surface()->entered_outputs();
  expected_entered_output_id = *(entered_outputs.begin());
  EXPECT_EQ(expected_entered_output_id, window_->GetPreferredEnteredOutputId());
}

TEST_P(WaylandWindowTest, GetChildrenPreferredOutput) {
  VerifyAndClearExpectations();

  // Buffer scale must be 1 when no output has been entered by the window.
  EXPECT_EQ(1, window_->window_scale());

  MockPlatformWindowDelegate menu_window_delegate;
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, window_->GetWidget(),
      gfx::Rect(10, 10, 10, 10), &menu_window_delegate);

  menu_window->Show(false);

  Sync();

  // Creating an output with scale 1.
  wl::TestOutput* output1 = server_.CreateAndInitializeOutput();
  output1->SetRect(gfx::Rect(0, 0, 1920, 1080));
  Sync();

  // Creating an output with scale 2.
  wl::TestOutput* output2 = server_.CreateAndInitializeOutput();
  output2->SetRect(gfx::Rect(1921, 0, 1920, 1080));
  Sync();

  auto entered_outputs = window_->root_surface()->entered_outputs();
  EXPECT_EQ(0u, entered_outputs.size());

  auto menu_entered_outputs = menu_window->root_surface()->entered_outputs();
  EXPECT_EQ(0u, menu_entered_outputs.size());

  // Enter |output1|.
  wl::MockSurface* surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());
  ASSERT_TRUE(surface);
  wl_surface_send_enter(surface->resource(), output1->resource());
  Sync();

  // The window entered the output.
  entered_outputs = window_->root_surface()->entered_outputs();
  EXPECT_EQ(1u, entered_outputs.size());

  // The menu also thinks it entered the same output.
  menu_entered_outputs = menu_window->root_surface()->entered_outputs();
  EXPECT_EQ(0u, menu_entered_outputs.size());

  EXPECT_EQ(window_->GetPreferredEnteredOutputId(),
            menu_window->GetPreferredEnteredOutputId());

  // Pretend Wayland sends that menu entered output2, while the toplevel is on
  // output1.  Output1 must still be preferred by the menu.
  wl::MockSurface* menu_surface = server_.GetObject<wl::MockSurface>(
      menu_window->root_surface()->GetSurfaceId());
  wl_surface_send_enter(menu_surface->resource(), output1->resource());
  Sync();

  // The menu surface should be aware of the output that Wayland sent it.
  EXPECT_EQ(1u, window_->root_surface()->entered_outputs().size());
  EXPECT_EQ(1u, menu_window->root_surface()->entered_outputs().size());

  EXPECT_EQ(window_->GetPreferredEnteredOutputId(),
            menu_window->GetPreferredEnteredOutputId());

  // Pretend Wayland sends that toplevel entered output2.
  wl_surface_send_enter(surface->resource(), output1->resource());
  Sync();

  EXPECT_EQ(2u, window_->root_surface()->entered_outputs().size());
  EXPECT_EQ(1u, menu_window->root_surface()->entered_outputs().size());

  EXPECT_EQ(window_->GetPreferredEnteredOutputId(),
            menu_window->GetPreferredEnteredOutputId());

  // Now, the output2 changes its scale factor to 2.
  output2->SetScale(2);
  output2->Flush();
  Sync();

  // It must be the very the second output now.
  entered_outputs = window_->root_surface()->entered_outputs();
  uint32_t expected_entered_output_id = *(++entered_outputs.begin());
  EXPECT_EQ(expected_entered_output_id, window_->GetPreferredEnteredOutputId());

  EXPECT_EQ(window_->GetPreferredEnteredOutputId(),
            menu_window->GetPreferredEnteredOutputId());
}

// Tests that xdg_popup is configured with default anchor properties and bounds
// if delegate doesn't have anchor properties set.
TEST_P(WaylandWindowTest, PopupPassesDefaultAnchorInformation) {
  PopupPosition menu_window_positioner, nested_menu_window_positioner;

  if (GetParam().shell_version == wl::ShellVersion::kV6) {
    menu_window_positioner = {gfx::Rect(439, 46, 1, 1), gfx::Size(287, 409),
                              ZXDG_POSITIONER_V6_ANCHOR_TOP |
                                  ZXDG_POSITIONER_V6_ANCHOR_LEFT,
                              ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
                                  ZXDG_POSITIONER_V6_GRAVITY_RIGHT,
                              ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_Y},

    nested_menu_window_positioner = {
        gfx::Rect(285, 1, 1, 1), gfx::Size(305, 99),
        ZXDG_POSITIONER_V6_ANCHOR_TOP | ZXDG_POSITIONER_V6_ANCHOR_LEFT,
        ZXDG_POSITIONER_V6_GRAVITY_BOTTOM | ZXDG_POSITIONER_V6_GRAVITY_RIGHT,
        ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_Y};
  } else {
    menu_window_positioner = {gfx::Rect(439, 46, 1, 1), gfx::Size(287, 409),
                              XDG_POSITIONER_ANCHOR_TOP_LEFT,
                              XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT,
                              XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y};
    nested_menu_window_positioner = {
        gfx::Rect(285, 1, 1, 1), gfx::Size(305, 99),
        XDG_POSITIONER_ANCHOR_TOP_LEFT, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT,
        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y};
  }

  auto* toplevel_window = window_.get();
  toplevel_window->SetBounds(gfx::Rect(0, 0, 739, 574));

  // Case 1: properties are not provided. In this case, bounds' origin must
  // be used as anchor rect and anchor position, gravity and constraints should
  // be normal.
  MockPlatformWindowDelegate menu_window_delegate;
  EXPECT_CALL(menu_window_delegate, GetMenuType())
      .WillOnce(Return(MenuType::kRootMenu));
  EXPECT_CALL(menu_window_delegate, GetOwnedWindowAnchorAndRectInPx())
      .WillOnce(Return(absl::nullopt));
  gfx::Rect menu_window_bounds(gfx::Point(439, 46),
                               menu_window_positioner.size);
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, toplevel_window->GetWidget(),
      menu_window_bounds, &menu_window_delegate);
  EXPECT_TRUE(menu_window);

  Sync();

  VerifyXdgPopupPosition(menu_window.get(), menu_window_positioner);

  EXPECT_CALL(menu_window_delegate, OnBoundsChanged(_)).Times(0);
  SendConfigureEventPopup(menu_window.get(), menu_window_bounds);

  Sync();

  EXPECT_EQ(menu_window->GetBounds(), menu_window_bounds);

  // Case 2: the nested menu window is positioned normally.
  MockPlatformWindowDelegate nested_menu_window_delegate;
  EXPECT_CALL(nested_menu_window_delegate, GetMenuType())
      .WillRepeatedly(Return(MenuType::kChildMenu));
  gfx::Rect nested_menu_window_bounds(gfx::Point(724, 47),
                                      nested_menu_window_positioner.size);
  std::unique_ptr<WaylandWindow> nested_menu_window =
      CreateWaylandWindowWithParams(
          PlatformWindowType::kMenu, menu_window->GetWidget(),
          nested_menu_window_bounds, &nested_menu_window_delegate);
  EXPECT_TRUE(nested_menu_window);

  Sync();

  VerifyXdgPopupPosition(nested_menu_window.get(),
                         nested_menu_window_positioner);
}

// Tests that xdg_popup is configured with anchor properties received from
// delegate.
TEST_P(WaylandWindowTest, PopupPassesSetAnchorInformation) {
  PopupPosition menu_window_positioner, nested_menu_window_positioner;

  if (GetParam().shell_version == wl::ShellVersion::kV6) {
    menu_window_positioner = {gfx::Rect(468, 46, 28, 28), gfx::Size(320, 404),
                              ZXDG_POSITIONER_V6_ANCHOR_BOTTOM |
                                  ZXDG_POSITIONER_V6_ANCHOR_RIGHT,
                              ZXDG_POSITIONER_V6_GRAVITY_BOTTOM |
                                  ZXDG_POSITIONER_V6_GRAVITY_LEFT,
                              ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_Y},

    nested_menu_window_positioner = {
        gfx::Rect(4, 83, 312, 1), gfx::Size(480, 294),
        ZXDG_POSITIONER_V6_ANCHOR_TOP | ZXDG_POSITIONER_V6_ANCHOR_RIGHT,
        ZXDG_POSITIONER_V6_GRAVITY_BOTTOM | ZXDG_POSITIONER_V6_GRAVITY_RIGHT,
        ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_Y |
            ZXDG_POSITIONER_V6_CONSTRAINT_ADJUSTMENT_FLIP_X};
  } else {
    menu_window_positioner = {gfx::Rect(468, 46, 28, 28), gfx::Size(320, 404),
                              XDG_POSITIONER_ANCHOR_BOTTOM_RIGHT,
                              XDG_POSITIONER_GRAVITY_BOTTOM_LEFT,
                              XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y};
    nested_menu_window_positioner = {
        gfx::Rect(4, 83, 312, 1), gfx::Size(480, 294),
        XDG_POSITIONER_ANCHOR_TOP_RIGHT, XDG_POSITIONER_GRAVITY_BOTTOM_RIGHT,
        XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_Y |
            XDG_POSITIONER_CONSTRAINT_ADJUSTMENT_FLIP_X};
  }

  auto* toplevel_window = window_.get();
  toplevel_window->SetBounds(gfx::Rect(0, 0, 508, 212));

  MockPlatformWindowDelegate menu_window_delegate;
  EXPECT_CALL(menu_window_delegate, GetMenuType())
      .WillOnce(Return(MenuType::kRootMenu));
  ui::OwnedWindowAnchor anchor = {
      gfx::Rect(menu_window_positioner.anchor_rect),
      OwnedWindowAnchorPosition::kBottomRight,
      OwnedWindowAnchorGravity::kBottomLeft,
      OwnedWindowConstraintAdjustment::kAdjustmentFlipY};
  EXPECT_CALL(menu_window_delegate, GetOwnedWindowAnchorAndRectInPx())
      .WillOnce(Return(anchor));
  gfx::Rect menu_window_bounds(gfx::Point(176, 74),
                               menu_window_positioner.size);
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, toplevel_window->GetWidget(),
      menu_window_bounds, &menu_window_delegate);
  EXPECT_TRUE(menu_window);

  Sync();

  VerifyXdgPopupPosition(menu_window.get(), menu_window_positioner);

  Sync();

  MockPlatformWindowDelegate nested_menu_window_delegate;
  EXPECT_CALL(nested_menu_window_delegate, GetMenuType())
      .WillRepeatedly(Return(MenuType::kChildMenu));
  anchor = {{180, 157, 312, 1},
            OwnedWindowAnchorPosition::kTopRight,
            OwnedWindowAnchorGravity::kBottomRight,
            OwnedWindowConstraintAdjustment::kAdjustmentFlipY |
                OwnedWindowConstraintAdjustment::kAdjustmentFlipX};
  EXPECT_CALL(nested_menu_window_delegate, GetOwnedWindowAnchorAndRectInPx())
      .WillOnce(Return(anchor));
  gfx::Rect nested_menu_window_bounds(gfx::Point(492, 157),
                                      nested_menu_window_positioner.size);
  std::unique_ptr<WaylandWindow> nested_menu_window =
      CreateWaylandWindowWithParams(
          PlatformWindowType::kMenu, menu_window->GetWidget(),
          nested_menu_window_bounds, &nested_menu_window_delegate);
  EXPECT_TRUE(nested_menu_window);

  Sync();

  VerifyXdgPopupPosition(nested_menu_window.get(),
                         nested_menu_window_positioner);
}

ACTION_P(VerifyRegion, ptr) {
  wl::TestRegion* region = wl::GetUserDataAs<wl::TestRegion>(arg0);
  EXPECT_EQ(*ptr, region->getBounds());
}

TEST_P(WaylandWindowTest, SetOpaqueRegion) {
  wl::MockSurface* mock_surface = server_.GetObject<wl::MockSurface>(
      window_->root_surface()->GetSurfaceId());

  gfx::Rect new_bounds(0, 0, 500, 600);
  auto state_array = MakeStateArray({XDG_TOPLEVEL_STATE_ACTIVATED});
  SendConfigureEvent(xdg_surface_, new_bounds.width(), new_bounds.height(), 1,
                     state_array.get());

  SkIRect rect =
      SkIRect::MakeXYWH(0, 0, new_bounds.width(), new_bounds.height());
  EXPECT_CALL(*mock_surface, SetOpaqueRegion(_)).WillOnce(VerifyRegion(&rect));

  Sync();

  VerifyAndClearExpectations();
  EXPECT_EQ(mock_surface->opaque_region(), new_bounds);

  new_bounds.set_size(gfx::Size(1000, 534));
  SendConfigureEvent(xdg_surface_, new_bounds.width(), new_bounds.height(), 2,
                     state_array.get());

  rect = SkIRect::MakeXYWH(0, 0, new_bounds.width(), new_bounds.height());
  EXPECT_CALL(*mock_surface, SetOpaqueRegion(_)).WillOnce(VerifyRegion(&rect));

  Sync();

  VerifyAndClearExpectations();
  EXPECT_EQ(mock_surface->opaque_region(), new_bounds);
}

TEST_P(WaylandWindowTest, OnCloseRequest) {
  EXPECT_CALL(delegate_, OnCloseRequest());

  if (GetParam().shell_version == wl::ShellVersion::kV6)
    zxdg_toplevel_v6_send_close(xdg_surface_->xdg_toplevel()->resource());
  else
    xdg_toplevel_send_close(xdg_surface_->xdg_toplevel()->resource());

  Sync();
}

TEST_P(WaylandWindowTest, WaylandPopupSimpleParent) {
  VerifyAndClearExpectations();

  EXPECT_CALL(delegate_, GetMenuType())
      .WillRepeatedly(Return(MenuType::kRootContextMenu));
  // WaylandPopup must ignore the parent provided by aura and should always
  // use focused window instead.
  gfx::Rect wayland_popup_bounds(gfx::Point(15, 15), gfx::Size(10, 10));
  std::unique_ptr<WaylandWindow> wayland_popup = CreateWaylandWindowWithParams(
      PlatformWindowType::kTooltip, window_->GetWidget(), wayland_popup_bounds,
      &delegate_);
  EXPECT_TRUE(wayland_popup);

  wayland_popup->Show(false);

  Sync();

  auto* mock_surface_popup = server_.GetObject<wl::MockSurface>(
      wayland_popup->root_surface()->GetSurfaceId());
  auto* mock_xdg_popup = mock_surface_popup->xdg_surface()->xdg_popup();

  EXPECT_EQ(mock_xdg_popup->anchor_rect().origin(),
            wayland_popup_bounds.origin());
  EXPECT_EQ(mock_surface_popup->opaque_region(),
            gfx::Rect(wayland_popup_bounds.size()));

  wayland_popup->Hide();
  window_->SetPointerFocus(false);
}

TEST_P(WaylandWindowTest, WaylandPopupNestedParent) {
  VerifyAndClearExpectations();

  gfx::Rect menu_window_bounds(gfx::Point(10, 10), gfx::Size(100, 100));
  auto menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, window_->GetWidget(), menu_window_bounds,
      &delegate_);
  EXPECT_TRUE(menu_window);

  VerifyAndClearExpectations();
  menu_window->SetPointerFocus(true);

  std::vector<PlatformWindowType> window_types{PlatformWindowType::kMenu,
                                               PlatformWindowType::kTooltip};
  for (const auto& type : window_types) {
    gfx::Rect nested_wayland_popup_bounds(gfx::Point(15, 15),
                                          gfx::Size(10, 10));
    auto nested_wayland_popup =
        CreateWaylandWindowWithParams(type, menu_window->GetWidget(),
                                      nested_wayland_popup_bounds, &delegate_);
    EXPECT_TRUE(nested_wayland_popup);

    VerifyAndClearExpectations();

    nested_wayland_popup->Show(false);

    Sync();

    auto* mock_surface_nested = server_.GetObject<wl::MockSurface>(
        nested_wayland_popup->root_surface()->GetSurfaceId());
    auto* mock_xdg_popup_nested =
        mock_surface_nested->xdg_surface()->xdg_popup();

    auto new_origin = nested_wayland_popup_bounds.origin() -
                      menu_window_bounds.origin().OffsetFromOrigin();
    EXPECT_EQ(mock_xdg_popup_nested->anchor_rect().origin(), new_origin);
    EXPECT_EQ(mock_surface_nested->opaque_region(),
              gfx::Rect(nested_wayland_popup_bounds.size()));

    menu_window->SetPointerFocus(false);
    nested_wayland_popup->Hide();
  }
}

TEST_P(WaylandWindowTest, OnSizeConstraintsChanged) {
  const bool kBooleans[] = {false, true};
  for (bool has_min_size : kBooleans) {
    for (bool has_max_size : kBooleans) {
      absl::optional<gfx::Size> min_size =
          has_min_size ? absl::optional<gfx::Size>(gfx::Size(100, 200))
                       : absl::nullopt;
      absl::optional<gfx::Size> max_size =
          has_max_size ? absl::optional<gfx::Size>(gfx::Size(300, 400))
                       : absl::nullopt;
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

  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window->root_surface()->GetSurfaceId());
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
  EXPECT_CALL(delegate, GetMenuType())
      .WillRepeatedly(Return(MenuType::kRootContextMenu));
  auto window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, window_->GetWidget(), gfx::Rect(0, 0, 50, 50),
      &delegate);
  ASSERT_TRUE(window);

  Sync();

  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window->root_surface()->GetSurfaceId());
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

TEST_P(WaylandWindowTest, ReattachesBackgroundOnShow) {
  EXPECT_TRUE(connection_->buffer_manager_host());

  auto interface_ptr = connection_->buffer_manager_host()->BindInterface();
  buffer_manager_gpu_->Initialize(
      std::move(interface_ptr), {}, false, true, false,
      /*supports_non_backed_solid_color_buffers*/ false);

  // Setup wl_buffers.
  constexpr uint32_t buffer_id1 = 1;
  constexpr uint32_t buffer_id2 = 2;
  gfx::Size buffer_size(1024, 768);
  auto length = 1024 * 768 * 4;
  buffer_manager_gpu_->CreateShmBasedBuffer(MakeFD(), length, buffer_size,
                                            buffer_id1);
  buffer_manager_gpu_->CreateShmBasedBuffer(MakeFD(), length, buffer_size,
                                            buffer_id2);

  Sync();

  // Create window.
  MockPlatformWindowDelegate delegate;
  auto window = CreateWaylandWindowWithParams(
      PlatformWindowType::kWindow, gfx::kNullAcceleratedWidget,
      gfx::Rect(0, 0, 100, 100), &delegate);
  ASSERT_TRUE(window);
  auto states = InitializeWlArrayWithActivatedState();

  Sync();

  // Configure window to be ready to attach wl_buffers.
  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window->root_surface()->GetSurfaceId());
  EXPECT_TRUE(mock_surface->xdg_surface());
  EXPECT_TRUE(mock_surface->xdg_surface()->xdg_toplevel());
  SendConfigureEvent(mock_surface->xdg_surface(), 100, 100, 1, states.get());

  // Commit a frame with only background.
  std::vector<ui::ozone::mojom::WaylandOverlayConfigPtr> overlays;
  ui::ozone::mojom::WaylandOverlayConfigPtr background{
      ui::ozone::mojom::WaylandOverlayConfig::New()};
  background->z_order = INT32_MIN;
  background->transform = gfx::OVERLAY_TRANSFORM_NONE;
  background->buffer_id = buffer_id1;
  background->surface_scale_factor = 1;
  background->opacity = 1.f;
  overlays.push_back(std::move(background));
  buffer_manager_gpu_->CommitOverlays(window->GetWidget(), std::move(overlays));
  mock_surface->SendFrameCallback();

  Sync();

  EXPECT_NE(mock_surface->attached_buffer(), nullptr);

  window->Hide();
  mock_surface->SendFrameCallback();

  Sync();

  mock_surface->ReleaseBuffer(mock_surface->attached_buffer());
  window->Show(false);

  Sync();

  SendConfigureEvent(mock_surface->xdg_surface(), 100, 100, 2, states.get());

  // Expects to receive an attach request on next frame.
  EXPECT_CALL(*mock_surface, Attach(_, _, _)).Times(1);

  // Commit a frame with only the primary_plane.
  overlays.clear();
  ui::ozone::mojom::WaylandOverlayConfigPtr primary{
      ui::ozone::mojom::WaylandOverlayConfig::New()};
  primary->z_order = 0;
  primary->transform = gfx::OVERLAY_TRANSFORM_NONE;
  primary->buffer_id = buffer_id2;
  primary->surface_scale_factor = 1;
  primary->opacity = 1.f;
  overlays.push_back(std::move(primary));
  buffer_manager_gpu_->CommitOverlays(window->GetWidget(), std::move(overlays));

  Sync();

  // WaylandWindow should automatically reattach the background.
  EXPECT_NE(mock_surface->attached_buffer(), nullptr);
}

// Tests that if the window gets hidden and shown again, the title, app id and
// size constraints remain the same.
TEST_P(WaylandWindowTest, SetsPropertiesOnShow) {
  constexpr char kAppId[] = "wayland_test";
  const std::u16string kTitle(u"WaylandWindowTest");

  PlatformWindowInitProperties properties;
  properties.bounds = gfx::Rect(0, 0, 100, 100);
  properties.type = PlatformWindowType::kWindow;
  properties.wm_class_class = kAppId;

  MockPlatformWindowDelegate delegate;
  auto window = WaylandWindow::Create(&delegate, connection_.get(),
                                      std::move(properties), true, true);
  DCHECK(window);
  window->Show(false);

  Sync();

  auto* mock_surface = server_.GetObject<wl::MockSurface>(
      window->root_surface()->GetSurfaceId());
  auto* mock_xdg_toplevel = mock_surface->xdg_surface()->xdg_toplevel();

  // Only app id must be set now.
  EXPECT_EQ(window->GetWindowUniqueId(), mock_xdg_toplevel->app_id());
  EXPECT_TRUE(mock_xdg_toplevel->title().empty());
  EXPECT_TRUE(mock_xdg_toplevel->min_size().IsEmpty());
  EXPECT_TRUE(mock_xdg_toplevel->max_size().IsEmpty());

  // Now, propagate size constraints and title.
  absl::optional<gfx::Size> min_size(gfx::Size(1, 1));
  absl::optional<gfx::Size> max_size(gfx::Size(100, 100));
  EXPECT_CALL(delegate, GetMinimumSizeForWindow())
      .Times(2)
      .WillRepeatedly(Return(min_size));
  EXPECT_CALL(delegate, GetMaximumSizeForWindow())
      .Times(2)
      .WillRepeatedly(Return(max_size));

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
  // WaylandToplevelWindow object that cached the values we set and must
  // restore them on Show().
  EXPECT_EQ(mock_xdg_toplevel->min_size(), min_size.value());
  EXPECT_EQ(mock_xdg_toplevel->max_size(), max_size.value());
  EXPECT_EQ(window->GetWindowUniqueId(), mock_xdg_toplevel->app_id());
  EXPECT_EQ(mock_xdg_toplevel->title(), base::UTF16ToUTF8(kTitle));
}

// Tests that a popup window is created using the serial of button press
// events as required by the Wayland protocol spec.
TEST_P(WaylandWindowTest, CreatesPopupOnButtonPressSerial) {
  for (bool use_explicit_grab : {false, true}) {
    base::test::ScopedCommandLine command_line_;
    if (use_explicit_grab) {
      command_line_.GetProcessCommandLine()->AppendSwitch(
          switches::kUseWaylandExplicitGrab);
    }

    wl_seat_send_capabilities(
        server_.seat()->resource(),
        WL_SEAT_CAPABILITY_POINTER | WL_SEAT_CAPABILITY_KEYBOARD);

    Sync();

    constexpr uint32_t enter_serial = 1;
    constexpr uint32_t button_press_serial = 2;
    constexpr uint32_t button_release_serial = 3;

    wl::MockSurface* toplevel_surface = server_.GetObject<wl::MockSurface>(
        window_->root_surface()->GetSurfaceId());
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
    EXPECT_CALL(delegate, GetMenuType())
        .WillOnce(Return(MenuType::kRootContextMenu));
    auto popup = CreateWaylandWindowWithParams(
        PlatformWindowType::kMenu, window_->GetWidget(),
        gfx::Rect(0, 0, 50, 50), &delegate);
    ASSERT_TRUE(popup);

    Sync();

    auto* test_popup = GetTestXdgPopupByWindow(popup.get());
    ASSERT_TRUE(test_popup);
    if (use_explicit_grab) {
      EXPECT_NE(test_popup->grab_serial(), button_release_serial);
      EXPECT_EQ(test_popup->grab_serial(), button_press_serial);
    } else {
      EXPECT_EQ(test_popup->grab_serial(), 0U);
    }
  }
}

// Tests that a popup window is created using the serial of touch down events
// as required by the Wayland protocol spec.
TEST_P(WaylandWindowTest, CreatesPopupOnTouchDownSerial) {
  for (bool use_explicit_grab : {false, true}) {
    base::test::ScopedCommandLine command_line_;
    if (use_explicit_grab) {
      command_line_.GetProcessCommandLine()->AppendSwitch(
          switches::kUseWaylandExplicitGrab);
    }
    wl_seat_send_capabilities(
        server_.seat()->resource(),
        WL_SEAT_CAPABILITY_TOUCH | WL_SEAT_CAPABILITY_KEYBOARD);

    Sync();

    constexpr uint32_t enter_serial = 1;
    constexpr uint32_t touch_down_serial = 2;
    constexpr uint32_t touch_up_serial = 3;

    wl::MockSurface* toplevel_surface = server_.GetObject<wl::MockSurface>(
        window_->root_surface()->GetSurfaceId());
    struct wl_array empty;
    wl_array_init(&empty);
    wl_keyboard_send_enter(server_.seat()->keyboard()->resource(), enter_serial,
                           toplevel_surface->resource(), &empty);

    // Send two events - touch down and touch up.
    wl_touch_send_down(server_.seat()->touch()->resource(), touch_down_serial,
                       0, surface_->resource(), 0 /* id */,
                       wl_fixed_from_int(50), wl_fixed_from_int(100));
    wl_touch_send_up(server_.seat()->touch()->resource(), touch_up_serial, 1000,
                     0 /* id */);

    Sync();

    // Create a popup window and verify the client used correct serial.
    MockPlatformWindowDelegate delegate;
    EXPECT_CALL(delegate, GetMenuType())
        .WillRepeatedly(Return(MenuType::kRootContextMenu));
    auto popup = CreateWaylandWindowWithParams(
        PlatformWindowType::kMenu, window_->GetWidget(),
        gfx::Rect(0, 0, 50, 50), &delegate);
    ASSERT_TRUE(popup);

    Sync();

    auto* test_popup = GetTestXdgPopupByWindow(popup.get());
    ASSERT_TRUE(test_popup);

    // Touch events are the exception. We can't use the serial that was sent
    // before the "up" event. Otherwise, some compositors may dismiss popups.
    // Thus, no serial must be used.
    EXPECT_EQ(test_popup->grab_serial(), 0U);

    popup->Hide();

    // Send a single down event now.
    wl_touch_send_down(server_.seat()->touch()->resource(), touch_down_serial,
                       0, surface_->resource(), 0 /* id */,
                       wl_fixed_from_int(50), wl_fixed_from_int(100));

    Sync();

    popup->Show(false);

    Sync();

    test_popup = GetTestXdgPopupByWindow(popup.get());
    ASSERT_TRUE(test_popup);

    uint32_t expected_serial = touch_down_serial;
#if !BUILDFLAG(IS_CHROMEOS_LACROS)
    auto env = base::Environment::Create();
    if (base::nix::GetDesktopEnvironment(env.get()) ==
        base::nix::DESKTOP_ENVIRONMENT_GNOME) {
      // We do not grab with touch events on gnome shell.
      expected_serial = 0u;
    }
#endif  // !BUILDFLAG(IS_CHROMEOS_LACROS)
    if (use_explicit_grab) {
      EXPECT_EQ(test_popup->grab_serial(), expected_serial);
    } else {
      EXPECT_EQ(test_popup->grab_serial(), 0U);
    }
  }
}

// Tests nested menu windows get the topmost window in the stack of windows
// within the same family/tree.
TEST_P(WaylandWindowTest, NestedPopupWindowsGetCorrectParent) {
  VerifyAndClearExpectations();

  gfx::Rect menu_window_bounds(gfx::Rect(10, 20, 20, 20));
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, window_->GetWidget(), menu_window_bounds,
      &delegate_);
  EXPECT_TRUE(menu_window);

  EXPECT_TRUE(menu_window->parent_window() == window_.get());

  gfx::Rect menu_window_bounds2(gfx::Rect(20, 40, 30, 20));
  std::unique_ptr<WaylandWindow> menu_window2 = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, menu_window->GetWidget(), menu_window_bounds2,
      &delegate_);
  EXPECT_TRUE(menu_window2);

  EXPECT_TRUE(menu_window2->parent_window() == menu_window.get());

  gfx::Rect menu_window_bounds3(gfx::Rect(30, 40, 30, 20));
  std::unique_ptr<WaylandWindow> menu_window3 = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, menu_window2->GetWidget(), menu_window_bounds3,
      &delegate_);
  EXPECT_TRUE(menu_window3);

  EXPECT_TRUE(menu_window3->parent_window() == menu_window2.get());

  gfx::Rect menu_window_bounds4(gfx::Rect(40, 40, 30, 20));
  std::unique_ptr<WaylandWindow> menu_window4 = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, menu_window3->GetWidget(), menu_window_bounds4,
      &delegate_);
  EXPECT_TRUE(menu_window4);

  EXPECT_TRUE(menu_window4->parent_window() == menu_window3.get());
}

TEST_P(WaylandWindowTest, DoesNotGrabPopupIfNoSeat) {
  // Create a popup window and verify the grab serial is not set.
  MockPlatformWindowDelegate delegate;
  EXPECT_CALL(delegate, GetMenuType())
      .WillOnce(Return(MenuType::kRootContextMenu));
  auto popup = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, window_->GetWidget(), gfx::Rect(0, 0, 50, 50),
      &delegate);
  ASSERT_TRUE(popup);

  Sync();

  auto* test_popup = GetTestXdgPopupByWindow(popup.get());
  ASSERT_TRUE(test_popup);
  EXPECT_EQ(test_popup->grab_serial(), 0u);
}

// Regression test for https://crbug.com/1247799.
TEST_P(WaylandWindowTest, DoesNotGrabPopupUnlessParentHasGrab) {
  wl_seat_send_capabilities(server_.seat()->resource(),
                            WL_SEAT_CAPABILITY_POINTER);
  Sync();
  ASSERT_TRUE(connection_->seat()->pointer());
  window_->SetPointerFocus(true);

  // Emulate a root menu creation with no serial available and ensure
  // ozone/wayland does not attempt to grab it.
  connection_->serial_tracker().ClearForTesting();

  MockPlatformWindowDelegate delegate;
  std::unique_ptr<WaylandWindow> root_menu;
  EXPECT_CALL(delegate, GetMenuType()).WillOnce(Return(MenuType::kRootMenu));
  root_menu = CreateWaylandWindowWithParams(PlatformWindowType::kMenu,
                                            window_->GetWidget(),
                                            gfx::Rect(50, 50), &delegate);
  Sync();
  VerifyAndClearExpectations();
  Mock::VerifyAndClearExpectations(&delegate);
  ASSERT_TRUE(root_menu);

  auto* server_root_menu = GetTestXdgPopupByWindow(root_menu.get());
  ASSERT_TRUE(server_root_menu);
  EXPECT_EQ(server_root_menu->grab_serial(), 0u);

  // Emulate a nested menu creation triggered by a mouse button event and ensure
  // ozone/wayland does not attempt to grab it, as its parent also has not grab.
  auto* server_root_menu_surface = server_.GetObject<wl::MockSurface>(
      root_menu->root_surface()->GetSurfaceId());
  ASSERT_TRUE(server_root_menu_surface);

  auto* pointer_resource = server_.seat()->pointer()->resource();
  wl_pointer_send_enter(pointer_resource, 3u /*serial*/,
                        server_root_menu_surface->resource(), 0, 0);
  wl_pointer_send_button(pointer_resource, 4u /*serial*/, 1, BTN_LEFT,
                         WL_POINTER_BUTTON_STATE_PRESSED);
  EXPECT_CALL(delegate, DispatchEvent(_)).Times(2);
  Sync();
  Mock::VerifyAndClearExpectations(&delegate);

  MockPlatformWindowDelegate delegate_2;
  std::unique_ptr<WaylandWindow> child_menu;
  EXPECT_CALL(delegate_2, GetMenuType()).WillOnce(Return(MenuType::kChildMenu));
  child_menu = CreateWaylandWindowWithParams(PlatformWindowType::kMenu,
                                             root_menu->GetWidget(),
                                             gfx::Rect(10, 10), &delegate_2);
  Sync();
  VerifyAndClearExpectations();
  Mock::VerifyAndClearExpectations(&delegate_2);
  ASSERT_TRUE(child_menu);

  auto* server_child_menu = GetTestXdgPopupByWindow(child_menu.get());
  ASSERT_TRUE(server_child_menu);
  EXPECT_EQ(server_child_menu->grab_serial(), 0u);

  wl_pointer_send_leave(pointer_resource, 5u /*serial*/,
                        server_root_menu_surface->resource());
  Sync();
}

TEST_P(WaylandWindowTest, OneWaylandSubsurface) {
  VerifyAndClearExpectations();

  std::unique_ptr<WaylandWindow> window = CreateWaylandWindowWithParams(
      PlatformWindowType::kWindow, gfx::kNullAcceleratedWidget,
      gfx::Rect(0, 0, 640, 480), &delegate_);
  EXPECT_TRUE(window);

  gfx::Rect subsurface_bounds(gfx::Point(15, 15), gfx::Size(10, 10));
  bool result = window->RequestSubsurface();
  EXPECT_TRUE(result);

  WaylandSubsurface* wayland_subsurface =
      window->wayland_subsurfaces().begin()->get();

  Sync();

  auto* mock_surface_root_window = server_.GetObject<wl::MockSurface>(
      window->root_surface()->GetSurfaceId());
  auto* mock_surface_subsurface = server_.GetObject<wl::MockSurface>(
      wayland_subsurface->wayland_surface()->GetSurfaceId());
  EXPECT_TRUE(mock_surface_subsurface);
  wayland_subsurface->ConfigureAndShowSurface(
      subsurface_bounds, gfx::Rect(0, 0, 640, 480) /*parent_bounds_px*/,
      1.f /*buffer_scale*/, nullptr, nullptr);
  connection_->ScheduleFlush();

  Sync();

  auto* test_subsurface = mock_surface_subsurface->sub_surface();
  EXPECT_TRUE(test_subsurface);
  auto* parent_resource = mock_surface_root_window->resource();
  EXPECT_EQ(parent_resource, test_subsurface->parent_resource());

  EXPECT_EQ(test_subsurface->position(), subsurface_bounds.origin());
  EXPECT_TRUE(test_subsurface->sync());
}

// Tests that WaylandPopups can be repositioned.
TEST_P(WaylandWindowTest, RepositionPopups) {
  VerifyAndClearExpectations();

  EXPECT_CALL(delegate_, GetMenuType())
      .WillRepeatedly(Return(MenuType::kRootContextMenu));
  gfx::Rect menu_window_bounds(gfx::Rect(6, 20, 8, 20));
  std::unique_ptr<WaylandWindow> menu_window = CreateWaylandWindowWithParams(
      PlatformWindowType::kMenu, window_->GetWidget(), menu_window_bounds,
      &delegate_);
  EXPECT_TRUE(menu_window);
  EXPECT_TRUE(menu_window->IsVisible());

  Sync();

  auto* mock_surface_popup = server_.GetObject<wl::MockSurface>(
      menu_window->root_surface()->GetSurfaceId());
  auto* mock_xdg_popup = mock_surface_popup->xdg_surface()->xdg_popup();

  EXPECT_EQ(mock_xdg_popup->anchor_rect().origin(),
            menu_window_bounds.origin());
  EXPECT_EQ(mock_xdg_popup->size(), menu_window_bounds.size());
  EXPECT_EQ(mock_surface_popup->opaque_region(),
            gfx::Rect(menu_window_bounds.size()));

  VerifyAndClearExpectations();

  const gfx::Rect damage_rect = {0, 0, menu_window_bounds.width(),
                                 menu_window_bounds.height()};
  EXPECT_CALL(delegate_, OnDamageRect(Eq(damage_rect))).Times(1);
  menu_window_bounds.set_origin({10, 10});
  menu_window->SetBounds(menu_window_bounds);

  Sync();

  // Xdg objects can be recreated depending on the version of the xdg shell.
  mock_surface_popup = server_.GetObject<wl::MockSurface>(
      menu_window->root_surface()->GetSurfaceId());
  mock_xdg_popup = mock_surface_popup->xdg_surface()->xdg_popup();

  EXPECT_EQ(mock_xdg_popup->anchor_rect().origin(),
            menu_window_bounds.origin());
  EXPECT_EQ(mock_xdg_popup->size(), menu_window_bounds.size());
  EXPECT_EQ(mock_surface_popup->opaque_region(),
            gfx::Rect(menu_window_bounds.size()));

  // This will send a configure event for the xdg_surface that backs the
  // xdg_popup. Size and state are not used there.
  SendConfigureEvent(mock_surface_popup->xdg_surface(), 0, 0, 1, nullptr);

  // Call sync so that server's configuration event is received by
  // Ozone/Wayland.
  Sync();

  VerifyAndClearExpectations();
}

// If buffers are not attached (aka WaylandBufferManagerHost is not used for
// buffer management), WaylandSurface::Commit mustn't result in creation of
// surface sync.
TEST_P(WaylandWindowTest, DoesNotCreateSurfaceSyncOnCommitWithoutBuffers) {
  EXPECT_THAT(window_->root_surface()->surface_sync_, nullptr);
  window_->root_surface()->Commit();
  EXPECT_THAT(window_->root_surface()->surface_sync_, nullptr);
}

TEST_P(WaylandWindowTest, StartWithMinimized) {
  // Make sure the window is initialized to normal state from the beginning.
  EXPECT_EQ(PlatformWindowState::kNormal, window_->GetPlatformWindowState());

  ScopedWlArray states = InitializeWlArrayWithActivatedState();
  SendConfigureEvent(xdg_surface_, 0, 0, 1, states.get());
  Sync();

  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(1);
  window_->Minimize();
  // The state of the window has to be already minimized.
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMinimized);

  // We don't receive any state change if that does not differ from the last
  // state.
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _)).Times(0);
  // It must be still the same minimized state.
  EXPECT_EQ(window_->GetPlatformWindowState(), PlatformWindowState::kMinimized);
  ui::PlatformWindowState state;
  EXPECT_CALL(delegate_, OnWindowStateChanged(_, _))
      .WillRepeatedly(DoAll(SaveArg<0>(&state), InvokeWithoutArgs([&]() {
                              EXPECT_EQ(state, PlatformWindowState::kMinimized);
                            })));
  // The window geometry has to be set to the current bounds of the window for
  // minimized state.
  gfx::Rect bounds = window_->GetBounds();
  EXPECT_CALL(*xdg_surface_, SetWindowGeometry(0, 0, bounds.width(), bounds.height()));
  // Send one additional empty configuration event for minimized state.
  // (which means the surface is not maximized, fullscreen or activated)
  states = ScopedWlArray();
  SendConfigureEvent(xdg_surface_, 0, 0, 2, states.get());
  Sync();
}

INSTANTIATE_TEST_SUITE_P(XdgVersionStableTest,
                         WaylandWindowTest,
                         Values(wl::ServerConfig{
                             .shell_version = wl::ShellVersion::kStable}));
INSTANTIATE_TEST_SUITE_P(XdgVersionV6Test,
                         WaylandWindowTest,
                         Values(wl::ServerConfig{
                             .shell_version = wl::ShellVersion::kV6}));

}  // namespace ui
