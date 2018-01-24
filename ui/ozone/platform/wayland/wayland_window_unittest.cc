// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <wayland-server-core.h>
#include <xdg-shell-unstable-v5-server-protocol.h>
#include <xdg-shell-unstable-v6-server-protocol.h>

#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event.h"
#include "ui/ozone/platform/wayland/fake_server.h"
#include "ui/ozone/platform/wayland/wayland_test.h"
#include "ui/ozone/platform/wayland/wayland_window.h"
#include "ui/ozone/test/mock_platform_window_delegate.h"

using ::testing::Eq;
using ::testing::Mock;
using ::testing::SaveArg;
using ::testing::StrEq;
using ::testing::_;

namespace ui {

class WaylandWindowTest : public WaylandTest {
 public:
  WaylandWindowTest()
      : test_mouse_event(ET_MOUSE_PRESSED,
                         gfx::Point(10, 15),
                         gfx::Point(10, 15),
                         ui::EventTimeStampFromSeconds(123456),
                         EF_LEFT_MOUSE_BUTTON | EF_RIGHT_MOUSE_BUTTON,
                         EF_LEFT_MOUSE_BUTTON) {}

  void SetUp() override {
    WaylandTest::SetUp();

    xdg_surface = surface->xdg_surface.get();
    ASSERT_TRUE(xdg_surface);
  }

 protected:
  void SendConfigureEvent(int width,
                          int height,
                          uint32_t serial,
                          struct wl_array* states) {
    if (!xdg_surface->xdg_toplevel) {
      xdg_surface_send_configure(xdg_surface->resource(), width, height, states,
                                 serial);
      return;
    }

    // In xdg_shell_v6, both surfaces send serial configure event and toplevel
    // surfaces send other data like states, heights and widths.
    zxdg_surface_v6_send_configure(xdg_surface->resource(), serial);
    ASSERT_TRUE(xdg_surface->xdg_toplevel);
    zxdg_toplevel_v6_send_configure(xdg_surface->xdg_toplevel->resource(),
                                    width, height, states);
  }

  // Depending on a shell version, xdg_surface or xdg_toplevel surface should
  // get the mock calls. This method decided, which surface to use.
  wl::MockXdgSurface* GetXdgSurface() {
    if (GetParam() == kXdgShellV5)
      return xdg_surface;
    return xdg_surface->xdg_toplevel.get();
  }

  void SetWlArrayWithState(uint32_t state, wl_array* states) {
    uint32_t* s;
    s = static_cast<uint32_t*>(wl_array_add(states, sizeof *s));
    *s = state;
  }

  void InitializeWlArrayWithActivatedState(wl_array* states) {
    wl_array_init(states);
    SetWlArrayWithState(XDG_SURFACE_STATE_ACTIVATED, states);
  }

  wl::MockXdgSurface* xdg_surface;

  MouseEvent test_mouse_event;

 private:
  DISALLOW_COPY_AND_ASSIGN(WaylandWindowTest);
};

TEST_P(WaylandWindowTest, SetTitle) {
  EXPECT_CALL(*GetXdgSurface(), SetTitle(StrEq("hello")));
  window->SetTitle(base::ASCIIToUTF16("hello"));
}

TEST_P(WaylandWindowTest, MaximizeAndRestore) {
  wl_array states;
  InitializeWlArrayWithActivatedState(&states);

  EXPECT_CALL(delegate,
              OnWindowStateChanged(Eq(PLATFORM_WINDOW_STATE_MAXIMIZED)));
  SetWlArrayWithState(XDG_SURFACE_STATE_MAXIMIZED, &states);

  EXPECT_CALL(*GetXdgSurface(), SetMaximized());
  window->Maximize();
  SendConfigureEvent(0, 0, 1, &states);
  Sync();

  EXPECT_CALL(delegate, OnWindowStateChanged(Eq(PLATFORM_WINDOW_STATE_NORMAL)));
  EXPECT_CALL(*GetXdgSurface(), UnsetMaximized());
  window->Restore();
  // Reinitialize wl_array, which removes previous old states.
  InitializeWlArrayWithActivatedState(&states);
  SendConfigureEvent(0, 0, 2, &states);
  Sync();
}

TEST_P(WaylandWindowTest, Minimize) {
  wl_array states;
  wl_array_init(&states);

  // Initialize to normal first.
  EXPECT_CALL(delegate, OnWindowStateChanged(Eq(PLATFORM_WINDOW_STATE_NORMAL)));
  SendConfigureEvent(0, 0, 1, &states);
  Sync();

  EXPECT_CALL(*GetXdgSurface(), SetMinimized());
  // The state of the window must retain minimized, which means we are not
  // notified about the state, because 1) minimized state was set manually
  // in WaylandWindow, and it has been confirmed in a back call from the server,
  // which resulted in the same state as before.
  EXPECT_CALL(delegate, OnWindowStateChanged(_)).Times(0);
  window->Minimize();
  // Reinitialize wl_array, which removes previous old states.
  wl_array_init(&states);
  SendConfigureEvent(0, 0, 2, &states);
  Sync();
}

TEST_P(WaylandWindowTest, SetFullscreenAndRestore) {
  wl_array states;
  InitializeWlArrayWithActivatedState(&states);

  SetWlArrayWithState(XDG_SURFACE_STATE_FULLSCREEN, &states);

  EXPECT_CALL(*GetXdgSurface(), SetFullscreen());
  EXPECT_CALL(delegate,
              OnWindowStateChanged(Eq(PLATFORM_WINDOW_STATE_FULLSCREEN)));
  window->ToggleFullscreen();
  SendConfigureEvent(0, 0, 1, &states);
  Sync();

  EXPECT_CALL(*GetXdgSurface(), UnsetFullscreen());
  EXPECT_CALL(delegate, OnWindowStateChanged(Eq(PLATFORM_WINDOW_STATE_NORMAL)));
  window->Restore();
  // Reinitialize wl_array, which removes previous old states.
  InitializeWlArrayWithActivatedState(&states);
  SendConfigureEvent(0, 0, 2, &states);
  Sync();
}

TEST_P(WaylandWindowTest, SetMaximizedFullscreenAndRestore) {
  wl_array states;
  InitializeWlArrayWithActivatedState(&states);

  EXPECT_CALL(*GetXdgSurface(), SetMaximized());
  EXPECT_CALL(delegate,
              OnWindowStateChanged(Eq(PLATFORM_WINDOW_STATE_MAXIMIZED)));
  window->Maximize();
  SetWlArrayWithState(XDG_SURFACE_STATE_MAXIMIZED, &states);
  SendConfigureEvent(0, 0, 2, &states);
  Sync();

  EXPECT_CALL(*GetXdgSurface(), SetFullscreen());
  EXPECT_CALL(delegate,
              OnWindowStateChanged(Eq(PLATFORM_WINDOW_STATE_FULLSCREEN)));
  window->ToggleFullscreen();
  SetWlArrayWithState(XDG_SURFACE_STATE_FULLSCREEN, &states);
  SendConfigureEvent(0, 0, 3, &states);
  Sync();

  EXPECT_CALL(*GetXdgSurface(), UnsetFullscreen());
  EXPECT_CALL(*GetXdgSurface(), UnsetMaximized());
  EXPECT_CALL(delegate, OnWindowStateChanged(Eq(PLATFORM_WINDOW_STATE_NORMAL)));
  window->Restore();
  // Reinitialize wl_array, which removes previous old states.
  InitializeWlArrayWithActivatedState(&states);
  SendConfigureEvent(0, 0, 4, &states);
  Sync();
}

TEST_P(WaylandWindowTest, RestoreBoundsAfterMaximize) {
  const gfx::Rect current_bounds = window->GetBounds();

  wl_array states;
  InitializeWlArrayWithActivatedState(&states);

  const gfx::Rect maximized_bounds = gfx::Rect(0, 0, 1024, 768);
  EXPECT_CALL(delegate, OnBoundsChanged(Eq(maximized_bounds)));
  window->Maximize();
  SetWlArrayWithState(XDG_SURFACE_STATE_MAXIMIZED, &states);
  SendConfigureEvent(maximized_bounds.width(), maximized_bounds.height(), 1,
                     &states);
  Sync();

  EXPECT_CALL(delegate, OnBoundsChanged(Eq(current_bounds)));
  // Both in XdgV5 and XdgV6, surfaces implement SetWindowGeometry method.
  // Thus, using a toplevel object in XdgV6 case is not right thing. Use a
  // surface here instead.
  EXPECT_CALL(*xdg_surface, SetWindowGeometry(0, 0, current_bounds.width(),
                                              current_bounds.height()));
  window->Restore();
  // Reinitialize wl_array, which removes previous old states.
  InitializeWlArrayWithActivatedState(&states);
  SendConfigureEvent(0, 0, 2, &states);
  Sync();
}

TEST_P(WaylandWindowTest, RestoreBoundsAfterFullscreen) {
  const gfx::Rect current_bounds = window->GetBounds();

  wl_array states;
  InitializeWlArrayWithActivatedState(&states);

  const gfx::Rect fullscreen_bounds = gfx::Rect(0, 0, 1280, 720);
  EXPECT_CALL(delegate, OnBoundsChanged(Eq(fullscreen_bounds)));
  window->ToggleFullscreen();
  SetWlArrayWithState(XDG_SURFACE_STATE_FULLSCREEN, &states);
  SendConfigureEvent(fullscreen_bounds.width(), fullscreen_bounds.height(), 1,
                     &states);
  Sync();

  EXPECT_CALL(delegate, OnBoundsChanged(Eq(current_bounds)));
  // Both in XdgV5 and XdgV6, surfaces implement SetWindowGeometry method.
  // Thus, using a toplevel object in XdgV6 case is not right thing. Use a
  // surface here instead.
  EXPECT_CALL(*xdg_surface, SetWindowGeometry(0, 0, current_bounds.width(),
                                              current_bounds.height()));
  window->Restore();
  // Reinitialize wl_array, which removes previous old states.
  InitializeWlArrayWithActivatedState(&states);
  SendConfigureEvent(0, 0, 2, &states);
  Sync();
}

TEST_P(WaylandWindowTest, RestoreBoundsAfterMaximizeAndFullscreen) {
  const gfx::Rect current_bounds = window->GetBounds();

  wl_array states;
  InitializeWlArrayWithActivatedState(&states);

  const gfx::Rect maximized_bounds = gfx::Rect(0, 0, 1024, 768);
  EXPECT_CALL(delegate, OnBoundsChanged(Eq(maximized_bounds)));
  window->Maximize();
  SetWlArrayWithState(XDG_SURFACE_STATE_MAXIMIZED, &states);
  SendConfigureEvent(maximized_bounds.width(), maximized_bounds.height(), 1,
                     &states);
  Sync();

  const gfx::Rect fullscreen_bounds = gfx::Rect(0, 0, 1280, 720);
  EXPECT_CALL(delegate, OnBoundsChanged(Eq(fullscreen_bounds)));
  window->ToggleFullscreen();
  SetWlArrayWithState(XDG_SURFACE_STATE_FULLSCREEN, &states);
  SendConfigureEvent(fullscreen_bounds.width(), fullscreen_bounds.height(), 2,
                     &states);
  Sync();

  EXPECT_CALL(delegate, OnBoundsChanged(Eq(maximized_bounds)));
  window->Maximize();
  // Reinitialize wl_array, which removes previous old states.
  InitializeWlArrayWithActivatedState(&states);
  SetWlArrayWithState(XDG_SURFACE_STATE_MAXIMIZED, &states);
  SendConfigureEvent(maximized_bounds.width(), maximized_bounds.height(), 3,
                     &states);
  Sync();

  EXPECT_CALL(delegate, OnBoundsChanged(Eq(current_bounds)));
  // Both in XdgV5 and XdgV6, surfaces implement SetWindowGeometry method.
  // Thus, using a toplevel object in XdgV6 case is not right thing. Use a
  // surface here instead.
  EXPECT_CALL(*xdg_surface, SetWindowGeometry(0, 0, current_bounds.width(),
                                              current_bounds.height()));
  window->Restore();
  // Reinitialize wl_array, which removes previous old states.
  InitializeWlArrayWithActivatedState(&states);
  SendConfigureEvent(0, 0, 4, &states);
  Sync();
}

TEST_P(WaylandWindowTest, SendsBoundsOnRequest) {
  const gfx::Rect initial_bounds = window->GetBounds();

  const gfx::Rect new_bounds = gfx::Rect(0, 0, initial_bounds.width() + 10,
                                         initial_bounds.height() + 10);
  EXPECT_CALL(delegate, OnBoundsChanged(Eq(new_bounds)));
  window->SetBounds(new_bounds);

  wl_array states;
  InitializeWlArrayWithActivatedState(&states);

  // First case is when Wayland sends a configure event with 0,0 height and
  // widht.
  EXPECT_CALL(*xdg_surface,
              SetWindowGeometry(0, 0, new_bounds.width(), new_bounds.height()))
      .Times(2);
  SendConfigureEvent(0, 0, 2, &states);
  Sync();

  // Second case is when Wayland sends a configure event with 1, 1 height and
  // width. It looks more like a bug in Gnome Shell with Wayland as long as the
  // documentation says it must be set to 0, 0, when wayland requests bounds.
  SendConfigureEvent(0, 0, 3, &states);
  Sync();
}

TEST_P(WaylandWindowTest, CanDispatchMouseEventDefault) {
  EXPECT_FALSE(window->CanDispatchEvent(&test_mouse_event));
}

TEST_P(WaylandWindowTest, CanDispatchMouseEventFocus) {
  window->set_pointer_focus(true);
  EXPECT_TRUE(window->CanDispatchEvent(&test_mouse_event));
}

TEST_P(WaylandWindowTest, CanDispatchMouseEventUnfocus) {
  window->set_pointer_focus(false);
  EXPECT_FALSE(window->CanDispatchEvent(&test_mouse_event));
}

ACTION_P(CloneEvent, ptr) {
  *ptr = Event::Clone(*arg0);
}

TEST_P(WaylandWindowTest, DispatchEvent) {
  std::unique_ptr<Event> event;
  EXPECT_CALL(delegate, DispatchEvent(_)).WillOnce(CloneEvent(&event));
  window->DispatchEvent(&test_mouse_event);
  ASSERT_TRUE(event);
  ASSERT_TRUE(event->IsMouseEvent());
  auto* mouse_event = event->AsMouseEvent();
  EXPECT_EQ(mouse_event->location_f(), test_mouse_event.location_f());
  EXPECT_EQ(mouse_event->root_location_f(), test_mouse_event.root_location_f());
  EXPECT_EQ(mouse_event->time_stamp(), test_mouse_event.time_stamp());
  EXPECT_EQ(mouse_event->button_flags(), test_mouse_event.button_flags());
  EXPECT_EQ(mouse_event->changed_button_flags(),
            test_mouse_event.changed_button_flags());
}

TEST_P(WaylandWindowTest, ConfigureEvent) {
  wl_array states;
  wl_array_init(&states);
  SendConfigureEvent(1000, 1000, 12, &states);
  SendConfigureEvent(1500, 1000, 13, &states);

  // Make sure that the implementation does not call OnBoundsChanged for each
  // configure event if it receives multiple in a row.
  EXPECT_CALL(delegate, OnBoundsChanged(Eq(gfx::Rect(0, 0, 1500, 1000))));
  // Responding to a configure event, the window geometry in here must respect
  // the sizing negotiations specified by the configure event.
  // |xdg_surface| must receive the following calls in both xdg_shell_v5 and
  // xdg_shell_v6. Other calls like SetTitle or SetMaximized are recieved by
  // xdg_toplevel in xdg_shell_v6 and by xdg_surface in xdg_shell_v5.
  EXPECT_CALL(*xdg_surface, SetWindowGeometry(0, 0, 1500, 1000)).Times(1);
  EXPECT_CALL(*xdg_surface, AckConfigure(13));
}

TEST_P(WaylandWindowTest, ConfigureEventWithNulledSize) {
  wl_array states;
  wl_array_init(&states);

  // If Wayland sends configure event with 0 width and 0 size, client should
  // call back with desired sizes. In this case, that's the actual size of
  // the window.
  SendConfigureEvent(0, 0, 14, &states);
  // |xdg_surface| must receive the following calls in both xdg_shell_v5 and
  // xdg_shell_v6. Other calls like SetTitle or SetMaximized are recieved by
  // xdg_toplevel in xdg_shell_v6 and by xdg_surface in xdg_shell_v5.
  EXPECT_CALL(*xdg_surface, SetWindowGeometry(0, 0, 800, 600));
  EXPECT_CALL(*xdg_surface, AckConfigure(14));
}

INSTANTIATE_TEST_CASE_P(XdgVersionV5Test,
                        WaylandWindowTest,
                        ::testing::Values(kXdgShellV5));
INSTANTIATE_TEST_CASE_P(XdgVersionV6Test,
                        WaylandWindowTest,
                        ::testing::Values(kXdgShellV6));

}  // namespace ui
