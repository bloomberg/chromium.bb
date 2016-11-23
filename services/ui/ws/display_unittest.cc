// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "services/ui/common/types.h"
#include "services/ui/common/util.h"
#include "services/ui/display/viewport_metrics.h"
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

using display::ViewportMetrics;

namespace ui {
namespace ws {
namespace test {
namespace {

const UserId kTestId1 = "2";
const UserId kTestId2 = "21";

ClientWindowId ClientWindowIdForFirstRoot(WindowTree* tree) {
  if (tree->roots().empty())
    return ClientWindowId();
  return ClientWindowIdForWindow(tree, *tree->roots().begin());
}

WindowManagerState* GetWindowManagerStateForUser(Display* display,
                                                 const UserId& user_id) {
  WindowManagerDisplayRoot* display_root =
      display->GetWindowManagerDisplayRootForUser(user_id);
  return display_root ? display_root->window_manager_state() : nullptr;
}

// Returns the root ServerWindow for the specified Display.
ServerWindow* GetRootOnDisplay(WindowTree* tree, Display* display) {
  for (const ServerWindow* root : tree->roots()) {
    if (tree->GetDisplay(root) == display)
      return const_cast<ServerWindow*>(root);
  }
  return nullptr;
}

// Tracks destruction of a ServerWindow, setting a bool* to true when
// OnWindowDestroyed() is called
class ServerWindowDestructionObserver : public ServerWindowObserver {
 public:
  ServerWindowDestructionObserver(ServerWindow* window, bool* destroyed)
      : window_(window), destroyed_(destroyed) {
    window_->AddObserver(this);
  }
  ~ServerWindowDestructionObserver() override {
    if (window_)
      window_->RemoveObserver(this);
  }

  // ServerWindowObserver:
  void OnWindowDestroyed(ServerWindow* window) override {
    *destroyed_ = true;
    window_->RemoveObserver(this);
    window_ = nullptr;
  }

 private:
  ServerWindow* window_;
  bool* destroyed_;

  DISALLOW_COPY_AND_ASSIGN(ServerWindowDestructionObserver);
};

}  // namespace

// -----------------------------------------------------------------------------

class DisplayTest : public testing::Test {
 public:
  DisplayTest() {}
  ~DisplayTest() override {}

  WindowServer* window_server() { return ws_test_helper_.window_server(); }
  DisplayManager* display_manager() {
    return window_server()->display_manager();
  }
  TestWindowServerDelegate* window_server_delegate() {
    return ws_test_helper_.window_server_delegate();
  }
  TestPlatformScreen& platform_screen() { return platform_screen_; }

 protected:
  // testing::Test:
  void SetUp() override {
    platform_screen_.Init(window_server()->display_manager());
    window_server()->user_id_tracker()->AddUserId(kTestId1);
    window_server()->user_id_tracker()->AddUserId(kTestId2);
  }

 private:
  WindowServerTestHelper ws_test_helper_;
  TestPlatformScreen platform_screen_;

  DISALLOW_COPY_AND_ASSIGN(DisplayTest);
};

TEST_F(DisplayTest, CreateDisplay) {
  AddWindowManager(window_server(), kTestId1);
  const int64_t display_id =
      platform_screen().AddDisplay(MakeViewportMetrics(0, 0, 1024, 768, 1.0f));

  ASSERT_EQ(1u, display_manager()->displays().size());
  Display* display = display_manager()->GetDisplayById(display_id);

  // Display should have root window with correct size.
  ASSERT_NE(nullptr, display->root_window());
  EXPECT_EQ("0,0 1024x768", display->root_window()->bounds().ToString());

  // Display should have a WM root window with the correct size too.
  EXPECT_EQ(1u, display->num_window_manger_states());
  WindowManagerDisplayRoot* root1 =
      display->GetWindowManagerDisplayRootForUser(kTestId1);
  ASSERT_NE(nullptr, root1);
  ASSERT_NE(nullptr, root1->root());
  EXPECT_EQ("0,0 1024x768", root1->root()->bounds().ToString());
}

TEST_F(DisplayTest, CreateDisplayBeforeWM) {
  // Add one display, no WM exists yet.
  const int64_t display_id =
      platform_screen().AddDisplay(MakeViewportMetrics(0, 0, 1024, 768, 1.0f));
  EXPECT_EQ(1u, display_manager()->displays().size());

  Display* display = display_manager()->GetDisplayById(display_id);

  // Display should have root window with correct size.
  ASSERT_NE(nullptr, display->root_window());
  EXPECT_EQ("0,0 1024x768", display->root_window()->bounds().ToString());

  // There should be no WM state for display yet.
  EXPECT_EQ(0u, display->num_window_manger_states());
  EXPECT_EQ(nullptr, display->GetWindowManagerDisplayRootForUser(kTestId1));

  AddWindowManager(window_server(), kTestId1);

  // After adding a WM display should have WM state and WM root for the display.
  EXPECT_EQ(1u, display->num_window_manger_states());
  WindowManagerDisplayRoot* root1 =
      display->GetWindowManagerDisplayRootForUser(kTestId1);
  ASSERT_NE(nullptr, root1);
  ASSERT_NE(nullptr, root1->root());
  EXPECT_EQ("0,0 1024x768", root1->root()->bounds().ToString());
}

TEST_F(DisplayTest, CreateDisplayWithTwoWindowManagers) {
  AddWindowManager(window_server(), kTestId1);
  const int64_t display_id = platform_screen().AddDisplay();
  Display* display = display_manager()->GetDisplayById(display_id);

  // There should be only be one WM at this point.
  ASSERT_EQ(1u, display->num_window_manger_states());
  EXPECT_NE(nullptr, display->GetWindowManagerDisplayRootForUser(kTestId1));
  EXPECT_EQ(nullptr, display->GetWindowManagerDisplayRootForUser(kTestId2));

  AddWindowManager(window_server(), kTestId2);

  // There should now be two WMs.
  ASSERT_EQ(2u, display->num_window_manger_states());
  WindowManagerDisplayRoot* root1 =
      display->GetWindowManagerDisplayRootForUser(kTestId1);
  ASSERT_NE(nullptr, root1);
  WindowManagerDisplayRoot* root2 =
      display->GetWindowManagerDisplayRootForUser(kTestId2);
  ASSERT_NE(nullptr, root2);

  // Verify the two WMs have different roots but with the same bounds.
  EXPECT_NE(root1, root2);
  EXPECT_NE(root1->root(), root2->root());
  EXPECT_EQ(root1->root()->bounds(), root2->root()->bounds());
}

TEST_F(DisplayTest, CreateDisplayWithDeviceScaleFactor) {
  // The display bounds should be the pixel_size / device_scale_factor.
  const ViewportMetrics metrics = MakeViewportMetrics(0, 0, 1024, 768, 2.0f);
  EXPECT_EQ("0,0 512x384", metrics.bounds.ToString());
  EXPECT_EQ("1024x768", metrics.pixel_size.ToString());

  const int64_t display_id = platform_screen().AddDisplay(metrics);
  Display* display = display_manager()->GetDisplayById(display_id);

  // The root ServerWindow bounds should be in PP.
  EXPECT_EQ("0,0 1024x768", display->root_window()->bounds().ToString());

  ViewportMetrics modified_metrics = metrics;
  modified_metrics.work_area.set_height(metrics.work_area.height() - 48);
  platform_screen().ModifyDisplay(display_id, modified_metrics);

  // The root ServerWindow should still be in PP after updating the work area.
  EXPECT_EQ("0,0 1024x768", display->root_window()->bounds().ToString());
}

TEST_F(DisplayTest, Destruction) {
  AddWindowManager(window_server(), kTestId1);

  int64_t display_id = platform_screen().AddDisplay();

  // Add a second WM.
  AddWindowManager(window_server(), kTestId2);
  ASSERT_EQ(1u, display_manager()->displays().size());
  Display* display = display_manager()->GetDisplayById(display_id);
  ASSERT_EQ(2u, display->num_window_manger_states());
  // There should be two trees, one for each windowmanager.
  EXPECT_EQ(2u, window_server()->num_trees());

  {
    WindowManagerState* state = GetWindowManagerStateForUser(display, kTestId1);
    // Destroy the tree associated with |state|. Should result in deleting
    // |state|.
    window_server()->DestroyTree(state->window_tree());
    ASSERT_EQ(1u, display->num_window_manger_states());
    EXPECT_FALSE(GetWindowManagerStateForUser(display, kTestId1));
    EXPECT_EQ(1u, display_manager()->displays().size());
    EXPECT_EQ(1u, window_server()->num_trees());
  }

  EXPECT_FALSE(window_server_delegate()->got_on_no_more_displays());
  platform_screen().RemoveDisplay(display_id);
  // There is still one tree left.
  EXPECT_EQ(1u, window_server()->num_trees());
  EXPECT_TRUE(window_server_delegate()->got_on_no_more_displays());
}

TEST_F(DisplayTest, EventStateResetOnUserSwitch) {
  const int64_t display_id = platform_screen().AddDisplay();

  AddWindowManager(window_server(), kTestId1);
  AddWindowManager(window_server(), kTestId2);

  window_server()->user_id_tracker()->SetActiveUserId(kTestId1);

  ASSERT_EQ(1u, display_manager()->displays().size());
  Display* display = display_manager()->GetDisplayById(display_id);
  WindowManagerState* active_wms =
      display->GetActiveWindowManagerDisplayRoot()->window_manager_state();
  ASSERT_TRUE(active_wms);
  EXPECT_EQ(kTestId1, active_wms->user_id());

  static_cast<PlatformDisplayDelegate*>(display)->OnEvent(ui::PointerEvent(
      ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(20, 25),
                     gfx::Point(20, 25), base::TimeTicks(),
                     ui::EF_LEFT_MOUSE_BUTTON, ui::EF_LEFT_MOUSE_BUTTON)));

  EXPECT_TRUE(EventDispatcherTestApi(active_wms->event_dispatcher())
                  .AreAnyPointersDown());
  EXPECT_EQ(gfx::Point(20, 25),
            active_wms->event_dispatcher()->mouse_pointer_last_location());

  // Switch the user. Should trigger resetting state in old event dispatcher
  // and update state in new event dispatcher.
  window_server()->user_id_tracker()->SetActiveUserId(kTestId2);
  EXPECT_NE(
      active_wms,
      display->GetActiveWindowManagerDisplayRoot()->window_manager_state());
  EXPECT_FALSE(EventDispatcherTestApi(active_wms->event_dispatcher())
                   .AreAnyPointersDown());
  active_wms =
      display->GetActiveWindowManagerDisplayRoot()->window_manager_state();
  EXPECT_EQ(kTestId2, active_wms->user_id());
  EXPECT_EQ(gfx::Point(20, 25),
            active_wms->event_dispatcher()->mouse_pointer_last_location());
  EXPECT_FALSE(EventDispatcherTestApi(active_wms->event_dispatcher())
                   .AreAnyPointersDown());
}

// Verifies capture fails when wm is inactive and succeeds when wm is active.
TEST_F(DisplayTest, SetCaptureFromWindowManager) {
  const int64_t display_id = platform_screen().AddDisplay();
  AddWindowManager(window_server(), kTestId1);
  AddWindowManager(window_server(), kTestId2);
  window_server()->user_id_tracker()->SetActiveUserId(kTestId1);
  ASSERT_EQ(1u, display_manager()->displays().size());
  Display* display = display_manager()->GetDisplayById(display_id);
  WindowManagerState* wms_for_id2 =
      GetWindowManagerStateForUser(display, kTestId2);
  ASSERT_TRUE(wms_for_id2);
  EXPECT_FALSE(wms_for_id2->IsActive());

  // Create a child of the root that we can set capture on.
  WindowTree* tree = wms_for_id2->window_tree();
  ClientWindowId child_window_id;
  ASSERT_TRUE(NewWindowInTree(tree, &child_window_id));

  WindowTreeTestApi(tree).EnableCapture();

  // SetCapture() should fail for user id2 as it is inactive.
  EXPECT_FALSE(tree->SetCapture(child_window_id));

  // Make the second user active and verify capture works.
  window_server()->user_id_tracker()->SetActiveUserId(kTestId2);
  EXPECT_TRUE(wms_for_id2->IsActive());
  EXPECT_TRUE(tree->SetCapture(child_window_id));
}

TEST_F(DisplayTest, FocusFailsForInactiveUser) {
  const int64_t display_id = platform_screen().AddDisplay();
  AddWindowManager(window_server(), kTestId1);
  TestWindowTreeClient* window_tree_client1 =
      window_server_delegate()->last_client();
  ASSERT_TRUE(window_tree_client1);
  AddWindowManager(window_server(), kTestId2);
  window_server()->user_id_tracker()->SetActiveUserId(kTestId1);
  ASSERT_EQ(1u, display_manager()->displays().size());
  Display* display = display_manager()->GetDisplayById(display_id);
  WindowManagerState* wms_for_id2 =
      GetWindowManagerStateForUser(display, kTestId2);
  wms_for_id2->window_tree()->AddActivationParent(
      ClientWindowIdForFirstRoot(wms_for_id2->window_tree()));
  ASSERT_TRUE(wms_for_id2);
  EXPECT_FALSE(wms_for_id2->IsActive());
  ClientWindowId child2_id;
  NewWindowInTree(wms_for_id2->window_tree(), &child2_id);

  // Focus should fail for windows in inactive window managers.
  EXPECT_FALSE(wms_for_id2->window_tree()->SetFocus(child2_id));

  // Focus should succeed for the active window manager.
  WindowManagerState* wms_for_id1 =
      GetWindowManagerStateForUser(display, kTestId1);
  ASSERT_TRUE(wms_for_id1);
  wms_for_id1->window_tree()->AddActivationParent(
      ClientWindowIdForFirstRoot(wms_for_id1->window_tree()));
  ClientWindowId child1_id;
  NewWindowInTree(wms_for_id1->window_tree(), &child1_id);
  EXPECT_TRUE(wms_for_id1->IsActive());
  EXPECT_TRUE(wms_for_id1->window_tree()->SetFocus(child1_id));
}

// Verifies a single tree is used for multiple displays.
TEST_F(DisplayTest, MultipleDisplays) {
  platform_screen().AddDisplay();
  platform_screen().AddDisplay();
  AddWindowManager(window_server(), kTestId1);
  window_server()->user_id_tracker()->SetActiveUserId(kTestId1);
  ASSERT_EQ(1u, window_server_delegate()->bindings()->size());
  TestWindowTreeBinding* window_tree_binding =
      (*window_server_delegate()->bindings())[0];
  WindowTree* tree = window_tree_binding->tree();
  ASSERT_EQ(2u, tree->roots().size());
  std::set<const ServerWindow*> roots = tree->roots();
  auto it = roots.begin();
  ServerWindow* root1 = tree->GetWindow((*it)->id());
  ++it;
  ServerWindow* root2 = tree->GetWindow((*it)->id());
  ASSERT_NE(root1, root2);
  Display* display1 = tree->GetDisplay(root1);
  WindowManagerState* display1_wms =
      display1->GetWindowManagerDisplayRootForUser(kTestId1)
          ->window_manager_state();
  Display* display2 = tree->GetDisplay(root2);
  WindowManagerState* display2_wms =
      display2->GetWindowManagerDisplayRootForUser(kTestId1)
          ->window_manager_state();
  EXPECT_EQ(display1_wms->window_tree(), display2_wms->window_tree());
}

// Assertions around destroying a secondary display.
TEST_F(DisplayTest, DestroyingDisplayDoesntDelete) {
  AddWindowManager(window_server(), kTestId1);
  platform_screen().AddDisplay();
  const int64_t secondary_display_id = platform_screen().AddDisplay();
  window_server()->user_id_tracker()->SetActiveUserId(kTestId1);
  ASSERT_EQ(1u, window_server_delegate()->bindings()->size());
  WindowTree* tree = (*window_server_delegate()->bindings())[0]->tree();
  ASSERT_EQ(2u, tree->roots().size());
  Display* secondary_display =
      display_manager()->GetDisplayById(secondary_display_id);
  ASSERT_TRUE(secondary_display);
  bool secondary_root_destroyed = false;
  ServerWindow* secondary_root = GetRootOnDisplay(tree, secondary_display);
  ASSERT_TRUE(secondary_root);
  ServerWindowDestructionObserver observer(secondary_root,
                                           &secondary_root_destroyed);
  ClientWindowId secondary_root_id =
      ClientWindowIdForWindow(tree, secondary_root);
  TestWindowTreeClient* tree_client =
      static_cast<TestWindowTreeClient*>(tree->client());
  tree_client->tracker()->changes()->clear();
  TestWindowManager* test_window_manager =
      window_server_delegate()->last_binding()->window_manager();
  EXPECT_FALSE(test_window_manager->got_display_removed());
  platform_screen().RemoveDisplay(secondary_display_id);

  // Destroying the display should result in the following:
  // . The WindowManager should be told it was removed with the right id.
  EXPECT_TRUE(test_window_manager->got_display_removed());
  EXPECT_EQ(secondary_display_id, test_window_manager->display_removed_id());
  EXPECT_FALSE(secondary_root_destroyed);
  // The window should still be valid on the server side.
  ASSERT_TRUE(tree->GetWindowByClientId(secondary_root_id));
  // No changes.
  ASSERT_EQ(0u, tree_client->tracker()->changes()->size());

  // The window should be destroyed when the client says so.
  ASSERT_TRUE(tree->DeleteWindow(secondary_root_id));
  EXPECT_TRUE(secondary_root_destroyed);
}

}  // namespace test
}  // namespace ws
}  // namespace ui
