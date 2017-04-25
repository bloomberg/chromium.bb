// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "services/ui/common/types.h"
#include "services/ui/common/util.h"
#include "services/ui/public/interfaces/window_tree.mojom.h"
#include "services/ui/ws/display_manager.h"
#include "services/ui/ws/ids.h"
#include "services/ui/ws/platform_display.h"
#include "services/ui/ws/platform_display_factory.h"
#include "services/ui/ws/server_window.h"
#include "services/ui/ws/test_utils.h"
#include "services/ui/ws/window_manager_display_root.h"
#include "services/ui/ws/window_manager_state.h"
#include "services/ui/ws/window_server.h"
#include "services/ui/ws/window_server_delegate.h"
#include "services/ui/ws/window_tree.h"
#include "services/ui/ws/window_tree_binding.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
namespace ws {
namespace test {

const UserId kTestId1 = "20";

class CursorTest : public testing::Test {
 public:
  CursorTest() {}
  ~CursorTest() override {}

  WindowServer* window_server() { return ws_test_helper_.window_server(); }
  TestWindowServerDelegate* window_server_delegate() {
    return ws_test_helper_.window_server_delegate();
  }
  mojom::CursorType cursor() const { return ws_test_helper_.cursor(); }

 protected:
  // testing::Test:
  void SetUp() override {
    screen_manager_.Init(window_server()->display_manager());
    screen_manager_.AddDisplay();

    // As a side effect, this allocates Displays.
    AddWindowManager(window_server(), kTestId1);
    window_server()->user_id_tracker()->AddUserId(kTestId1);
    window_server()->user_id_tracker()->SetActiveUserId(kTestId1);
  }

  ServerWindow* GetRoot() {
    DisplayManager* display_manager = window_server()->display_manager();
    //    ASSERT_EQ(1u, display_manager->displays().size());
    Display* display = *display_manager->displays().begin();
    return display->GetWindowManagerDisplayRootForUser(kTestId1)->root();
  }

  // Create a 30x30 window where the outer 10 pixels is non-client.
  ServerWindow* BuildServerWindow() {
    DisplayManager* display_manager = window_server()->display_manager();
    Display* display = *display_manager->displays().begin();
    WindowManagerDisplayRoot* active_display_root =
        display->GetActiveWindowManagerDisplayRoot();
    WindowTree* tree =
        active_display_root->window_manager_state()->window_tree();
    ClientWindowId child_window_id;
    if (!NewWindowInTree(tree, &child_window_id))
      return nullptr;

    ServerWindow* w = tree->GetWindowByClientId(child_window_id);
    w->SetBounds(gfx::Rect(10, 10, 30, 30));
    w->SetClientArea(gfx::Insets(10, 10), std::vector<gfx::Rect>());
    w->SetVisible(true);

    return w;
  }

  void MoveCursorTo(const gfx::Point& p) {
    DisplayManager* display_manager = window_server()->display_manager();
    ASSERT_EQ(1u, display_manager->displays().size());
    Display* display = *display_manager->displays().begin();
    WindowManagerDisplayRoot* active_display_root =
        display->GetActiveWindowManagerDisplayRoot();
    ASSERT_TRUE(active_display_root);
    PointerEvent event(
        MouseEvent(ET_MOUSE_MOVED, p, p, base::TimeTicks(), 0, 0));
    ignore_result(static_cast<PlatformDisplayDelegate*>(display)
                      ->GetEventSink()
                      ->OnEventFromSource(&event));
    WindowManagerState* wms = active_display_root->window_manager_state();
    ASSERT_TRUE(WindowManagerStateTestApi(wms).AckInFlightEvent(
        mojom::EventResult::HANDLED));
  }

 private:
  WindowServerTestHelper ws_test_helper_;
  TestScreenManager screen_manager_;
  DISALLOW_COPY_AND_ASSIGN(CursorTest);
};

TEST_F(CursorTest, ChangeByMouseMove) {
  ServerWindow* win = BuildServerWindow();
  win->SetPredefinedCursor(mojom::CursorType::kIBeam);
  win->parent()->SetPredefinedCursor(mojom::CursorType::kCell);
  EXPECT_EQ(mojom::CursorType::kIBeam, win->cursor());
  win->SetNonClientCursor(mojom::CursorType::kEastResize);
  EXPECT_EQ(mojom::CursorType::kEastResize, win->non_client_cursor());

  // Non client area
  MoveCursorTo(gfx::Point(15, 15));
  EXPECT_EQ(mojom::CursorType::kEastResize, cursor());

  // Client area, which comes from win->parent().
  MoveCursorTo(gfx::Point(25, 25));
  EXPECT_EQ(mojom::CursorType::kCell, cursor());
}

TEST_F(CursorTest, ChangeByClientAreaChange) {
  ServerWindow* win = BuildServerWindow();
  win->parent()->SetPredefinedCursor(mojom::CursorType::kCross);
  win->SetPredefinedCursor(mojom::CursorType::kIBeam);
  EXPECT_EQ(mojom::CursorType::kIBeam, mojom::CursorType(win->cursor()));
  win->SetNonClientCursor(mojom::CursorType::kEastResize);
  EXPECT_EQ(mojom::CursorType::kEastResize, win->non_client_cursor());

  // Non client area before we move.
  MoveCursorTo(gfx::Point(15, 15));
  EXPECT_EQ(mojom::CursorType::kEastResize, cursor());

  // Changing the client area should cause a change. The cursor for the client
  // area comes from root ancestor, which is win->parent().
  win->SetClientArea(gfx::Insets(1, 1), std::vector<gfx::Rect>());
  EXPECT_EQ(mojom::CursorType::kCross, cursor());
}

TEST_F(CursorTest, NonClientCursorChange) {
  ServerWindow* win = BuildServerWindow();
  win->SetPredefinedCursor(mojom::CursorType::kIBeam);
  EXPECT_EQ(mojom::CursorType::kIBeam, win->cursor());
  win->SetNonClientCursor(mojom::CursorType::kEastResize);
  EXPECT_EQ(mojom::CursorType::kEastResize, win->non_client_cursor());

  MoveCursorTo(gfx::Point(15, 15));
  EXPECT_EQ(mojom::CursorType::kEastResize, cursor());

  win->SetNonClientCursor(mojom::CursorType::kWestResize);
  EXPECT_EQ(mojom::CursorType::kWestResize, cursor());
}

TEST_F(CursorTest, IgnoreClientCursorChangeInNonClientArea) {
  ServerWindow* win = BuildServerWindow();
  win->SetPredefinedCursor(mojom::CursorType::kIBeam);
  EXPECT_EQ(mojom::CursorType::kIBeam, win->cursor());
  win->SetNonClientCursor(mojom::CursorType::kEastResize);
  EXPECT_EQ(mojom::CursorType::kEastResize, win->non_client_cursor());

  MoveCursorTo(gfx::Point(15, 15));
  EXPECT_EQ(mojom::CursorType::kEastResize, cursor());

  win->SetPredefinedCursor(mojom::CursorType::kHelp);
  EXPECT_EQ(mojom::CursorType::kEastResize, cursor());
}

TEST_F(CursorTest, NonClientToClientByBoundsChange) {
  ServerWindow* win = BuildServerWindow();
  win->parent()->SetPredefinedCursor(mojom::CursorType::kCopy);
  win->SetPredefinedCursor(mojom::CursorType::kIBeam);
  EXPECT_EQ(mojom::CursorType::kIBeam, win->cursor());
  win->SetNonClientCursor(mojom::CursorType::kEastResize);
  EXPECT_EQ(mojom::CursorType::kEastResize, win->non_client_cursor());

  // Non client area before we move.
  MoveCursorTo(gfx::Point(15, 15));
  EXPECT_EQ(mojom::CursorType::kEastResize, cursor());

  win->SetBounds(gfx::Rect(0, 0, 30, 30));
  EXPECT_EQ(mojom::CursorType::kCopy, cursor());
}

}  // namespace test
}  // namespace ws
}  // namespace ui
