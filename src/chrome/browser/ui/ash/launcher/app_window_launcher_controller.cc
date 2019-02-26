// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/launcher/app_window_launcher_controller.h"

#include "ash/public/cpp/shelf_model.h"
#include "ash/shell.h"
#include "chrome/browser/ui/ash/launcher/app_window_launcher_item_controller.h"
#include "chrome/browser/ui/ash/launcher/chrome_launcher_controller.h"
#include "ui/wm/public/activation_client.h"

AppWindowLauncherController::AppWindowLauncherController(
    ChromeLauncherController* owner)
    : owner_(owner) {
  // TODO: this doesn't work in mash: https://crbug.com/826386 .
  if (ash::Shell::HasInstance() && ash::Shell::Get()->GetPrimaryRootWindow()) {
    activation_client_ =
        wm::GetActivationClient(ash::Shell::Get()->GetPrimaryRootWindow());
    if (activation_client_)
      activation_client_->AddObserver(this);
  }
  owner->shelf_model()->AddObserver(this);
}

AppWindowLauncherController::~AppWindowLauncherController() {
  owner()->shelf_model()->RemoveObserver(this);

  if (activation_client_)
    activation_client_->RemoveObserver(this);
}

void AppWindowLauncherController::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* new_active,
    aura::Window* old_active) {
  // Make the newly active window the active (first) entry in the controller.
  AppWindowLauncherItemController* new_controller =
      ControllerForWindow(new_active);
  if (new_controller)
    new_controller->SetActiveWindow(new_active);

  // Mark the old active window's launcher item as running (if different).
  AppWindowLauncherItemController* old_controller =
      ControllerForWindow(old_active);
  if (old_controller && old_controller != new_controller)
    owner_->SetItemStatus(old_controller->shelf_id(), ash::STATUS_RUNNING);
}

void AppWindowLauncherController::ShelfItemDelegateChanged(
    const ash::ShelfID& id,
    ash::ShelfItemDelegate* old_delegate,
    ash::ShelfItemDelegate* delegate) {
  if (!old_delegate)
    return;
  // Notify the LauncherController that its delegate might be destroyed and
  // cache needs to be updated. See crbug.com/770005
  OnItemDelegateDiscarded(old_delegate);
}
