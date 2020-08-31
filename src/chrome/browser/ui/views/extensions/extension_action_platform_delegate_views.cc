// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_action_platform_delegate_views.h"

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/accelerator_priority.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view_delegate_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/extensions/command.h"
#include "extensions/browser/extension_action.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "ui/views/view.h"

using extensions::ActionInfo;

// static
std::unique_ptr<ExtensionActionPlatformDelegate>
ExtensionActionPlatformDelegate::Create(
    ExtensionActionViewController* controller) {
  return base::WrapUnique(new ExtensionActionPlatformDelegateViews(controller));
}

ExtensionActionPlatformDelegateViews::ExtensionActionPlatformDelegateViews(
    ExtensionActionViewController* controller)
    : controller_(controller) {
  command_service_observer_.Add(
      extensions::CommandService::Get(controller_->browser()->profile()));
}

ExtensionActionPlatformDelegateViews::~ExtensionActionPlatformDelegateViews() {
  UnregisterCommand(false);
}

void ExtensionActionPlatformDelegateViews::RegisterCommand() {
  // If we've already registered, do nothing.
  if (action_keybinding_.get())
    return;

  extensions::Command extension_command;
  views::FocusManager* focus_manager =
      GetDelegateViews()->GetFocusManagerForAccelerator();
  if (focus_manager && controller_->GetExtensionCommand(&extension_command)) {
    action_keybinding_.reset(
        new ui::Accelerator(extension_command.accelerator()));
    focus_manager->RegisterAccelerator(*action_keybinding_,
                                       kExtensionAcceleratorPriority, this);
  }
}

void ExtensionActionPlatformDelegateViews::ShowPopup(
    std::unique_ptr<extensions::ExtensionViewHost> host,
    bool grant_tab_permissions,
    ExtensionActionViewController::PopupShowAction show_action) {
  // TOP_RIGHT is correct for both RTL and LTR, because the views platform
  // performs the flipping in RTL cases.
  views::BubbleBorder::Arrow arrow = views::BubbleBorder::TOP_RIGHT;

  ExtensionPopup::ShowAction popup_show_action =
      show_action == ExtensionActionViewController::SHOW_POPUP ?
          ExtensionPopup::SHOW : ExtensionPopup::SHOW_AND_INSPECT;
  ExtensionPopup::ShowPopup(std::move(host),
                            GetDelegateViews()->GetReferenceButtonForPopup(),
                            arrow, popup_show_action);
}

void ExtensionActionPlatformDelegateViews::ShowContextMenu() {
  views::View* view = GetDelegateViews()->GetAsView();
  view->context_menu_controller()->ShowContextMenuForView(
      view, view->GetKeyboardContextMenuLocation(), ui::MENU_SOURCE_NONE);
}

void ExtensionActionPlatformDelegateViews::OnExtensionCommandAdded(
    const std::string& extension_id,
    const extensions::Command& command) {
  if (extension_id != controller_->extension()->id())
    return;  // Not this action's extension.

  if (command.command_name() !=
          extensions::manifest_values::kBrowserActionCommandEvent &&
      command.command_name() !=
          extensions::manifest_values::kPageActionCommandEvent) {
    // Not an action-related command.
    return;
  }

  RegisterCommand();
}

void ExtensionActionPlatformDelegateViews::OnExtensionCommandRemoved(
    const std::string& extension_id,
    const extensions::Command& command) {
  if (extension_id != controller_->extension()->id())
    return;

  if (command.command_name() !=
          extensions::manifest_values::kBrowserActionCommandEvent &&
      command.command_name() !=
          extensions::manifest_values::kPageActionCommandEvent) {
    // Not an action-related command.
    return;
  }

  UnregisterCommand(/*only_if_removed=*/true);
}

void ExtensionActionPlatformDelegateViews::OnCommandServiceDestroying() {
  command_service_observer_.RemoveAll();
}

bool ExtensionActionPlatformDelegateViews::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  DCHECK(controller_->CanHandleAccelerators());

  if (controller_->IsShowingPopup()) {
    controller_->HidePopup();
  } else {
    controller_->ExecuteAction(
        true, ToolbarActionViewController::InvocationSource::kCommand);
  }

  return true;
}

bool ExtensionActionPlatformDelegateViews::CanHandleAccelerators() const {
  return controller_->CanHandleAccelerators();
}

void ExtensionActionPlatformDelegateViews::UnregisterCommand(
    bool only_if_removed) {
  views::FocusManager* focus_manager =
      GetDelegateViews()->GetFocusManagerForAccelerator();
  if (!focus_manager || !action_keybinding_.get())
    return;

  // If |only_if_removed| is true, it means that we only need to unregister
  // ourselves as an accelerator if the command was removed. Otherwise, we need
  // to unregister ourselves no matter what (likely because we are shutting
  // down).
  extensions::Command extension_command;
  if (!only_if_removed ||
      !controller_->GetExtensionCommand(&extension_command)) {
    focus_manager->UnregisterAccelerator(*action_keybinding_, this);
    action_keybinding_.reset();
  }
}

ToolbarActionViewDelegateViews*
ExtensionActionPlatformDelegateViews::GetDelegateViews() const {
  return static_cast<ToolbarActionViewDelegateViews*>(
      controller_->view_delegate());
}
