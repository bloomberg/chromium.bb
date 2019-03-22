// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_EXTENSION_APP_WINDOW_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_EXTENSION_APP_WINDOW_LAUNCHER_CONTROLLER_H_

#include <list>
#include <map>
#include <string>

#include "ash/public/cpp/shelf_types.h"
#include "base/macros.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_controller.h"
#include "extensions/browser/app_window/app_window_registry.h"
#include "ui/aura/window_observer.h"
#include "ui/views/widget/widget_observer.h"

namespace aura {
class Window;
}

namespace extensions {
class AppWindow;
}

class ChromeLauncherController;
class ExtensionAppWindowLauncherItemController;

// AppWindowLauncherController observes the app window registry and the
// aura window manager. It handles adding and removing launcher items from
// ChromeLauncherController.
//
// This uses WidgetObserver to observe activation changes rather than
// wm::ActivationChangeObserver (inherited from the superclass). WidgetObserver
// works with mash, where as wm::ActivationChangeObserver only works in the
// ash process. See https://crbug.com/826386 for details.
class ExtensionAppWindowLauncherController
    : public AppWindowLauncherController,
      public extensions::AppWindowRegistry::Observer,
      public aura::WindowObserver,
      public views::WidgetObserver {
 public:
  explicit ExtensionAppWindowLauncherController(
      ChromeLauncherController* owner);
  ~ExtensionAppWindowLauncherController() override;

  // AppWindowLauncherController:
  AppWindowLauncherItemController* ControllerForWindow(
      aura::Window* window) override;
  void OnItemDelegateDiscarded(ash::ShelfItemDelegate* delegate) override;
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // Overridden from AppWindowRegistry::Observer:
  void OnAppWindowAdded(extensions::AppWindow* app_window) override;
  void OnAppWindowShown(extensions::AppWindow* app_window,
                        bool was_hidden) override;
  void OnAppWindowHidden(extensions::AppWindow* app_window) override;

  // Overriden from aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // Overriden from views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;

 protected:
  // Registers an app window with the shelf and this object.
  void RegisterApp(extensions::AppWindow* app_window);

  // Unregisters an app window with the shelf and this object.
  void UnregisterApp(aura::Window* window);

  // Check if a given window is known to the launcher controller.
  bool IsRegisteredApp(aura::Window* window);

 private:
  using AppControllerMap =
      std::map<ash::ShelfID, ExtensionAppWindowLauncherItemController*>;

  // The AppWindowRegistry for the active user (represented by |owner()|).
  extensions::AppWindowRegistry* registry_;

  // Map of shelf id to controller.
  AppControllerMap app_controller_map_;

  // Map of aura::Windows to shelf ids.
  std::map<aura::Window*, ash::ShelfID> window_to_shelf_id_map_;

  // If the active Widget is an extension, this is its ShelfID, otherwise this
  // is empty.
  ash::ShelfID active_shelf_id_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionAppWindowLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_EXTENSION_APP_WINDOW_LAUNCHER_CONTROLLER_H_
