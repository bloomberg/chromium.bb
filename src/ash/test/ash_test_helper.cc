// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_helper.h"

#include <algorithm>

#include "ash/app_list/test/app_list_test_helper.h"
#include "ash/assistant/assistant_controller_impl.h"
#include "ash/assistant/test/test_assistant_service.h"
#include "ash/display/display_configuration_controller_test_api.h"
#include "ash/display/screen_ash.h"
#include "ash/host/ash_window_tree_host.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/keyboard/test_keyboard_ui.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/public/cpp/test/test_keyboard_controller_observer.h"
#include "ash/public/cpp/test/test_new_window_delegate.h"
#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/shell/toplevel_window.h"
#include "ash/shell_init_params.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/screen_layout_observer.h"
#include "ash/test/ash_test_views_delegate.h"
#include "ash/test_shell_delegate.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/run_loop.h"
#include "base/system/sys_info.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/audio/cras_audio_client.h"
#include "chromeos/dbus/power/power_policy_controller.h"
#include "device/bluetooth/bluetooth_adapter_factory.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/display/display.h"
#include "ui/display/display_switches.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/events/gesture_detection/gesture_configuration.h"
#include "ui/gfx/geometry/point.h"
#include "ui/platform_window/common/platform_window_defaults.h"
#include "ui/views/test/views_test_helper_aura.h"
#include "ui/wm/core/capture_controller.h"
#include "ui/wm/core/cursor_manager.h"
#include "ui/wm/core/focus_controller.h"

namespace ash {

namespace {
std::unique_ptr<views::TestViewsDelegate> MakeTestViewsDelegate() {
  return std::make_unique<AshTestViewsDelegate>();
}
}  // namespace

AshTestHelper::InitParams::InitParams() = default;
AshTestHelper::InitParams::InitParams(InitParams&&) = default;
AshTestHelper::InitParams::~InitParams() = default;

class AshTestHelper::BluezDBusManagerInitializer {
 public:
  BluezDBusManagerInitializer() { bluez::BluezDBusManager::InitializeFake(); }
  ~BluezDBusManagerInitializer() {
    device::BluetoothAdapterFactory::Shutdown();
    bluez::BluezDBusManager::Shutdown();
  }
};

class AshTestHelper::PowerPolicyControllerInitializer {
 public:
  PowerPolicyControllerInitializer() {
    chromeos::PowerPolicyController::Initialize(
        chromeos::PowerManagerClient::Get());
  }
  ~PowerPolicyControllerInitializer() {
    chromeos::PowerPolicyController::Shutdown();
  }
};

AshTestHelper::AshTestHelper(ConfigType config_type,
                             ui::ContextFactory* context_factory)
    : AuraTestHelper(context_factory, config_type == kUnitTest),
      config_type_(config_type) {
  views::ViewsTestHelperAura::SetFallbackTestViewsDelegateFactory(
      &MakeTestViewsDelegate);

  // TODO(jamescook): Can we do this without changing command line?
  // Use the origin (1,1) so that it doesn't overlap with the native mouse
  // cursor.
  if (!base::SysInfo::IsRunningOnChromeOS() &&
      !command_line_->GetProcessCommandLine()->HasSwitch(
          ::switches::kHostWindowBounds)) {
    // TODO(oshima): Disable native events instead of adding offset.
    command_line_->GetProcessCommandLine()->AppendSwitchASCII(
        ::switches::kHostWindowBounds, "10+10-800x600");
  }

  if (config_type_ == kUnitTest)
    TabletModeController::SetUseScreenshotForTest(false);

  if (config_type_ != kShell)
    display::ResetDisplayIdForTest();

  chromeos::CrasAudioClient::InitializeFake();
  // Create CrasAudioHandler for testing since g_browser_process is not
  // created in AshTestBase tests.
  chromeos::CrasAudioHandler::InitializeForTesting();

  // Reset the global state for the cursor manager. This includes the
  // last cursor visibility state, etc.
  wm::CursorManager::ResetCursorVisibilityStateForTest();

  // Clears the saved state so that test doesn't use on the wrong
  // default state.
  shell::ToplevelWindow::ClearSavedStateForTest();
}

AshTestHelper::~AshTestHelper() {
  if (app_list_test_helper_)
    TearDown();

  // Ensure the next test starts with a null display::Screen.  This must be done
  // here instead of in TearDown() since some tests test access to the Screen
  // after the shell shuts down (which they use TearDown() to trigger).
  ScreenAsh::DeleteScreenForShutdown();

  // This should never have a meaningful effect, since either there is no
  // ViewsTestHelperAura instance or the instance is currently in its
  // destructor.
  views::ViewsTestHelperAura::SetFallbackTestViewsDelegateFactory(nullptr);
}

void AshTestHelper::SetUp() {
  SetUp(InitParams());
}

void AshTestHelper::TearDown() {
  // The AppListTestHelper holds a pointer to the AppListController the Shell
  // owns, so shut the test helper down first.
  app_list_test_helper_.reset();

  Shell::DeleteInstance();
  // Suspend the tear down until all resources are returned via
  // CompositorFrameSinkClient::ReclaimResources()
  base::RunLoop().RunUntilIdle();

  chromeos::CrasAudioHandler::Shutdown();
  chromeos::CrasAudioClient::Shutdown();

  // The PowerPolicyController holds a pointer to the PowerManagementClient, so
  // shut the controller down first.
  power_policy_controller_initializer_.reset();
  chromeos::PowerManagerClient::Shutdown();

  TabletModeController::SetUseScreenshotForTest(true);

  // Destroy all owned objects to prevent tests from depending on their state
  // after this returns.
  test_keyboard_controller_observer_.reset();
  session_controller_client_.reset();
  test_views_delegate_.reset();
  new_window_delegate_.reset();
  bluez_dbus_manager_initializer_.reset();
  system_tray_client_.reset();
  assistant_service_.reset();
  notifier_settings_controller_.reset();
  prefs_provider_.reset();
  statistics_provider_.reset();
  command_line_.reset();

  AuraTestHelper::TearDown();
}

aura::Window* AshTestHelper::GetContext() {
  aura::Window* root_window = Shell::GetRootWindowForNewWindows();
  if (!root_window)
    root_window = Shell::GetPrimaryRootWindow();
  DCHECK(root_window);
  return root_window;
}

aura::WindowTreeHost* AshTestHelper::GetHost() {
  auto* manager = Shell::Get()->window_tree_host_manager();
  const int64_t id = manager->GetPrimaryDisplayId();
  return manager->GetAshWindowTreeHostForDisplayId(id)->AsWindowTreeHost();
}

aura::TestScreen* AshTestHelper::GetTestScreen() {
  // If a test needs this, we may need to refactor TestScreen such that its
  // methods can operate atop some sort of real screen/host/display, and hook
  // them to the ones provided by the shell.  For now, not bothering.
  NOTIMPLEMENTED();
  return nullptr;
}

aura::client::FocusClient* AshTestHelper::GetFocusClient() {
  return Shell::Get()->focus_controller();
}

aura::client::CaptureClient* AshTestHelper::GetCaptureClient() {
  return wm::CaptureController::Get();
}

void AshTestHelper::SetUp(InitParams init_params) {
  // This block of objects are conditionally initialized here rather than in the
  // constructor to make it easier for test classes to override them.
  if (!bluez::BluezDBusManager::IsInitialized()) {
    bluez_dbus_manager_initializer_ =
        std::make_unique<BluezDBusManagerInitializer>();
  }
  if (!chromeos::PowerManagerClient::Get())
    chromeos::PowerManagerClient::InitializeFake();
  if (!chromeos::PowerPolicyController::IsInitialized()) {
    power_policy_controller_initializer_ =
        std::make_unique<PowerPolicyControllerInitializer>();
  }
  if (!NewWindowDelegate::GetInstance())
    new_window_delegate_ = std::make_unique<TestNewWindowDelegate>();
  if (!views::ViewsDelegate::GetInstance())
    test_views_delegate_ = MakeTestViewsDelegate();

  ShellInitParams shell_init_params;
  shell_init_params.delegate = std::move(init_params.delegate);
  if (!shell_init_params.delegate)
    shell_init_params.delegate = std::make_unique<TestShellDelegate>();
  shell_init_params.context_factory = GetContextFactory();
  shell_init_params.local_state = init_params.local_state;
  shell_init_params.keyboard_ui_factory =
      std::make_unique<TestKeyboardUIFactory>();
  Shell::CreateInstance(std::move(shell_init_params));
  Shell* shell = Shell::Get();

  // Cursor is visible by default in tests.
  shell->cursor_manager()->ShowCursor();

  shell->assistant_controller()->SetAssistant(
      assistant_service_->CreateRemoteAndBind());

  shell->system_tray_model()->SetClient(system_tray_client_.get());

  session_controller_client_ = std::make_unique<TestSessionControllerClient>(
      shell->session_controller(), prefs_provider_.get());
  session_controller_client_->InitializeAndSetClient();
  if (init_params.start_session)
    session_controller_client_->CreatePredefinedUserSessions(1);

  // Requires the AppListController the Shell creates.
  app_list_test_helper_ = std::make_unique<AppListTestHelper>();

  Shell::GetPrimaryRootWindow()->Show();
  Shell::GetPrimaryRootWindow()->GetHost()->Show();

  if (config_type_ == kShell) {
    shell->wallpaper_controller()->ShowDefaultWallpaperForTesting();
    return;
  }

  // Don't change the display size due to host size resize.
  display::test::DisplayManagerTestApi(shell->display_manager())
      .DisableChangeDisplayUponHostResize();

  // Create the test keyboard controller observer to respond to
  // OnLoadKeyboardContentsRequested().
  test_keyboard_controller_observer_ =
      std::make_unique<TestKeyboardControllerObserver>(
          shell->keyboard_controller());

  if (config_type_ != kUnitTest)
    return;

  // Tests that change the display configuration generally don't care about the
  // notifications and the popup UI can interfere with things like cursors.
  shell->screen_layout_observer()->set_show_notifications_for_testing(false);

  // Disable display change animations in unit tests.
  DisplayConfigurationControllerTestApi(
      shell->display_configuration_controller())
      .SetDisplayAnimator(false);

  // Remove the app dragging animations delay for testing purposes.
  shell->overview_controller()->set_delayed_animation_task_delay_for_test(
      base::TimeDelta());

  // Tests expect empty wallpaper.
  shell->wallpaper_controller()->CreateEmptyWallpaperForTesting();

  // Move the mouse cursor to far away so that native events don't interfere
  // with test expectations.
  Shell::GetPrimaryRootWindow()->MoveCursorTo(gfx::Point(-1000, -1000));
  Shell::Get()->cursor_manager()->EnableMouseEvents();

  // Changing GestureConfiguration shouldn't make tests fail. These values
  // prevent unexpected events from being generated during tests. Such as
  // delayed events which create race conditions on slower tests.
  ui::GestureConfiguration* gesture_config =
      ui::GestureConfiguration::GetInstance();
  gesture_config->set_max_touch_down_duration_for_click_in_ms(800);
  gesture_config->set_long_press_time_in_ms(1000);
  gesture_config->set_max_touch_move_in_pixels_for_click(5);
}

display::Display AshTestHelper::GetSecondaryDisplay() const {
  return display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .GetSecondaryDisplay();
}

}  // namespace ash
