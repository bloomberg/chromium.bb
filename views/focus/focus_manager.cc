// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/focus/focus_manager.h"

#include <algorithm>

#include "build/build_config.h"

#if defined(OS_LINUX)
#include <gtk/gtk.h>
#endif

#include "base/logging.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "views/accelerator.h"
#include "views/focus/focus_search.h"
#include "views/focus/view_storage.h"
#include "views/view.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"

namespace views {

// FocusManager::WidgetFocusManager ---------------------------------

void FocusManager::WidgetFocusManager::AddFocusChangeListener(
    WidgetFocusChangeListener* listener) {
  DCHECK(std::find(focus_change_listeners_.begin(),
                   focus_change_listeners_.end(), listener) ==
         focus_change_listeners_.end()) <<
             "Adding a WidgetFocusChangeListener twice.";
  focus_change_listeners_.push_back(listener);
}

void FocusManager::WidgetFocusManager::RemoveFocusChangeListener(
    WidgetFocusChangeListener* listener) {
  WidgetFocusChangeListenerList::iterator iter(std::find(
      focus_change_listeners_.begin(),
      focus_change_listeners_.end(),
      listener));
  if (iter != focus_change_listeners_.end()) {
    focus_change_listeners_.erase(iter);
  } else {
    NOTREACHED() <<
      "Attempting to remove an unregistered WidgetFocusChangeListener.";
  }
}

void FocusManager::WidgetFocusManager::OnWidgetFocusEvent(
    gfx::NativeView focused_before,
    gfx::NativeView focused_now) {
  if (!enabled_)
    return;

  // Perform a safe iteration over the focus listeners, as the array
  // may change during notification.
  WidgetFocusChangeListenerList local_listeners(focus_change_listeners_);
  WidgetFocusChangeListenerList::iterator iter(local_listeners.begin());
  for (;iter != local_listeners.end(); ++iter) {
    (*iter)->NativeFocusWillChange(focused_before, focused_now);
  }
}

FocusManager::WidgetFocusManager::WidgetFocusManager() : enabled_(true) {}

FocusManager::WidgetFocusManager::~WidgetFocusManager() {}

// static
FocusManager::WidgetFocusManager*
FocusManager::WidgetFocusManager::GetInstance() {
  return Singleton<WidgetFocusManager>::get();
}

// FocusManager -----------------------------------------------------

FocusManager::FocusManager(Widget* widget)
    : widget_(widget),
      focused_view_(NULL),
      focus_change_reason_(kReasonDirectFocusChange) {
  DCHECK(widget_);
  stored_focused_view_storage_id_ =
      ViewStorage::GetInstance()->CreateStorageID();
}

FocusManager::~FocusManager() {
  // If there are still registered FocusChange listeners, chances are they were
  // leaked so warn about them.
  DCHECK(focus_change_listeners_.empty());
}

// static
FocusManager::WidgetFocusManager* FocusManager::GetWidgetFocusManager() {
  return WidgetFocusManager::GetInstance();
}

bool FocusManager::OnKeyEvent(const KeyEvent& event) {
#if defined(OS_WIN)
  // If the focused view wants to process the key event as is, let it be.
  // On Linux we always dispatch key events to the focused view first, so
  // we should not do this check here. See also WidgetGtk::OnKeyEvent().
  if (focused_view_ && focused_view_->SkipDefaultKeyEventProcessing(event))
    return true;
#endif

  // Intercept Tab related messages for focus traversal.
  // Note that we don't do focus traversal if the root window is not part of the
  // active window hierarchy as this would mean we have no focused view and
  // would focus the first focusable view.
#if defined(OS_WIN)
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
  ui::KeyboardCode key_code = event.key_code();
  if (focused_view_ && focused_view_->GetGroup() != -1 &&
      (key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN ||
       key_code == ui::VKEY_LEFT || key_code == ui::VKEY_RIGHT)) {
    bool next = (key_code == ui::VKEY_RIGHT || key_code == ui::VKEY_DOWN);
    std::vector<View*> views;
    focused_view_->parent()->GetViewsWithGroup(focused_view_->GetGroup(),
                                               &views);
    std::vector<View*>::const_iterator iter = std::find(views.begin(),
                                                        views.end(),
                                                        focused_view_);
    DCHECK(iter != views.end());
    int index = static_cast<int>(iter - views.begin());
    index += next ? 1 : -1;
    if (index < 0) {
      index = static_cast<int>(views.size()) - 1;
    } else if (index >= static_cast<int>(views.size())) {
      index = 0;
    }
    SetFocusedViewWithReason(views[index], kReasonFocusTraversal);
    return false;
  }

  // Process keyboard accelerators.
  // If the key combination matches an accelerator, the accelerator is
  // triggered, otherwise the key event is processed as usual.
  Accelerator accelerator(event.key_code(),
                          event.IsShiftDown(),
                          event.IsControlDown(),
                          event.IsAltDown());
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
        // This is the case with WidgetWins for example.
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

  // Notified listeners that the focus will change.
  FocusChangeListenerList::const_iterator iter;
  for (iter = focus_change_listeners_.begin();
       iter != focus_change_listeners_.end(); ++iter) {
    (*iter)->FocusWillChange(focused_view_, view);
  }

  focus_change_reason_ = reason;
  if (focused_view_)
    focused_view_->Blur();
  focused_view_ = view;
  if (focused_view_)
    focused_view_->Focus();
}

void FocusManager::ClearFocus() {
  SetFocusedView(NULL);
  ClearNativeFocus();
}

void FocusManager::StoreFocusedView() {
  ViewStorage* view_storage = ViewStorage::GetInstance();
  if (!view_storage) {
    // This should never happen but bug 981648 seems to indicate it could.
    NOTREACHED();
    return;
  }

  // TODO (jcampan): when a TabContents containing a popup is closed, the focus
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
  ViewStorage* view_storage = ViewStorage::GetInstance();
  if (!view_storage) {
    // This should never happen but bug 981648 seems to indicate it could.
    NOTREACHED();
    return;
  }

  View* view = view_storage->RetrieveView(stored_focused_view_storage_id_);
  if (view) {
    if (ContainsView(view)) {
      if (!view->IsFocusableInRootView() &&
          view->IsAccessibilityFocusableInRootView()) {
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
  } else {
    // Clearing the focus will focus the root window, so we still get key
    // events.
    ClearFocus();
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
    const Accelerator& accelerator,
    AcceleratorTarget* target) {
  AcceleratorTargetList& targets = accelerators_[accelerator];
  DCHECK(std::find(targets.begin(), targets.end(), target) == targets.end())
      << "Registering the same target multiple times";
  targets.push_front(target);
}

void FocusManager::UnregisterAccelerator(const Accelerator& accelerator,
                                         AcceleratorTarget* target) {
  AcceleratorMap::iterator map_iter = accelerators_.find(accelerator);
  if (map_iter == accelerators_.end()) {
    NOTREACHED() << "Unregistering non-existing accelerator";
    return;
  }

  AcceleratorTargetList* targets = &map_iter->second;
  AcceleratorTargetList::iterator target_iter =
      std::find(targets->begin(), targets->end(), target);
  if (target_iter == targets->end()) {
    NOTREACHED() << "Unregistering accelerator for wrong target";
    return;
  }

  targets->erase(target_iter);
}

void FocusManager::UnregisterAccelerators(AcceleratorTarget* target) {
  for (AcceleratorMap::iterator map_iter = accelerators_.begin();
       map_iter != accelerators_.end(); ++map_iter) {
    AcceleratorTargetList* targets = &map_iter->second;
    targets->remove(target);
  }
}

bool FocusManager::ProcessAccelerator(const Accelerator& accelerator) {
  AcceleratorMap::iterator map_iter = accelerators_.find(accelerator);
  if (map_iter != accelerators_.end()) {
    // We have to copy the target list here, because an AcceleratorPressed
    // event handler may modify the list.
    AcceleratorTargetList targets(map_iter->second);
    for (AcceleratorTargetList::iterator iter = targets.begin();
         iter != targets.end(); ++iter) {
      if ((*iter)->AcceleratorPressed(accelerator))
        return true;
    }
  }
  return false;
}

AcceleratorTarget* FocusManager::GetCurrentTargetForAccelerator(
    const views::Accelerator& accelerator) const {
  AcceleratorMap::const_iterator map_iter = accelerators_.find(accelerator);
  if (map_iter == accelerators_.end() || map_iter->second.empty())
    return NULL;
  return map_iter->second.front();
}

// static
bool FocusManager::IsTabTraversalKeyEvent(const KeyEvent& key_event) {
  return key_event.key_code() == ui::VKEY_TAB && !key_event.IsControlDown();
}

void FocusManager::ViewRemoved(View* parent, View* removed) {
  if (focused_view_ && focused_view_ == removed)
    ClearFocus();
}

void FocusManager::AddFocusChangeListener(FocusChangeListener* listener) {
  DCHECK(std::find(focus_change_listeners_.begin(),
                   focus_change_listeners_.end(), listener) ==
      focus_change_listeners_.end()) << "Adding a listener twice.";
  focus_change_listeners_.push_back(listener);
}

void FocusManager::RemoveFocusChangeListener(FocusChangeListener* listener) {
  FocusChangeListenerList::iterator place =
      std::find(focus_change_listeners_.begin(), focus_change_listeners_.end(),
                listener);
  if (place == focus_change_listeners_.end()) {
    NOTREACHED() << "Removing a listener that isn't registered.";
    return;
  }
  focus_change_listeners_.erase(place);
}

}  // namespace views
