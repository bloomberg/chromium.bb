// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/focus/focus_manager.h"

#include <algorithm>

#include "build/build_config.h"

#if defined(OS_LINUX)
#include <gtk/gtk.h>
#endif

#include "base/logging.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/views/events/event.h"
#include "ui/views/focus/focus_search.h"
#include "ui/views/view.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/native_widget.h"
#include "ui/views/widget/widget.h"

namespace ui {

namespace {

FocusEvent::TraversalDirection DirectionFromBool(bool forward) {
  return forward ? FocusEvent::DIRECTION_FORWARD
                 : FocusEvent::DIRECTION_REVERSE;
}

FocusEvent::TraversalDirection DirectionFromKeyEvent(const KeyEvent& event) {
  return DirectionFromBool(!event.IsShiftDown());
}

bool IsTraverseForward(FocusEvent::TraversalDirection direction) {
  return direction == FocusEvent::DIRECTION_FORWARD;
}

#if defined(OS_WIN)
// Don't allow focus traversal if the root window is not part of the active
// window hierarchy as this would mean we have no focused view and would focus
// the first focusable view.
bool CanTraverseFocus(Widget* widget) {
  HWND top_window = widget->native_widget()->GetNativeView();
  HWND active_window = ::GetActiveWindow();
  return active_window == top_window || ::IsChild(active_window, top_window);
}
#else
bool CanTraverseFocus(Widget* widget) {
  return true;
}
#endif

}

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

// static
FocusManager::WidgetFocusManager*
FocusManager::WidgetFocusManager::GetInstance() {
  return Singleton<WidgetFocusManager>::get();
}

////////////////////////////////////////////////////////////////////////////////
// FocusManager, public:

FocusManager::FocusManager(Widget* widget)
    : widget_(widget),
      focused_view_(NULL) {
  DCHECK(widget_);
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
  // If the focused view wants to process the key event as is, let it be.
  // On Linux we always dispatch key events to the focused view first, so
  // we should not do this check here. See also WidgetGtk::OnKeyEvent().
  if (focused_view_ && focused_view_->SkipDefaultKeyEventProcessing(event))
    return true;

  // Intercept Tab related messages for focus traversal.
  if (CanTraverseFocus(widget_) && IsTabTraversalKeyEvent(event)) {
    AdvanceFocus(DirectionFromKeyEvent(event));
    return false;
  }

  // Intercept arrow key messages to switch between grouped views.
  // TODO(beng): Perhaps make this a FocusTraversable that is created for
  //             Views that have a group set?
  ui::KeyboardCode key_code = event.key_code();
  if (focused_view_ && focused_view_->group() != -1 &&
      (key_code == ui::VKEY_UP || key_code == ui::VKEY_DOWN ||
       key_code == ui::VKEY_LEFT || key_code == ui::VKEY_RIGHT)) {
    bool next = (key_code == ui::VKEY_RIGHT || key_code == ui::VKEY_DOWN);
    std::vector<View*> views;
    focused_view_->parent()->GetViewsInGroup(focused_view_->group(), &views);
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
    SetFocusedViewWithReasonAndDirection(views[index],
                                         FocusEvent::REASON_TRAVERSAL,
                                         DirectionFromBool(next));
    return false;
  }

  // Process keyboard accelerators.
  // If the key combination matches an accelerator, the accelerator is
  // triggered, otherwise the key event is processed as usual.
  Accelerator accelerator(event.key_code(), event.GetModifiers());
  if (ProcessAccelerator(accelerator)) {
    // If a shortcut was activated for this keydown message, do not propagate
    // the event further.
    return false;
  }
  return true;
}

bool FocusManager::ContainsView(View* view) const {
  Widget* widget = view->GetWidget();
  return widget ? widget->GetFocusManager() == this : false;
}

void FocusManager::RemoveView(View* view) {
  // Clear focus if the removed child was focused.
  if (focused_view_ == view)
    ClearFocus();
}

void FocusManager::AdvanceFocus(FocusEvent::TraversalDirection direction) {
  View* view = GetNextFocusableView(focused_view_, direction, false);
  // Note: Do not skip this next block when v == focused_view_.  If the user
  // tabs past the last focusable element in a web page, we'll get here, and if
  // the TabContentsContainerView is the only focusable view (possible in
  // full-screen mode), we need to run this block in order to cycle around to
  // the first element on the page.
  if (view) {
    SetFocusedViewWithReasonAndDirection(view, FocusEvent::REASON_TRAVERSAL,
                                         direction);
  }
}

void FocusManager::SetFocusedViewWithReasonAndDirection(
    View* view,
    FocusEvent::Reason reason,
    FocusEvent::TraversalDirection direction) {
  if (focused_view_ == view)
    return;

  View* prev_focused_view = focused_view_;
  if (focused_view_) {
    focused_view_->OnBlur(
        FocusEvent(FocusEvent::TYPE_FOCUS_OUT, reason, direction));
    focused_view_->Invalidate();
  }

  // Notified listeners that the focus changed.
  FocusChangeListenerList::const_iterator iter;
  for (iter = focus_change_listeners_.begin();
       iter != focus_change_listeners_.end(); ++iter) {
    (*iter)->FocusWillChange(prev_focused_view, view);
  }

  focused_view_ = view;

  if (view) {
    view->Invalidate();
    view->OnFocus(FocusEvent(FocusEvent::TYPE_FOCUS_IN, reason, direction));
    // The view might be deleted now.
  }
}

void FocusManager::ClearFocus() {
  SetFocusedView(NULL);
  widget_->native_widget()->FocusNativeView(NULL);
}

void FocusManager::ValidateFocusedView() {
  if (focused_view_) {
    if (!ContainsView(focused_view_))
      ClearFocus();
  }
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

AcceleratorTarget* FocusManager::GetCurrentTargetForAccelerator(
    const Accelerator& accelerator) const {
  AcceleratorMap::const_iterator map_iter = accelerators_.find(accelerator);
  if (map_iter == accelerators_.end() || map_iter->second.empty())
    return NULL;
  return map_iter->second.front();
}

// static
bool FocusManager::IsTabTraversalKeyEvent(const KeyEvent& key_event) {
  return key_event.key_code() == ui::VKEY_TAB &&
         !key_event.IsControlDown();
}

// static
FocusManager* FocusManager::GetFocusManagerForNativeView(
    gfx::NativeView native_view) {
  NativeWidget* native_widget =
      NativeWidget::GetNativeWidgetForNativeView(native_view);
  return native_widget ? native_widget->GetWidget()->GetFocusManager() : NULL;
}

// static
FocusManager* FocusManager::GetFocusManagerForNativeWindow(
    gfx::NativeWindow native_window) {
  NativeWidget* native_widget =
      NativeWidget::GetNativeWidgetForNativeWindow(native_window);
  return native_widget ? native_widget->GetWidget()->GetFocusManager() : NULL;
}

////////////////////////////////////////////////////////////////////////////////
// FocusManager, private:

View* FocusManager::GetNextFocusableView(
    View* original_starting_view,
    FocusEvent::TraversalDirection direction,
    bool dont_loop) {
  FocusTraversable* focus_traversable = NULL;

  // Let's re-validate the focused view.
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
      if (IsTraverseForward(direction)) {
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
  View* v = FindFocusableView(focus_traversable, starting_view, direction);
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
      bool check_starting_view = !IsTraverseForward(direction);
      v = parent_focus_traversable->GetFocusSearch()->FindNextFocusableView(
          starting_view, !IsTraverseForward(direction), FocusSearch::UP,
          check_starting_view, &new_focus_traversable, &new_starting_view);

      if (new_focus_traversable) {
        DCHECK(!v);

        // There is a FocusTraversable, traverse it down.
        v = FindFocusableView(new_focus_traversable, NULL, direction);
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
      return GetNextFocusableView(NULL, direction, true);
    }
  }
  return NULL;
}

// Find the next focusable view for the specified FocusTraversable, starting at
// the specified view, traversing down the FocusTraversable hierarchy in
// |direction|.
View* FocusManager::FindFocusableView(
    FocusTraversable* focus_traversable,
    View* starting_view,
    FocusEvent::TraversalDirection direction) {
  FocusTraversable* new_focus_traversable = NULL;
  View* new_starting_view = NULL;
  View* v = focus_traversable->GetFocusSearch()->FindNextFocusableView(
      starting_view,
      !IsTraverseForward(direction),
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
        !IsTraverseForward(direction),
        FocusSearch::DOWN,
        false,
        &new_focus_traversable,
        &new_starting_view);
  }
  return v;
}

}  // namespace ui
