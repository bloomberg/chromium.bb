// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_BASE_H_
#define ASH_TEST_ASH_TEST_BASE_H_

#include <stdint.h>

#include <memory>
#include <string>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/wm/desks/desks_util.h"
#include "base/macros.h"
#include "base/threading/thread.h"
#include "components/user_manager/user_type.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/client/window_types.h"
#include "ui/aura/env.h"
#include "ui/display/display.h"

namespace aura {
class Window;
class WindowDelegate;
}  // namespace aura

namespace base {
namespace test {
class ScopedTaskEnvironment;
}
}  // namespace base

namespace chromeos {
class FakePowerManagerClient;
}

namespace display {
class Display;
class DisplayManager;

namespace test {
class DisplayManagerTestApi;
}  // namespace test
}  // namespace display

namespace gfx {
class Rect;
}

namespace ui {
namespace test {
class EventGenerator;
}
}  // namespace ui

namespace views {
class Widget;
class WidgetDelegate;
}

namespace ash {

class AppListTestHelper;
class AshTestHelper;
class Shelf;
class TestScreenshotDelegate;
class UnifiedSystemTray;
class WorkAreaInsets;

class AshTestBase : public testing::Test {
 public:
  AshTestBase();
  ~AshTestBase() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  // Returns the Shelf for the primary display.
  static Shelf* GetPrimaryShelf();

  // Returns the unified system tray on the primary display.
  static UnifiedSystemTray* GetPrimaryUnifiedSystemTray();

  // Returns WorkAreaInsets for the primary display.
  static WorkAreaInsets* GetPrimaryWorkAreaInsets();

  // AshTestBase creates a ScopedTaskEnvironment. This may not be appropriate in
  // some environments. Use this to destroy it.
  void DestroyScopedTaskEnvironment();

  // Update the display configuration as given in |display_specs|.
  // See ash::DisplayManagerTestApi::UpdateDisplay for more details.
  void UpdateDisplay(const std::string& display_specs);

  // Returns a root Window. Usually this is the active root Window, but that
  // method can return NULL sometimes, and in those cases, we fall back on the
  // primary root Window.
  aura::Window* CurrentContext();

  // Creates and shows a widget. See ash/public/cpp/shell_window_ids.h for
  // values for |container_id|.
  static std::unique_ptr<views::Widget> CreateTestWidget(
      views::WidgetDelegate* delegate = nullptr,
      int container_id = desks_util::GetActiveDeskContainerId(),
      const gfx::Rect& bounds = gfx::Rect(),
      bool show = true);

  // Creates a visible window in the appropriate container. If
  // |bounds_in_screen| is empty the window is added to the primary root
  // window, otherwise the window is added to the display matching
  // |bounds_in_screen|. |shell_window_id| is the shell window id to give to
  // the new window.
  // If |type| is WINDOW_TYPE_NORMAL this creates a views::Widget, otherwise
  // this creates an aura::Window.
  std::unique_ptr<aura::Window> CreateTestWindow(
      const gfx::Rect& bounds_in_screen = gfx::Rect(),
      aura::client::WindowType type = aura::client::WINDOW_TYPE_NORMAL,
      int shell_window_id = kShellWindowId_Invalid);

  // Creates a visible top-level window with a delegate.
  std::unique_ptr<aura::Window> CreateToplevelTestWindow(
      const gfx::Rect& bounds_in_screen = gfx::Rect(),
      int shell_window_id = kShellWindowId_Invalid);

  // Versions of the functions in aura::test:: that go through our shell
  // StackingController instead of taking a parent.
  aura::Window* CreateTestWindowInShellWithId(int id);
  aura::Window* CreateTestWindowInShellWithBounds(const gfx::Rect& bounds);
  aura::Window* CreateTestWindowInShell(SkColor color,
                                        int id,
                                        const gfx::Rect& bounds);

  // Creates a visible window parented to |parent| with the specified bounds and
  // id.
  std::unique_ptr<aura::Window> CreateChildWindow(
      aura::Window* parent,
      const gfx::Rect& bounds = gfx::Rect(),
      int shell_window_id = kShellWindowId_Invalid);

  aura::Window* CreateTestWindowInShellWithDelegate(
      aura::WindowDelegate* delegate,
      int id,
      const gfx::Rect& bounds);
  aura::Window* CreateTestWindowInShellWithDelegateAndType(
      aura::WindowDelegate* delegate,
      aura::client::WindowType type,
      int id,
      const gfx::Rect& bounds);

  // Attach |window| to the current shell's root window.
  void ParentWindowInPrimaryRootWindow(aura::Window* window);

  // Returns the EventGenerator that uses screen coordinates and works
  // across multiple displays. It creates a new generator if it
  // hasn't been created yet.
  ui::test::EventGenerator* GetEventGenerator();

  // Convenience method to return the DisplayManager.
  display::DisplayManager* display_manager();

  // Convenience method to return the FakePowerManagerClient.
  chromeos::FakePowerManagerClient* power_manager_client() const;

  // Test if moving a mouse to |point_in_screen| warps it to another
  // display.
  bool TestIfMouseWarpsAt(ui::test::EventGenerator* event_generator,
                          const gfx::Point& point_in_screen);

 protected:
  enum UserSessionBlockReason {
    FIRST_BLOCK_REASON,
    BLOCKED_BY_LOCK_SCREEN = FIRST_BLOCK_REASON,
    BLOCKED_BY_LOGIN_SCREEN,
    BLOCKED_BY_USER_ADDING_SCREEN,
    NUMBER_OF_BLOCK_REASONS
  };

  // Returns the rotation currentl active for the display |id|.
  static display::Display::Rotation GetActiveDisplayRotation(int64_t id);

  // Returns the rotation currently active for the internal display.
  static display::Display::Rotation GetCurrentInternalDisplayRotation();

  void set_start_session(bool start_session) { start_session_ = start_session; }
  void disable_provide_local_state() { provide_local_state_ = false; }

  AshTestHelper* ash_test_helper() { return ash_test_helper_.get(); }

  TestScreenshotDelegate* GetScreenshotDelegate();

  TestSessionControllerClient* GetSessionControllerClient();

  AppListTestHelper* GetAppListTestHelper();

  // Emulates an ash session that have |session_count| user sessions running.
  // Note that existing user sessions will be cleared.
  void CreateUserSessions(int session_count);

  // Simulates a user sign-in. It creates a new user session, adds it to
  // existing user sessions and makes it the active user session.
  void SimulateUserLogin(const std::string& user_email);

  // Simular to SimulateUserLogin but for a newly created user first ever login.
  void SimulateNewUserFirstLogin(const std::string& user_email);

  // Similar to SimulateUserLogin but for a guest user.
  void SimulateGuestLogin();

  // Simulates kiosk mode. |user_type| must correlate to a kiosk type user.
  void SimulateKioskMode(user_manager::UserType user_type);

  // Simulates setting height of the accessibility panel.
  // Note: Accessibility panel widget needs to be setup first.
  void SetAccessibilityPanelHeight(int panel_height);

  // Clears all user sessions and resets to the primary login screen state.
  void ClearLogin();

  // Emulates whether the active user can lock screen.
  void SetCanLockScreen(bool can_lock);

  // Emulates whether the screen should be locked automatically.
  void SetShouldLockScreenAutomatically(bool should_lock);

  // Emulates whether the user adding screen is running.
  void SetUserAddingScreenRunning(bool user_adding_screen_running);

  // Methods to emulate blocking and unblocking user session with given
  // |block_reason|.
  void BlockUserSession(UserSessionBlockReason block_reason);
  void UnblockUserSession();

  // Enable or disable the keyboard for touch and run the message loop to
  // allow observer operations to complete.
  void SetTouchKeyboardEnabled(bool enabled);

  void DisableIME();

  // Swap the primary display with the secondary.
  void SwapPrimaryDisplay();

  display::Display GetPrimaryDisplay();
  display::Display GetSecondaryDisplay();

 private:
  void CreateWindowTreeIfNecessary();

  bool setup_called_ = false;
  bool teardown_called_ = false;
  // |SetUp()| doesn't activate session if this is set to false.
  bool start_session_ = true;
  // |SetUp()| doesn't inject local-state PrefService into Shell if this is
  // set to false.
  bool provide_local_state_ = true;
  std::unique_ptr<base::test::ScopedTaskEnvironment> scoped_task_environment_;
  std::unique_ptr<AshTestHelper> ash_test_helper_;
  std::unique_ptr<ui::test::EventGenerator> event_generator_;

  DISALLOW_COPY_AND_ASSIGN(AshTestBase);
};

class NoSessionAshTestBase : public AshTestBase {
 public:
  NoSessionAshTestBase() { set_start_session(false); }
  ~NoSessionAshTestBase() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(NoSessionAshTestBase);
};

}  // namespace ash

#endif  // ASH_TEST_ASH_TEST_BASE_H_
