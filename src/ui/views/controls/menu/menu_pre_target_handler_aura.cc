// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_pre_target_handler_aura.h"

#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/ui_base_features.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/public/activation_client.h"

namespace views {

namespace {

aura::Window* GetOwnerRootWindow(views::Widget* owner) {
  return owner ? owner->GetNativeWindow()->GetRootWindow() : nullptr;
}

}  // namespace

MenuPreTargetHandlerAura::MenuPreTargetHandlerAura(MenuController* controller,
                                                   Widget* owner)
    : controller_(controller), root_(GetOwnerRootWindow(owner)) {
  if (root_) {
    root_->env()->AddPreTargetHandler(this, ui::EventTarget::Priority::kSystem);
    wm::GetActivationClient(root_)->AddObserver(this);
    root_->AddObserver(this);
  } else {
    // TODO(mukai): check if this code path can run in ChromeOS and find the
    // solution for SingleProcessMash.
    if (features::IsUsingWindowService()) {
      LOG(WARNING) << "MenuPreTargetHandlerAura is created without owner "
                   << "widget. This may not work well in SingleProcessMash.";
    }
    aura::Env::GetInstance()->AddPreTargetHandler(
        this, ui::EventTarget::Priority::kSystem);
  }
}

MenuPreTargetHandlerAura::~MenuPreTargetHandlerAura() {
  if (root_)
    root_->env()->RemovePreTargetHandler(this);
  else
    aura::Env::GetInstance()->RemovePreTargetHandler(this);
  Cleanup();
}

void MenuPreTargetHandlerAura::OnWindowActivated(
    wm::ActivationChangeObserver::ActivationReason reason,
    aura::Window* gained_active,
    aura::Window* lost_active) {
  if (!controller_->drag_in_progress())
    controller_->CancelAll();
}

void MenuPreTargetHandlerAura::OnWindowDestroying(aura::Window* window) {
  Cleanup();
}

void MenuPreTargetHandlerAura::OnCancelMode(ui::CancelModeEvent* event) {
  controller_->CancelAll();
}

void MenuPreTargetHandlerAura::OnKeyEvent(ui::KeyEvent* event) {
  controller_->OnWillDispatchKeyEvent(event);
}

void MenuPreTargetHandlerAura::Cleanup() {
  if (!root_)
    return;
  // The ActivationClient may have been destroyed by the time we get here.
  wm::ActivationClient* client = wm::GetActivationClient(root_);
  if (client)
    client->RemoveObserver(this);
  root_->RemoveObserver(this);
  root_ = nullptr;
}

// static
std::unique_ptr<MenuPreTargetHandler> MenuPreTargetHandler::Create(
    MenuController* controller,
    Widget* owner) {
  return std::make_unique<MenuPreTargetHandlerAura>(controller, owner);
}

}  // namespace views
