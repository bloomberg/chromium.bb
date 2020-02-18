// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_INTERNAL_APP_WINDOW_SHELF_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_INTERNAL_APP_WINDOW_SHELF_CONTROLLER_H_

#include <map>
#include <memory>
#include <vector>

#include "ash/public/cpp/shelf_types.h"
#include "base/macros.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_controller.h"
#include "chrome/browser/ui/settings_window_manager_observer_chromeos.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"

namespace aura {
class Window;
}

class AppWindowBase;
class ChromeLauncherController;

// A controller to manage internal app shelf items.
class InternalAppWindowShelfController : public AppWindowLauncherController,
                                         public aura::EnvObserver,
                                         public aura::WindowObserver {
 public:
  explicit InternalAppWindowShelfController(ChromeLauncherController* owner);
  ~InternalAppWindowShelfController() override;

  // AppWindowLauncherController:
  void ActiveUserChanged(const std::string& user_email) override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override;

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override;
  void OnWindowDestroying(aura::Window* window) override;

  // Creates an AppWindow and updates its AppWindowLauncherItemController by
  // |window| and |shelf_id|.
  void RegisterAppWindow(aura::Window* window, const ash::ShelfID& shelf_id);

 private:
  using AuraWindowToAppWindow =
      std::map<aura::Window*, std::unique_ptr<AppWindowBase>>;

  void AddToShelf(AppWindowBase* app_window);
  void RemoveFromShelf(AppWindowBase* app_window);

  // Removes an AppWindow from its AppWindowLauncherItemController.
  void UnregisterAppWindow(AppWindowBase* app_window);

  // AppWindowLauncherController:
  AppWindowLauncherItemController* ControllerForWindow(
      aura::Window* window) override;
  void OnItemDelegateDiscarded(ash::ShelfItemDelegate* delegate) override;

  AuraWindowToAppWindow aura_window_to_app_window_;
  std::vector<aura::Window*> observed_windows_;

  DISALLOW_COPY_AND_ASSIGN(InternalAppWindowShelfController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_INTERNAL_APP_WINDOW_SHELF_CONTROLLER_H_
