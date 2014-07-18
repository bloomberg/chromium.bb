// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_H_

#include "ui/views/controls/menu/menu_runner_impl_interface.h"

#include <set>

#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/views/controls/menu/menu_controller_delegate.h"

namespace views {

class MenuController;
class MenuDelegate;
class MenuItemView;

namespace internal {

// A menu runner implementation that uses views::MenuItemView to show a menu.
class MenuRunnerImpl : public MenuRunnerImplInterface,
                       public MenuControllerDelegate {
 public:
  explicit MenuRunnerImpl(MenuItemView* menu);

  virtual bool IsRunning() const OVERRIDE;
  virtual void Release() OVERRIDE;
  virtual MenuRunner::RunResult RunMenuAt(Widget* parent,
                                          MenuButton* button,
                                          const gfx::Rect& bounds,
                                          MenuAnchorPosition anchor,
                                          int32 run_types) OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual base::TimeDelta GetClosingEventTime() const OVERRIDE;

  // MenuControllerDelegate:
  virtual void DropMenuClosed(NotifyType type, MenuItemView* menu) OVERRIDE;
  virtual void SiblingMenuCreated(MenuItemView* menu) OVERRIDE;

 private:
  virtual ~MenuRunnerImpl();

  // Cleans up after the menu is no longer showing. |result| is the menu that
  // the user selected, or NULL if nothing was selected.
  MenuRunner::RunResult MenuDone(MenuItemView* result, int mouse_event_flags);

  // Returns true if mnemonics should be shown in the menu.
  bool ShouldShowMnemonics(MenuButton* button);

  // The menu. We own this. We don't use scoped_ptr as the destructor is
  // protected and we're a friend.
  MenuItemView* menu_;

  // Any sibling menus. Does not include |menu_|. We own these too.
  std::set<MenuItemView*> sibling_menus_;

  // Created and set as the delegate of the MenuItemView if Release() is
  // invoked.  This is done to make sure the delegate isn't notified after
  // Release() is invoked. We do this as we assume the delegate is no longer
  // valid if MenuRunner has been deleted.
  scoped_ptr<MenuDelegate> empty_delegate_;

  // Are we in run waiting for it to return?
  bool running_;

  // Set if |running_| and Release() has been invoked.
  bool delete_after_run_;

  // Are we running for a drop?
  bool for_drop_;

  // The controller.
  MenuController* controller_;

  // Do we own the controller?
  bool owns_controller_;

  // The timestamp of the event which closed the menu - or 0.
  base::TimeDelta closing_event_time_;

  // Used to detect deletion of |this| when notifying delegate of success.
  base::WeakPtrFactory<MenuRunnerImpl> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MenuRunnerImpl);
};

}  // namespace internal
}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_RUNNER_IMPL_H_
