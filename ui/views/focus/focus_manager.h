// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_FOCUS_FOCUS_MANAGER_H_
#define UI_VIEWS_FOCUS_FOCUS_MANAGER_H_
#pragma once

#include <list>
#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/singleton.h"
#include "gfx/native_widget_types.h"
#include "ui/views/events/accelerator.h"

// The FocusManager class is used to handle focus traversal, store/restore
// focused views and handle keyboard accelerators.
//
// There are 2 types of focus:
// - the native focus, which is the focus that an gfx::NativeView has.
// - the view focus, which is the focus that a views::View has.
//
// Each native view must register with their Focus Manager so the focus manager
// gets notified when they are focused (and keeps track of the native focus) and
// as well so that the tab key events can be intercepted.
// They can provide when they register a View that is kept in synch in term of
// focus. This is used in NativeControl for example, where a View wraps an
// actual native window.
// This is already done for you if you subclass the NativeControl class or if
// you use the NativeViewHost class.
//
// When creating a top window (derived from views::Widget) that is not a child
// window, it creates and owns a FocusManager to manage the focus for itself and
// all its child windows.
//
// The FocusTraversable interface exposes the methods a class should implement
// in order to be able to be focus traversed when tab key is pressed.
// RootViews implement FocusTraversable.
// The FocusManager contains a top FocusTraversable instance, which is the top
// RootView.
//
// If you just use views, then the focus traversal is handled for you by the
// RootView. The default traversal order is the order in which the views have
// been added to their container. You can modify this order by using the View
// method SetNextFocusableView().
//
// If you are embedding a native view containing a nested RootView (for example
// by adding a NativeControl that contains a WidgetWin as its native
// component), then you need to:
// - override the View::GetFocusTraversable() method in your outer component.
//   It should return the RootView of the inner component. This is used when
//   the focus traversal traverse down the focus hierarchy to enter the nested
//   RootView. In the example mentioned above, the NativeControl overrides
//   GetFocusTraversable() and returns hwnd_view_container_->GetRootView().
// - call RootView::SetFocusTraversableParent() on the nested RootView and point
//   it to the outter RootView. This is used when the focus goes out of the
//   nested RootView. In the example:
//   hwnd_view_container_->GetRootView()->SetFocusTraversableParent(
//      native_control->GetRootView());
// - call RootView::SetFocusTraversableParentView() on the nested RootView with
//   the parent view that directly contains the native window. This is needed
//   when traversing up from the nested RootView to know which view to start
//   with when going to the next/previous view.
//   In our example:
//   hwnd_view_container_->GetRootView()->SetFocusTraversableParent(
//      native_control);
//
// Note that FocusTraversable do not have to be RootViews: AccessibleToolbarView
// is FocusTraversable.

namespace ui {

class FocusSearch;
class RootView;
class View;
class Widget;

// The FocusTraversable interface is used by components that want to process
// focus traversal events (due to Tab/Shift-Tab key events).
class FocusTraversable {
 public:
  // Return a FocusSearch object that implements the algorithm to find
  // the next or previous focusable view.
  virtual FocusSearch* GetFocusSearch() = 0;

  // Should return the parent FocusTraversable.
  // The top RootView which is the top FocusTraversable returns NULL.
  virtual FocusTraversable* GetFocusTraversableParent() = 0;

  // This should return the View this FocusTraversable belongs to.
  // It is used when walking up the view hierarchy tree to find which view
  // should be used as the starting view for finding the next/previous view.
  virtual View* GetFocusTraversableParentView() = 0;

 protected:
  virtual ~FocusTraversable() {}
};

// This interface should be implemented by classes that want to be notified when
// the focus is about to change.  See the Add/RemoveFocusChangeListener methods.
class FocusChangeListener {
 public:
  virtual void FocusWillChange(View* focused_before, View* focused_now) = 0;

 protected:
  virtual ~FocusChangeListener() {}
};

// This interface should be implemented by classes that want to be notified when
// the native focus is about to change.  Listeners implementing this interface
// will be invoked for all native focus changes across the entire Chrome
// application.  FocusChangeListeners are only called for changes within the
// children of a single top-level native-view.
class WidgetFocusChangeListener {
 public:
  virtual void NativeFocusWillChange(gfx::NativeView focused_before,
                                     gfx::NativeView focused_now) = 0;

 protected:
  virtual ~WidgetFocusChangeListener() {}
};

class FocusManager {
 public:
  class WidgetFocusManager {
   public:
    // Returns the singleton instance.
    static WidgetFocusManager* GetInstance();

    // Adds/removes a WidgetFocusChangeListener |listener| to the set of
    // active listeners.
    void AddFocusChangeListener(WidgetFocusChangeListener* listener);
    void RemoveFocusChangeListener(WidgetFocusChangeListener* listener);

    // To be called when native-focus shifts from |focused_before| to
    // |focused_now|.
    // TODO(port) : Invocations to this routine are only implemented for
    // the Win32 platform.  Calls need to be placed appropriately for
    // non-Windows environments.
    void OnWidgetFocusEvent(gfx::NativeView focused_before,
                            gfx::NativeView focused_now);

    // Enable/Disable notification of registered listeners during calls
    // to OnWidgetFocusEvent.  Used to prevent unwanted focus changes from
    // propagating notifications.
    void EnableNotifications() { enabled_ = true; }
    void DisableNotifications() { enabled_ = false; }

   private:
    WidgetFocusManager() : enabled_(true) {}

    typedef std::vector<WidgetFocusChangeListener*>
      WidgetFocusChangeListenerList;
    WidgetFocusChangeListenerList focus_change_listeners_;

    bool enabled_;

    friend struct DefaultSingletonTraits<WidgetFocusManager>;
    DISALLOW_COPY_AND_ASSIGN(WidgetFocusManager);
  };

  // The reason why the focus changed.
  enum FocusChangeReason {
    // The focus changed because the user traversed focusable views using
    // keys like Tab or Shift+Tab.
    kReasonFocusTraversal,

    // The focus changed due to restoring the focus.
    kReasonFocusRestore,

    // The focus changed due to a click or a shortcut to jump directly to
    // a particular view.
    kReasonDirectFocusChange
  };

  explicit FocusManager(Widget* widget);
  virtual ~FocusManager();

  // Returns the global WidgetFocusManager instance for the running application.
  static WidgetFocusManager* GetWidgetFocusManager();

  // Processes the passed key event for accelerators and tab traversal.
  // Returns false if the event has been consumed and should not be processed
  // further.
  bool OnKeyEvent(const KeyEvent& event);

  // Returns true is the specified is part of the hierarchy of the window
  // associated with this FocusManager.
  bool ContainsView(View* view);

  // Advances the focus (backward if reverse is true).
  void AdvanceFocus(bool reverse);

  // The FocusManager keeps track of the focused view within a RootView.
  View* focused_view() const { return focused_view_; }

  // Low-level methods to force the focus to change (and optionally provide
  // a reason). If the focus change should only happen if the view is
  // currently focusable, enabled, and visible, call view->RequestFocus().
  void SetFocusedViewWithReason(View* view, FocusChangeReason reason);
  void SetFocusedView(View* view) {
    SetFocusedViewWithReason(view, kReasonDirectFocusChange);
  }

  // Get the reason why the focus most recently changed.
  FocusChangeReason focus_change_reason() const {
    return focus_change_reason_;
  }

  // Clears the focused view. The window associated with the top root view gets
  // the native focus (so we still get keyboard events).
  void ClearFocus();

  // Validates the focused view, clearing it if the window it belongs too is not
  // attached to the window hierarchy anymore.
  void ValidateFocusedView();

  // Stores and restores the focused view. Used when the window becomes
  // active/inactive.
  void StoreFocusedView();
  void RestoreFocusedView();

  // Clears the stored focused view.
  void ClearStoredFocusedView();

  // Register a keyboard accelerator for the specified target. If multiple
  // targets are registered for an accelerator, a target registered later has
  // higher priority.
  // Note that we are currently limited to accelerators that are either:
  // - a key combination including Ctrl or Alt
  // - the escape key
  // - the enter key
  // - any F key (F1, F2, F3 ...)
  // - any browser specific keys (as available on special keyboards)
  void RegisterAccelerator(const Accelerator& accelerator,
                           AcceleratorTarget* target);

  // Unregister the specified keyboard accelerator for the specified target.
  void UnregisterAccelerator(const Accelerator& accelerator,
                             AcceleratorTarget* target);

  // Unregister all keyboard accelerator for the specified target.
  void UnregisterAccelerators(AcceleratorTarget* target);

  // Activate the target associated with the specified accelerator.
  // First, AcceleratorPressed handler of the most recently registered target
  // is called, and if that handler processes the event (i.e. returns true),
  // this method immediately returns. If not, we do the same thing on the next
  // target, and so on.
  // Returns true if an accelerator was activated.
  bool ProcessAccelerator(const Accelerator& accelerator);

  // Called by a RootView when a view within its hierarchy is removed from its
  // parent. This will only be called by a RootView in a hierarchy of Widgets
  // that this FocusManager is attached to the parent Widget of.
  void ViewRemoved(View* parent, View* removed);

  // Adds/removes a listener.  The FocusChangeListener is notified every time
  // the focused view is about to change.
  void AddFocusChangeListener(FocusChangeListener* listener);
  void RemoveFocusChangeListener(FocusChangeListener* listener);

  // Returns the AcceleratorTarget that should be activated for the specified
  // keyboard accelerator, or NULL if no view is registered for that keyboard
  // accelerator.
  AcceleratorTarget* GetCurrentTargetForAccelerator(
      const Accelerator& accelertor) const;

  // Convenience method that returns true if the passed |key_event| should
  // trigger tab traversal (if it is a TAB key press with or without SHIFT
  // pressed).
  static bool IsTabTraversalKeyEvent(const KeyEvent& key_event);

  // Retrieves the FocusManager associated with the passed native view.
  static FocusManager* GetFocusManagerForNativeView(
      gfx::NativeView native_view);

  // Retrieves the FocusManager associated with the passed native view.
  static FocusManager* GetFocusManagerForNativeWindow(
      gfx::NativeWindow native_window);

 private:
  // Returns the next focusable view.
  View* GetNextFocusableView(View* starting_view, bool reverse, bool dont_loop);

  // Returns the focusable view found in the FocusTraversable specified starting
  // at the specified view. This traverses down along the FocusTraversable
  // hierarchy.
  // Returns NULL if no focusable view were found.
  View* FindFocusableView(FocusTraversable* focus_traversable,
                          View* starting_view,
                          bool reverse);

  // The top-level Widget this FocusManager is associated with.
  Widget* widget_;

  // The view that currently is focused.
  View* focused_view_;

  // The storage id used in the ViewStorage to store/restore the view that last
  // had focus.
  int stored_focused_view_storage_id_;

  // The reason why the focus most recently changed.
  FocusChangeReason focus_change_reason_;

  // The accelerators and associated targets.
  typedef std::list<AcceleratorTarget*> AcceleratorTargetList;
  typedef std::map<Accelerator, AcceleratorTargetList> AcceleratorMap;
  AcceleratorMap accelerators_;

  // The list of registered FocusChange listeners.
  typedef std::vector<FocusChangeListener*> FocusChangeListenerList;
  FocusChangeListenerList focus_change_listeners_;

  DISALLOW_COPY_AND_ASSIGN(FocusManager);
};

// A basic helper class that is used to disable native focus change
// notifications within a scope.
class AutoNativeNotificationDisabler {
 public:
  AutoNativeNotificationDisabler() {
    FocusManager::GetWidgetFocusManager()->DisableNotifications();
  }

  ~AutoNativeNotificationDisabler() {
    FocusManager::GetWidgetFocusManager()->EnableNotifications();
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(AutoNativeNotificationDisabler);
};

}  // namespace ui

#endif  // UI_VIEWS_FOCUS_FOCUS_MANAGER_H_
