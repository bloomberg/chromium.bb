// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_LAUNCHER_APP_WINDOW_LAUNCHER_CONTROLLER_H_
#define CHROME_BROWSER_UI_ASH_LAUNCHER_APP_WINDOW_LAUNCHER_CONTROLLER_H_

#include <string>

#include "ash/public/cpp/shelf_model_observer.h"
#include "base/macros.h"
#include "ui/wm/public/activation_change_observer.h"

class AppWindowLauncherItemController;
class ChromeLauncherController;
class Profile;

namespace aura {
class Window;
}

namespace wm {
class ActivationClient;
}

class AppWindowLauncherController : public wm::ActivationChangeObserver,
                                    public ash::ShelfModelObserver {
 public:
  ~AppWindowLauncherController() override;

  // Called by ChromeLauncherController when the active user changed and the
  // items need to be updated.
  virtual void ActiveUserChanged(const std::string& user_email) {}

  // An additional user identified by |Profile|, got added to the existing
  // session.
  virtual void AdditionalUserAddedToSession(Profile* profile) {}

  // Overriden from client::ActivationChangeObserver:
  void OnWindowActivated(wm::ActivationChangeObserver::ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  ChromeLauncherController* owner() { return owner_; }

 protected:
  explicit AppWindowLauncherController(ChromeLauncherController* owner);

  virtual AppWindowLauncherItemController* ControllerForWindow(
      aura::Window* window) = 0;

  // Called to update local caches when the item |delegate| is replaced. Note,
  // |delegate| might not belong to current launcher controller.
  virtual void OnItemDelegateDiscarded(ash::ShelfItemDelegate* delegate) = 0;

 private:
  // Unowned pointers.
  ChromeLauncherController* owner_;
  wm::ActivationClient* activation_client_ = nullptr;

  // ash::ShelfModelObserver:
  void ShelfItemDelegateChanged(const ash::ShelfID& id,
                                ash::ShelfItemDelegate* old_delegate,
                                ash::ShelfItemDelegate* delegate) override;

  DISALLOW_COPY_AND_ASSIGN(AppWindowLauncherController);
};

#endif  // CHROME_BROWSER_UI_ASH_LAUNCHER_APP_WINDOW_LAUNCHER_CONTROLLER_H_
