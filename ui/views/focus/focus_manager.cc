// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/focus/focus_manager.h"

#include <algorithm>

#include "base/auto_reset.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "ui/base/accelerators/accelerator.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/views/focus/focus_manager_delegate.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/focus/view_storage.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/view.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

namespace views {

FocusManager::FocusManager(Widget* widget, FocusManagerDelegate* delegate)
    : widget_(widget),
      delegate_(delegate),
      focused_view_(NULL),
      accelerator_manager_(new ui::AcceleratorManager),
      focus_change_reason_(kReasonDirectFocusChange),
#if defined(USE_X11)
      should_handle_menu_key_release_(false),
#endif
      is_changing_focus_(false) {
  DCHECK(widget_);
  stored_focused_view_storage_id_ =
      ViewStorage::GetInstance()->CreateStorageID();
}

FocusManager::~FocusManager() {
}

bool FocusManager::OnKeyEvent(const KeyEvent& event) {
  const int key_code = event.key_code();

#if defined(USE_X11)
  // TODO(ben): beng believes that this should be done in
  // RootWindowHosLinux for aura/linux.

  // Always reset |should_handle_menu_key_release_| unless we are handling a
  // VKEY_MENU key release event. It ensures that VKEY_MENU accelerator can only
  // be activated when handling a VKEY_MENU key release event which is preceded
  // by an un-handled VKEY_MENU key press event.
  if (key_code != ui::VKEY_MENU || event.type() != ui::ET_KEY_RELEASED)
    should_handle_menu_key_release_ = false;

  if (event.type() == ui::ET_KEY_PRESSED) {
    // VKEY_MENU is triggered by key release event.
    // FocusManager::OnKeyEvent() returns false when the key has been consumed.
    if ((key_code == ui::VKEY_MENU) &&
        (event.flags() & ~ui::EF_ALT_DOWN) == 0) {
      should_handle_menu_key_release_ = true;
      return false;
    }
    // Pass through to the rest of OnKeyEvent.
  } else if (key_code == ui::VKEY_MENU && should_handle_menu_key_release_ &&
             (event.flags() & ~ui::EF_ALT_DOWN) == 0) {
    // Trigger VKEY_MENU when only this key is pressed and released, and both
    // press and release events are not handled by others.
    ui::Accelerator accelerator(ui::VKEY_MENU, false, false, false);
    return ProcessAccelerator(accelerator);
  } else if (event.type() != ui::ET_KEY_RELEASED) {
    return false;
  }
#else
  if (event.type() != ui::ET_KEY_PRESSED && event.type() != ui::ET_KEY_RELEASED)
    return false;
#endif

  ui::Accelerator accelerator(event.key_code(),
                              event.IsShiftDown(),
                              event.IsControlDown(),
                              event.IsAltDown());
  accelerator.set_type(event.type());

  if (event.type() == ui::ET_KEY_PRESSED) {
#if defined(OS_WIN)
    // If the focused view wants to process the key event as is, let it be.
    // This is not used for linux/aura.
    if (focused_view_ && focused_view_->SkipDefaultKeyEventProcessing(event) &&
        !accelerator_manager_->HasPriorityHandler(accelerator))
      return true;
#endif

    // Intercept Tab related messages for focus traversal.
    // Note that we don't do focus traversal if the root window is not part of
    // the active window hierarchy as this would mean we have no focused view
    // and would focus the first focusable view.
#if defined(OS_WIN) && !defined(USE_AURA)
    HWND top_window = widget_->GetNativeView();
    HWND active_window = ::GetActiveWindow();
    if ((active_window == top_window || ::IsChild(active_window, top_window)) &&
        IsTabTraversalKeyEvent(event)) {
      AdvanceFocus(event.IsShiftDown());
      return false;
    }
#else
    if (IsTabTraversalKeyEvent(event)) {
      AdvanceFocus(event.IsShiftDown());
      return false;
    }
#endif

    // Intercept arrow key messages to switch between grouped views.
    if (focused_view_ && focused_view_->GetGroup() != -1 &&
        (key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN ||
         key_code == ui::VKEY_LEFT || key_code == ui::VKEY_RIGHT)) {
      bool next = (key_code == ui::VKEY_RIGHT || key_code == ui::VKEY_DOWN);
      View::Views views;
      focused_view_->parent()->GetViewsInGroup(focused_view_->GetGroup(),
                                               &views);
      View::Views::const_iterator i(
          std::find(views.begin(), views.end(), focused_view_));
      DCHECK(i != views.end());
      int index = static_cast<int>(i - views.begin());
      index += next ? 1 : -1;
      if (index < 0) {
        index = static_cast<int>(views.size()) - 1;
      } else if (index >= static_cast<int>(views.size())) {
        index = 0;
      }
      SetFocusedViewWithReason(views[index], kReasonFocusTraversal);
      return false;
    }
  }

  // Process keyboard accelerators.
  // If the key combination matches an accelerator, the accelerator is
  // triggered, otherwise the key event is processed as usual.
  if (ProcessAccelerator(accelerator)) {
    // If a shortcut was activated for this keydown message, do not propagate
    // the event further.
    return false;
  }
  return true;
}

void FocusManager::ValidateFocusedView() {
  if (focused_view_) {
    if (!ContainsView(focused_view_))
      ClearFocus();
  }
}

// Tests whether a view is valid, whether it still belongs to the window
// hierarchy of the FocusManager.
bool FocusManager::ContainsView(View* view) {
  Widget* widget = view->GetWidget();
  return widget ? widget->GetFocusManager() == this : false;
}

void FocusManager::AdvanceFocus(bool reverse) {
  View* v = GetNextFocusableView(focused_view_, reverse, false);
  // Note: Do not skip this next block when v == focused_view_.  If the user
  // tabs past the last focusable element in a webpage, we'll get here, and if
  // the TabContentsContainerView is the only focusable view (possible in
  // fullscreen mode), we need to run this block in order to cycle around to the
  // first element on the page.
  if (v) {
    v->AboutToRequestFocusFromTabTraversal(reverse);
    SetFocusedViewWithReason(v, kReasonFocusTraversal);
  }
}

void FocusManager::ClearNativeFocus() {
  // Keep the top root window focused so we get keyboard events.
  widget_->ClearNativeFocus();
}

View* FocusManager::GetNextFocusableView(View* original_starting_view,
                                         bool reverse,
                                         bool dont_loop) {
  FocusTraversable* focus_traversable = NULL;

  // Let's revalidate the focused view.
  ValidateFocusedView();

  View* starting_view = NULL;
  if (original_starting_view) {
    // Search up the containment hierarchy to see if a view is acting as
    // a pane, and wants to implement its own focus traversable to keep
    // the focus trapped within that pane.
    View* pane_search = original_starting_view;
    while (pane_search) {
      focus_traversable = pane_search->GetPaneFocusTraversable();
      if (focus_traversable) {
        starting_view = original_starting_view;
        break;
      }
      pane_search = pane_search->parent();
    }

    if (!focus_traversable) {
      if (!reverse) {
        // If the starting view has a focus traversable, use it.
        // This is the case with NativeWidgetWins for example.
        focus_traversable = original_starting_view->GetFocusTraversable();

        // Otherwise default to the root view.
        if (!focus_traversable) {
          focus_traversable =
              original_starting_view->GetWidget()->GetFocusTraversable();
          starting_view = original_starting_view;
        }
      } else {
        // When you are going back, starting view's FocusTraversable
        // should not be used.
        focus_traversable =
            original_starting_view->GetWidget()->GetFocusTraversable();
        starting_view = original_starting_view;
      }
    }
  } else {
    focus_traversable = widget_->GetFocusTraversable();
  }

  // Traverse the FocusTraversable tree down to find the focusable view.
  View* v = FindFocusableView(focus_traversable, starting_view, reverse);
  if (v) {
    return v;
  } else {
    // Let's go up in the FocusTraversable tree.
    FocusTraversable* parent_focus_traversable =
        focus_traversable->GetFocusTraversableParent();
    starting_view = focus_traversable->GetFocusTraversableParentView();
    while (parent_focus_traversable) {
      FocusTraversable* new_focus_traversable = NULL;
      View* new_starting_view = NULL;
      // When we are going backward, the parent view might gain the next focus.
      bool check_starting_view = reverse;
      v = parent_focus_traversable->GetFocusSearch()->FindNextFocusableView(
          starting_view, reverse, FocusSearch::UP,
          check_starting_view, &new_focus_traversable, &new_starting_view);

      if (new_focus_traversable) {
        DCHECK(!v);

        // There is a FocusTraversable, traverse it down.
        v = FindFocusableView(new_focus_traversable, NULL, reverse);
      }

      if (v)
        return v;

      starting_view = focus_traversable->GetFocusTraversableParentView();
      parent_focus_traversable =
          parent_focus_traversable->GetFocusTraversableParent();
    }

    // If we get here, we have reached the end of the focus hierarchy, let's
    // loop. Make sure there was at least a view to start with, to prevent
    // infinitely looping in empty windows.
    if (!dont_loop && original_starting_view) {
      // Easy, just clear the selection and press tab again.
      // By calling with NULL as the starting view, we'll start from the
      // top_root_view.
      return GetNextFocusableView(NULL, reverse, true);
    }
  }
  return NULL;
}

void FocusManager::SetFocusedViewWithReason(
    View* view, FocusChangeReason reason) {
  if (focused_view_ == view)
    return;

  AutoReset<bool> auto_changing_focus(&is_changing_focus_, true);
  // Update the reason for the focus change (since this is checked by
  // some listeners), then notify all listeners.
  focus_change_reason_ = reason;
  FOR_EACH_OBSERVER(FocusChangeListener, focus_change_listeners_,
                    OnWillChangeFocus(focused_view_, view));

  View* old_focused_view = focused_view_;
  focused_view_ = view;
  if (old_focused_view)
    old_focused_view->Blur();
  if (focused_view_)
    focused_view_->Focus();

  FOR_EACH_OBSERVER(FocusChangeListener, focus_change_listeners_,
                    OnDidChangeFocus(old_focused_view, focused_view_));
}

void FocusManager::ClearFocus() {
  SetFocusedView(NULL);
  ClearNativeFocus();
}

void FocusManager::StoreFocusedView() {
#if defined(USE_X11)
  // Forget menu key state when the window lost focus.
  should_handle_menu_key_release_ = false;
#endif
  ViewStorage* view_storage = ViewStorage::GetInstance();
  if (!view_storage) {
    // This should never happen but bug 981648 seems to indicate it could.
    NOTREACHED();
    return;
  }

  // TODO(jcivelli): when a TabContents containing a popup is closed, the focus
  // is stored twice causing an assert. We should find a better alternative than
  // removing the view from the storage explicitly.
  view_storage->RemoveView(stored_focused_view_storage_id_);

  if (!focused_view_)
    return;

  view_storage->StoreView(stored_focused_view_storage_id_, focused_view_);

  View* v = focused_view_;

  {
    // Temporarily disable notification.  ClearFocus() will set the focus to the
    // main browser window.  This extra focus bounce which happens during
    // deactivation can confuse registered WidgetFocusListeners, as the focus
    // is not changing due to a user-initiated event.
    AutoNativeNotificationDisabler local_notification_disabler;
    ClearFocus();
  }

  if (v)
    v->SchedulePaint();  // Remove focus border.
}

void FocusManager::RestoreFocusedView() {
#if defined(USE_X11)
  DCHECK(!should_handle_menu_key_release_);
#endif
  ViewStorage* view_storage = ViewStorage::GetInstance();
  if (!view_storage) {
    // This should never happen but bug 981648 seems to indicate it could.
    NOTREACHED();
    return;
  }

  View* view = view_storage->RetrieveView(stored_focused_view_storage_id_);
  if (view) {
    if (ContainsView(view)) {
      if (!view->IsFocusable() && view->IsAccessibilityFocusable()) {
        // RequestFocus would fail, but we want to restore focus to controls
        // that had focus in accessibility mode.
        SetFocusedViewWithReason(view, kReasonFocusRestore);
      } else {
        // This usually just sets the focus if this view is focusable, but
        // let the view override RequestFocus if necessary.
        view->RequestFocus();

        // If it succeeded, the reason would be incorrect; set it to
        // focus restore.
        if (focused_view_ == view)
          focus_change_reason_ = kReasonFocusRestore;
      }
    }
  }
}

void FocusManager::ClearStoredFocusedView() {
  ViewStorage* view_storage = ViewStorage::GetInstance();
  if (!view_storage) {
    // This should never happen but bug 981648 seems to indicate it could.
    NOTREACHED();
    return;
  }
  view_storage->RemoveView(stored_focused_view_storage_id_);
}

// Find the next (previous if reverse is true) focusable view for the specified
// FocusTraversable, starting at the specified view, traversing down the
// FocusTraversable hierarchy.
View* FocusManager::FindFocusableView(FocusTraversable* focus_traversable,
                                      View* starting_view,
                                      bool reverse) {
  FocusTraversable* new_focus_traversable = NULL;
  View* new_starting_view = NULL;
  View* v = focus_traversable->GetFocusSearch()->FindNextFocusableView(
      starting_view,
      reverse,
      FocusSearch::DOWN,
      false,
      &new_focus_traversable,
      &new_starting_view);

  // Let's go down the FocusTraversable tree as much as we can.
  while (new_focus_traversable) {
    DCHECK(!v);
    focus_traversable = new_focus_traversable;
    starting_view = new_starting_view;
    new_focus_traversable = NULL;
    starting_view = NULL;
    v = focus_traversable->GetFocusSearch()->FindNextFocusableView(
        starting_view,
        reverse,
        FocusSearch::DOWN,
        false,
        &new_focus_traversable,
        &new_starting_view);
  }
  return v;
}

void FocusManager::RegisterAccelerator(
    const ui::Accelerator& accelerator,
    ui::AcceleratorManager::HandlerPriority priority,
    ui::AcceleratorTarget* target) {
  accelerator_manager_->Register(accelerator, priority, target);
}

void FocusManager::UnregisterAccelerator(const ui::Accelerator& accelerator,
                                         ui::AcceleratorTarget* target) {
  accelerator_manager_->Unregister(accelerator, target);
}

void FocusManager::UnregisterAccelerators(ui::AcceleratorTarget* target) {
  accelerator_manager_->UnregisterAll(target);
}

bool FocusManager::ProcessAccelerator(const ui::Accelerator& accelerator) {
  if (accelerator_manager_->Process(accelerator))
    return true;
  if (delegate_.get())
    return delegate_->ProcessAccelerator(accelerator);
  return false;
}

void FocusManager::MaybeResetMenuKeyState(const KeyEvent& key) {
#if defined(USE_X11)
  // Always reset |should_handle_menu_key_release_| unless we are handling a
  // VKEY_MENU key release event. It ensures that VKEY_MENU accelerator can only
  // be activated when handling a VKEY_MENU key release event which is preceded
  // by an unhandled VKEY_MENU key press event. See also HandleKeyboardEvent().
  if (key.key_code() != ui::VKEY_MENU || key.type() != ui::ET_KEY_RELEASED)
    should_handle_menu_key_release_ = false;
#endif
}

ui::AcceleratorTarget* FocusManager::GetCurrentTargetForAccelerator(
    const ui::Accelerator& accelerator) const {
  ui::AcceleratorTarget* target =
      accelerator_manager_->GetCurrentTarget(accelerator);
  if (!target && delegate_.get())
    target = delegate_->GetCurrentTargetForAccelerator(accelerator);
  return target;
}

bool FocusManager::HasPriorityHandler(
    const ui::Accelerator& accelerator) const {
  return accelerator_manager_->HasPriorityHandler(accelerator);
}

// static
bool FocusManager::IsTabTraversalKeyEvent(const KeyEvent& key_event) {
  return key_event.key_code() == ui::VKEY_TAB && !key_event.IsControlDown();
}

void FocusManager::ViewRemoved(View* removed) {
  // If the view being removed contains (or is) the focused view,
  // clear the focus.  However, it's not safe to call ClearFocus()
  // (and in turn ClearNativeFocus()) here because ViewRemoved() can
  // be called while the top level widget is being destroyed.
  if (focused_view_ && removed && removed->Contains(focused_view_))
    SetFocusedView(NULL);
}

void FocusManager::AddFocusChangeListener(FocusChangeListener* listener) {
  focus_change_listeners_.AddObserver(listener);
}

void FocusManager::RemoveFocusChangeListener(FocusChangeListener* listener) {
  focus_change_listeners_.RemoveObserver(listener);
}

}  // namespace views
