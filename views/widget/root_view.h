// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_ROOT_VIEW_H_
#define VIEWS_WIDGET_ROOT_VIEW_H_
#pragma once

#include <string>

#include "base/ref_counted.h"
#include "views/focus/focus_manager.h"
#include "views/focus/focus_search.h"
#include "views/view.h"

namespace views {

class Widget;
#if defined(TOUCH_UI)
class GestureManager;
#endif

////////////////////////////////////////////////////////////////////////////////
// RootView class
//
//  The RootView is the root of a View hierarchy. A RootView is attached to a
//  Widget. The Widget is responsible for receiving events from the host
//  environment, converting them to views-compatible events and then forwarding
//  them to the RootView for propagation into the View hierarchy.
//
//  A RootView can have only one child, called its "Contents View" which is
//  sized to fill the bounds of the RootView (and hence the client area of the
//  Widget). Call SetContentsView() after the associated Widget has been
//  initialized to attach the contents view to the RootView.
//  TODO(beng): Enforce no other callers to AddChildView/tree functions by
//              overriding those methods as private here.
//  TODO(beng): Move to internal namespace and remove accessors from
//              View/Widget.
//  TODO(beng): Clean up API further, make WidgetImpl a friend.
//
class RootView : public View,
                 public FocusTraversable {
 public:
  static const char kViewClassName[];

  // Creation and lifetime -----------------------------------------------------
  explicit RootView(Widget* widget);
  virtual ~RootView();

  // Tree operations -----------------------------------------------------------

  // Sets the "contents view" of the RootView. This is the single child view
  // that is responsible for laying out the contents of the widget.
  void SetContentsView(View* contents_view);

  // Called when parent of the host changed.
  void NotifyNativeViewHierarchyChanged(bool attached,
                                        gfx::NativeView native_view);

  // Input ---------------------------------------------------------------------

  // Invoked By the Widget if the mouse drag is interrupted by
  // the system. Invokes OnMouseReleased with a value of true for canceled.
  void ProcessMouseDragCanceled();

  // Process a key event. Send the event to the focused view and up the focus
  // path, and finally to the default keyboard handler, until someone consumes
  // it.  Returns whether anyone consumed the event.
  bool ProcessKeyEvent(const KeyEvent& event);

#if defined(TOUCH_UI) && defined(UNIT_TEST)
  // For unit testing purposes, we use this method to set a mock
  // GestureManager
  void SetGestureManager(GestureManager* g) { gesture_manager_ = g; }
#endif

  // Focus ---------------------------------------------------------------------

  // Used to set the FocusTraversable parent after the view has been created
  // (typically when the hierarchy changes and this RootView is added/removed).
  virtual void SetFocusTraversableParent(FocusTraversable* focus_traversable);

  // Used to set the View parent after the view has been created.
  virtual void SetFocusTraversableParentView(View* view);

  // System events -------------------------------------------------------------

  // Public API for broadcasting theme change notifications to this View
  // hierarchy.
  void ThemeChanged();

  // Public API for broadcasting locale change notifications to this View
  // hierarchy.
  void LocaleChanged();

  // Overridden from FocusTraversable:
  virtual FocusSearch* GetFocusSearch() OVERRIDE;
  virtual FocusTraversable* GetFocusTraversableParent() OVERRIDE;
  virtual View* GetFocusTraversableParentView() OVERRIDE;

  // Overridden from View:
  virtual const Widget* GetWidget() const OVERRIDE;
  virtual Widget* GetWidget() OVERRIDE;
  virtual bool IsVisibleInRootView() const OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;
  virtual void SchedulePaintInRect(const gfx::Rect& rect) OVERRIDE;
  virtual bool OnMousePressed(const MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const MouseEvent& event, bool canceled) OVERRIDE;
  virtual void OnMouseMoved(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseExited(const MouseEvent& event) OVERRIDE;
  virtual bool OnMouseWheel(const MouseWheelEvent& event) OVERRIDE;
#if defined(TOUCH_UI)
  virtual TouchStatus OnTouchEvent(const TouchEvent& event) OVERRIDE;
#endif
  virtual void SetMouseHandler(View* new_mouse_handler) OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

 protected:
  // Overridden from View:
  virtual void ViewHierarchyChanged(bool is_add, View* parent,
                                    View* child) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  friend class View;
  friend class Widget;

#if defined(TOUCH_UI)
  // Required so the GestureManager can call the Process* entry points
  // with synthetic events as necessary.
  friend class GestureManager;
#endif

  // Coordinate conversion -----------------------------------------------------

  // Convert a point to our current mouse handler. Returns false if the
  // mouse handler is not connected to a Widget. In that case, the
  // conversion cannot take place and |p| is unchanged
  bool ConvertPointToMouseHandler(const gfx::Point& l, gfx::Point* p);

  // Input ---------------------------------------------------------------------

  // Update the cursor given a mouse event. This is called by non mouse_move
  // event handlers to honor the cursor desired by views located under the
  // cursor during drag operations. The location of the mouse should be in the
  // current coordinate system (i.e. any necessary transformation should be
  // applied to the point prior to calling this).
  void UpdateCursor(const MouseEvent& event);

  // Updates the last_mouse_* fields from e. The location of the mouse should be
  // in the current coordinate system (i.e. any necessary transformation should
  // be applied to the point prior to calling this).
  void SetMouseLocationAndFlags(const MouseEvent& event);

  //////////////////////////////////////////////////////////////////////////////

  // Tree operations -----------------------------------------------------------

  // The host Widget
  Widget* widget_;

  // Input ---------------------------------------------------------------------

  // The view currently handing down - drag - up
  View* mouse_pressed_handler_;

  // The view currently handling enter / exit
  View* mouse_move_handler_;

  // The last view to handle a mouse click, so that we can determine if
  // a double-click lands on the same view as its single-click part.
  View* last_click_handler_;

  // true if mouse_handler_ has been explicitly set
  bool explicit_mouse_handler_;

  // Last position/flag of a mouse press/drag. Used if capture stops and we need
  // to synthesize a release.
  int last_mouse_event_flags_;
  int last_mouse_event_x_;
  int last_mouse_event_y_;

#if defined(TOUCH_UI)
  // The gesture_manager_ for this.
  GestureManager* gesture_manager_;

  // The view currently handling touch events.
  View* touch_pressed_handler_;
#endif

  // Focus ---------------------------------------------------------------------

  // The focus search algorithm.
  FocusSearch focus_search_;

  // Whether this root view belongs to the current active window.
  // bool activated_;

  // The parent FocusTraversable, used for focus traversal.
  FocusTraversable* focus_traversable_parent_;

  // The View that contains this RootView. This is used when we have RootView
  // wrapped inside native components, and is used for the focus traversal.
  View* focus_traversable_parent_view_;

  // Drag and drop -------------------------------------------------------------

  // Tracks drag state for a view.
  View::DragInfo drag_info;

  DISALLOW_IMPLICIT_CONSTRUCTORS(RootView);
};
}  // namespace views

#endif  // VIEWS_WIDGET_ROOT_VIEW_H_
