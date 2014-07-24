// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/menu_runner_impl.h"

#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/menu/menu_controller.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_runner_impl_adapter.h"
#include "ui/views/widget/widget.h"

#if defined(OS_WIN)
#include "base/win/win_util.h"
#endif

namespace views {
namespace internal {

#if !defined(OS_MACOSX)
MenuRunnerImplInterface* MenuRunnerImplInterface::Create(
    ui::MenuModel* menu_model,
    int32 run_types) {
  return new MenuRunnerImplAdapter(menu_model);
}
#endif

MenuRunnerImpl::MenuRunnerImpl(MenuItemView* menu)
    : menu_(menu),
      running_(false),
      delete_after_run_(false),
      for_drop_(false),
      controller_(NULL),
      owns_controller_(false),
      closing_event_time_(base::TimeDelta()),
      weak_factory_(this) {
}

bool MenuRunnerImpl::IsRunning() const {
  return running_;
}

void MenuRunnerImpl::Release() {
  if (running_) {
    if (delete_after_run_)
      return;  // We already canceled.

    // The menu is running a nested message loop, we can't delete it now
    // otherwise the stack would be in a really bad state (many frames would
    // have deleted objects on them). Instead cancel the menu, when it returns
    // Holder will delete itself.
    delete_after_run_ = true;

    // Swap in a different delegate. That way we know the original MenuDelegate
    // won't be notified later on (when it's likely already been deleted).
    if (!empty_delegate_.get())
      empty_delegate_.reset(new MenuDelegate());
    menu_->set_delegate(empty_delegate_.get());

    DCHECK(controller_);
    // Release is invoked when MenuRunner is destroyed. Assume this is happening
    // because the object referencing the menu has been destroyed and the menu
    // button is no longer valid.
    controller_->Cancel(MenuController::EXIT_DESTROYED);
  } else {
    delete this;
  }
}

MenuRunner::RunResult MenuRunnerImpl::RunMenuAt(Widget* parent,
                                                MenuButton* button,
                                                const gfx::Rect& bounds,
                                                MenuAnchorPosition anchor,
                                                int32 run_types) {
  closing_event_time_ = base::TimeDelta();
  if (running_) {
    // Ignore requests to show the menu while it's already showing. MenuItemView
    // doesn't handle this very well (meaning it crashes).
    return MenuRunner::NORMAL_EXIT;
  }

  MenuController* controller = MenuController::GetActiveInstance();
  if (controller) {
    if ((run_types & MenuRunner::IS_NESTED) != 0) {
      if (!controller->IsBlockingRun()) {
        controller->CancelAll();
        controller = NULL;
      }
    } else {
      // There's some other menu open and we're not nested. Cancel the menu.
      controller->CancelAll();
      if ((run_types & MenuRunner::FOR_DROP) == 0) {
        // We can't open another menu, otherwise the message loop would become
        // twice nested. This isn't necessarily a problem, but generally isn't
        // expected.
        return MenuRunner::NORMAL_EXIT;
      }
      // Drop menus don't block the message loop, so it's ok to create a new
      // MenuController.
      controller = NULL;
    }
  }

  running_ = true;
  for_drop_ = (run_types & MenuRunner::FOR_DROP) != 0;
  bool has_mnemonics =
      (run_types & MenuRunner::HAS_MNEMONICS) != 0 && !for_drop_;
  owns_controller_ = false;
  if (!controller) {
    // No menus are showing, show one.
    ui::NativeTheme* theme =
        parent ? parent->GetNativeTheme() : ui::NativeTheme::instance();
    controller = new MenuController(theme, !for_drop_, this);
    owns_controller_ = true;
  }
  controller->set_is_combobox((run_types & MenuRunner::COMBOBOX) != 0);
  controller_ = controller;
  menu_->set_controller(controller_);
  menu_->PrepareForRun(owns_controller_,
                       has_mnemonics,
                       !for_drop_ && ShouldShowMnemonics(button));

  // Run the loop.
  int mouse_event_flags = 0;
  MenuItemView* result =
      controller->Run(parent,
                      button,
                      menu_,
                      bounds,
                      anchor,
                      (run_types & MenuRunner::CONTEXT_MENU) != 0,
                      &mouse_event_flags);
  // Get the time of the event which closed this menu.
  closing_event_time_ = controller->closing_event_time();
  if (for_drop_) {
    // Drop menus return immediately. We finish processing in DropMenuClosed.
    return MenuRunner::NORMAL_EXIT;
  }
  return MenuDone(result, mouse_event_flags);
}

void MenuRunnerImpl::Cancel() {
  if (running_)
    controller_->Cancel(MenuController::EXIT_ALL);
}

base::TimeDelta MenuRunnerImpl::GetClosingEventTime() const {
  return closing_event_time_;
}

void MenuRunnerImpl::DropMenuClosed(NotifyType type, MenuItemView* menu) {
  MenuDone(NULL, 0);

  if (type == NOTIFY_DELEGATE && menu->GetDelegate()) {
    // Delegate is null when invoked from the destructor.
    menu->GetDelegate()->DropMenuClosed(menu);
  }
}

void MenuRunnerImpl::SiblingMenuCreated(MenuItemView* menu) {
  if (menu != menu_ && sibling_menus_.count(menu) == 0)
    sibling_menus_.insert(menu);
}

MenuRunnerImpl::~MenuRunnerImpl() {
  delete menu_;
  for (std::set<MenuItemView*>::iterator i = sibling_menus_.begin();
       i != sibling_menus_.end();
       ++i)
    delete *i;
}

MenuRunner::RunResult MenuRunnerImpl::MenuDone(MenuItemView* result,
                                               int mouse_event_flags) {
  menu_->RemoveEmptyMenus();
  menu_->set_controller(NULL);

  if (owns_controller_) {
    // We created the controller and need to delete it.
    delete controller_;
    owns_controller_ = false;
  }
  controller_ = NULL;
  // Make sure all the windows we created to show the menus have been
  // destroyed.
  menu_->DestroyAllMenuHosts();
  if (delete_after_run_) {
    delete this;
    return MenuRunner::MENU_DELETED;
  }
  running_ = false;
  if (result && menu_->GetDelegate()) {
    // Executing the command may also delete this.
    base::WeakPtr<MenuRunnerImpl> ref(weak_factory_.GetWeakPtr());
    menu_->GetDelegate()->ExecuteCommand(result->GetCommand(),
                                         mouse_event_flags);
    if (!ref)
      return MenuRunner::MENU_DELETED;
  }
  return MenuRunner::NORMAL_EXIT;
}

bool MenuRunnerImpl::ShouldShowMnemonics(MenuButton* button) {
  // Show mnemonics if the button has focus or alt is pressed.
  bool show_mnemonics = button ? button->HasFocus() : false;
#if defined(OS_WIN)
  // This is only needed on Windows.
  if (!show_mnemonics)
    show_mnemonics = base::win::IsAltPressed();
#endif
  return show_mnemonics;
}

}  // namespace internal
}  // namespace views
