// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_service_app_window_crostini_tracker.h"

#include "ash/public/cpp/multi_user_window_manager.h"
#include "chrome/browser/chromeos/crostini/crostini_force_close_watcher.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service.h"
#include "chrome/browser/chromeos/crostini/crostini_registry_service_factory.h"
#include "chrome/browser/chromeos/crostini/crostini_util.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "chrome/browser/ui/ash/launcher/shelf_spinner_controller.h"
#include "chrome/browser/ui/ash/multi_user/multi_user_window_manager_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/chrome_features.h"
#include "components/arc/arc_util.h"
#include "components/exo/shell_surface_util.h"
#include "components/user_manager/user_manager.h"
#include "ui/aura/window.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/wm/core/window_util.h"

namespace {

void MoveWindowFromOldDisplayToNewDisplay(aura::Window* window,
                                          display::Display& old_display,
                                          display::Display& new_display) {
  // Adjust the window size and origin in proportion to the relative size of the
  // display.
  int old_width = old_display.bounds().width();
  int new_width = new_display.bounds().width();
  int old_height = old_display.bounds().height();
  int new_height = new_display.bounds().height();
  gfx::Rect old_bounds = window->bounds();
  gfx::Rect new_bounds(old_bounds.x() * new_width / old_width,
                       old_bounds.y() * new_height / old_height,
                       old_bounds.width() * new_width / old_width,
                       old_bounds.height() * new_height / old_height);

  // Transform the bounds in display to that in screen.
  gfx::Point new_origin = new_display.bounds().origin();
  new_origin.Offset(new_bounds.x(), new_bounds.y());
  new_bounds.set_origin(new_origin);
  window->SetBoundsInScreen(new_bounds, new_display);
}

}  // namespace

AppServiceAppWindowCrostiniTracker::AppServiceAppWindowCrostiniTracker() =
    default;

AppServiceAppWindowCrostiniTracker::~AppServiceAppWindowCrostiniTracker() =
    default;

void AppServiceAppWindowCrostiniTracker::OnWindowVisibilityChanging(
    aura::Window* window,
    const std::string& shelf_app_id) {
  // Transient windows are set up after window init, so remove them here.
  // Crostini shouldn't need to know about ARC app windows.
  if (wm::GetTransientParent(window) ||
      arc::GetWindowTaskId(window) != arc::kNoTaskId ||
      plugin_vm::IsPluginVmWindow(window)) {
    return;
  }

  // Handle browser windows, such as the Crostini terminal.
  Browser* browser = chrome::FindBrowserWithWindow(window);
  if (browser)
    return;

  // Currently Crostini can only be used from the primary profile. In the
  // future, this may be replaced by some way of matching the container that
  // runs this app with the user that owns it.
  const AccountId& primary_account_id =
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();

  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(
          chromeos::ProfileHelper::Get()->GetProfileByAccountId(
              primary_account_id));

  // At this point, all remaining windows are Crostini windows. Firstly, we add
  // support for forcibly closing it. We use the registration to retrieve the
  // app's name, but this may be null in the case of apps with no associated
  // launcher entry (i.e. no .desktop file), in which case the app's name is
  // unknown.
  base::Optional<crostini::CrostiniRegistryService::Registration> registration =
      registry_service->GetRegistration(shelf_app_id);
  RegisterCrostiniWindowForForceClose(
      window, registration.has_value() ? registration->Name() : "");

  // Failed to uniquely identify the Crostini app that this window is for.
  // The spinners on the shelf have internal app IDs which are valid
  // extensions IDs. If the ID here starts with "crostini:" then it implies
  // that it has failed to identify the exact app that's starting.
  // The existing spinner that fails to be linked back should be closed,
  // otherwise it will be left on the shelf indefinetely until it is closed
  // manually by the user.
  // When the condition is triggered here, the container is up and at least
  // one app is starting. It's safe to close all the spinners since their
  // respective apps take at most another few seconds to start.
  // Work is ongoing to make this occur as infrequently as possible.
  // See https://crbug.com/854911.
  if (base::StartsWith(shelf_app_id, crostini::kCrostiniAppIdPrefix,
                       base::CompareCase::SENSITIVE)) {
    ChromeLauncherController::instance()
        ->GetShelfSpinnerController()
        ->CloseCrostiniSpinners();
  }

  // Prevent Crostini window from showing up after user switch.
  MultiUserWindowManagerHelper::GetWindowManager()->SetWindowOwner(
      window, primary_account_id);

  // Move the Crostini app window to the right display if necessary.
  int64_t display_id = crostini_app_display_.GetDisplayIdForAppId(shelf_app_id);
  if (display_id == display::kInvalidDisplayId)
    return;

  display::Display new_display;
  if (!display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id,
                                                             &new_display)) {
    return;
  }

  display::Display old_display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);

  if (new_display != old_display)
    MoveWindowFromOldDisplayToNewDisplay(window, old_display, new_display);
}

void AppServiceAppWindowCrostiniTracker::OnWindowDestroying(
    const std::string& app_id) {
  if (app_id != app_id_to_restart_)
    return;
  crostini::LaunchCrostiniApp(ChromeLauncherController::instance()->profile(),
                              app_id, display_id_to_restart_in_);
  app_id_to_restart_.clear();
}

void AppServiceAppWindowCrostiniTracker::OnAppLaunchRequested(
    const std::string& app_id,
    int64_t display_id) {
  crostini_app_display_.Register(app_id, display_id);
}

void AppServiceAppWindowCrostiniTracker::Restart(const ash::ShelfID& shelf_id,
                                                 int64_t display_id) {
  app_id_to_restart_ = shelf_id.app_id;
  display_id_to_restart_in_ = display_id;
  ChromeLauncherController::instance()->Close(shelf_id);
}

std::string AppServiceAppWindowCrostiniTracker::GetShelfAppId(
    aura::Window* window) const {
  // Transient windows are set up after window init, so remove them here.
  // Crostini shouldn't need to know about ARC app windows.
  if (wm::GetTransientParent(window) ||
      arc::GetWindowTaskId(window) != arc::kNoTaskId ||
      plugin_vm::IsPluginVmWindow(window)) {
    return std::string();
  }

  // Handle browser windows, such as the Crostini terminal.
  Browser* browser = chrome::FindBrowserWithWindow(window);
  if (browser) {
    base::Optional<std::string> app_id =
        crostini::CrostiniAppIdFromAppName(browser->app_name());
    if (!app_id)
      return std::string();
    return app_id.value();
  }

  // Currently Crostini can only be used from the primary profile. In the
  // future, this may be replaced by some way of matching the container that
  // runs this app with the user that owns it.
  const AccountId& primary_account_id =
      user_manager::UserManager::Get()->GetPrimaryUser()->GetAccountId();
  crostini::CrostiniRegistryService* registry_service =
      crostini::CrostiniRegistryServiceFactory::GetForProfile(
          chromeos::ProfileHelper::Get()->GetProfileByAccountId(
              primary_account_id));
  std::string shelf_app_id = registry_service->GetCrostiniShelfAppId(
      exo::GetShellApplicationId(window), exo::GetShellStartupId(window));
  return shelf_app_id;
}

void AppServiceAppWindowCrostiniTracker::RegisterCrostiniWindowForForceClose(
    aura::Window* window,
    const std::string& app_name) {
  if (!base::FeatureList::IsEnabled(features::kCrostiniForceClose))
    return;
  exo::ShellSurfaceBase* surface = exo::GetShellSurfaceBaseForWindow(window);
  if (!surface)
    return;
  crostini::ForceCloseWatcher::Watch(
      std::make_unique<crostini::ShellSurfaceForceCloseDelegate>(surface,
                                                                 app_name));
}
