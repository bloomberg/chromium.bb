// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_VIEW_H_
#define UI_VIEWS_VIEW_H_

#include <vector>

#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "gfx/rect.h"
#include "ui/views/events/event.h"

namespace gfx {
class Canvas;
class Point;
class Rect;
class Size;
}

namespace ui {
namespace internal {
class RootView;
}
class Accelerator;
class Border;
class ContextMenuController;
class DragController;
class FocusManager;
class FocusTraversable;
class LayoutManager;
class ThemeProvider;
class Widget;

////////////////////////////////////////////////////////////////////////////////
// View class
//
//  View encapsulates rendering, layout and event handling for rectangles within
//  a view hierarchy.
//
//  Client code typically subclasses View and overrides virtual methods to
//  handle painting, child view positioning and handle certain types of events.
//
//  Views are owned by their parent View unless specified otherwise. This means
//  that in most cases Views are automatically destroyed when the window that
//  contains them is destroyed.
//
// TODO(beng): consider the visibility of many of these methods.
//             consider making RootView a friend and making many private or
//             protected.
class View {
 public:
  typedef std::vector<View*> ViewVector;

  // Creation and lifetime -----------------------------------------------------
  View();
  virtual ~View();

  // By default a View is owned by its parent unless specified otherwise here.
  bool parent_owned() const { return parent_owned_; }
  void set_parent_owned(bool parent_owned) { parent_owned_ = parent_owned; }

  void set_drag_controller(DragController* drag_controller) {
    drag_controller_ = drag_controller;
  }

  // Size and disposition ------------------------------------------------------

  void SetBounds(int x, int y, int width, int height);
  void SetBoundsRect(const gfx::Rect& bounds);
  void SetSize(const gfx::Size& size);
  void SetPosition(const gfx::Point& position);
  gfx::Rect bounds() const { return bounds_; }
  gfx::Rect GetVisibleBounds() const;
  int x() const { return bounds_.x(); }
  int y() const { return bounds_.y(); }
  int width() const { return bounds_.width(); }
  int height() const { return bounds_.height(); }

  // Owned by the view.
  void SetBorder(Border* border);
  Border* border() { return border_.get(); }

  // Returns the bounds of the content area of the view, i.e. the rectangle
  // enclosed by the view's border.
  gfx::Rect GetContentsBounds() const;

  // Override to be notified when the bounds of a view have changed.
  virtual void OnBoundsChanged();

  virtual gfx::Size GetPreferredSize() const;
  virtual gfx::Size GetMinimumSize() const;

  void SetLayoutManager(LayoutManager* layout_manager);
  virtual void Layout();

  // If a View is not visible, it will not be rendered, focused, etc.
  bool visible() const { return visible_; }
  void SetVisible(bool visible);

  // Disabled Views will not receive mouse press/release events, nor can they be
  // focused.
  bool enabled() const { return enabled_; }
  void SetEnabled(bool enabled);

  // Attributes ----------------------------------------------------------------

  int id() const { return id_; }
  void set_id(int id) { id_ = id; }
  int group() const { return group_; }
  void set_group(int group) { group_ = group; }

  // Returns the View within this View's hierarchy whose id matches that
  // specified.
  View* GetViewById(int id) const;

  // Populates a ViewVector with the Views within this View's hierarchy that
  // match the specified group id.
  void GetViewsWithGroup(int group, ViewVector* vec) const;

  // TODO(beng): implementme
  virtual View* GetSelectedViewForGroup(int group_id);

  // Coordinate conversion -----------------------------------------------------

  // Converts a point from the coordinate system of |source| to |target|.
  static void ConvertPointToView(View* source, View* target, gfx::Point* point);

  // Converts a point from the coordinate system of |source| to the screen.
  // If |source| is not attached to a Widget that is in screen space, |point| is
  // not modified.
  static void ConvertPointToScreen(View* source, gfx::Point* point);

  // Converts a point from the coordinate system of |source| to the Widget that
  // most closely contains it.
  static void ConvertPointToWidget(View* source, gfx::Point* point);

  // Tree operations -----------------------------------------------------------

  // Returns the Widget that contains this View, or NULL if it is not contained
  // within a Widget.
  virtual Widget* GetWidget() const;

  // Adds a View as a child of this one, optionally at |index|.
  void AddChildView(View* view);
  void AddChildViewAt(View* view, size_t index);

  // Removes a View as a child of this View. Does not delete the child.
  View* RemoveChildView(View* view);

  // Removes all View children of this View. Deletes the children if
  // |delete_children| is true.
  void RemoveAllChildViews(bool delete_children);

  // Returns the View at the specified |index|.
  View* GetChildViewAt(size_t index);

  // Returns the number of child views.
  size_t child_count() const { return children_.size(); }

  // Returns the parent View, or NULL if this View has no parent.
  View* parent() const { return parent_; }

  // Returns true if |child| is contained within this View's hierarchy, even as
  // an indirect descendant. Will return true if child is also this View.
  bool Contains(View* child);

  // Painting ------------------------------------------------------------------

  // Add all or part of a View's bounds to the enclosing Widget's invalid
  // rectangle. This will result in those areas being re-painted on the next
  // update.
  void Invalidate();
  virtual void InvalidateRect(const gfx::Rect& invalid_rect);

  // Input ---------------------------------------------------------------------

  // Returns true if the specified point is contained within this View or its
  // hit test mask. |point| is in this View's coordinates.
  bool HitTest(const gfx::Point& point) const;

  // Accelerators --------------------------------------------------------------

  // Accelerator Registration.
  void AddAccelerator(const Accelerator& accelerator);
  void RemoveAccelerator(const Accelerator& accelerator);
  void RemoveAllAccelerators();

  // Focus ---------------------------------------------------------------------

  // Manager.
  FocusManager* GetFocusManager() const;

  // Traversal.
  virtual FocusTraversable* GetFocusTraversable() const;
  View* GetNextFocusableView() const;
  View* GetPreviousFocusableView() const;

  // Attributes.
  void set_focusable(bool focusable) { focusable_ = focusable; }
  bool IsFocusable() const;

  bool HasFocus() const;
  void RequestFocus();

  // Context menus -------------------------------------------------------------

  void set_context_menu_controller(
      ContextMenuController* context_menu_controller) {
    context_menu_controller_ = context_menu_controller;
  }

  // Resources -----------------------------------------------------------------

  ThemeProvider* GetThemeProvider() const;

 protected:
  // Tree operations -----------------------------------------------------------

  // Called on every view in the hierarchy when a view is added or removed.
  virtual void OnViewAdded(View* parent, View* child);
  virtual void OnViewRemoved(View* parent, View* child);

  // Called on a View when it is added or removed from a Widget.
  virtual void OnViewAddedToWidget();
  virtual void OnViewRemovedFromWidget();

  // Painting ------------------------------------------------------------------

  // Responsible for calling Paint() on child Views. Override to control the
  // order child Views are painted.
  virtual void PaintChildren(gfx::Canvas* canvas);

  // Override to provide rendering in any part of the View's bounds. Typically
  // this is the "contents" of the view. If you override this method you will
  // have to call the subsequent OnPaint*() methods manually.
  virtual void OnPaint(gfx::Canvas* canvas);

  // Override to paint a background before any content is drawn. Typically this
  // is done if you are satisfied with a default OnPaint handler but wish to
  // supply a different background.
  virtual void OnPaintBackground(gfx::Canvas* canvas);

  // Override to paint a border not specified by SetBorder().
  virtual void OnPaintBorder(gfx::Canvas* canvas);

  // Override to paint a focus border (usually a dotted rectangle) around
  // relevant contents.
  virtual void OnPaintFocusBorder(gfx::Canvas* canvas);

  // Input ---------------------------------------------------------------------

  // Returns the visible View that would like to handle events occurring at the
  // specified |point|, in this View's coordinates.
  // This function is used by the event processing system in the Widget to
  // locate views for event targeting. Override this function if you wish to
  // specify a view other than the one most closely enclosing |point| to receive
  // notifications for events within it.
  virtual View* GetEventHandlerForPoint(const gfx::Point& point) const;

  virtual gfx::NativeCursor GetCursorForPoint(const gfx::Point& point);

  virtual bool OnKeyPressed(const KeyEvent& event);
  virtual bool OnKeyReleased(const KeyEvent& event);
  virtual bool OnMouseWheel(const MouseWheelEvent& event);
  // To receive OnMouseDragged() or OnMouseReleased() events, overriding classes
  // must return true from this function.
  virtual bool OnMousePressed(const MouseEvent& event);
  virtual bool OnMouseDragged(const MouseEvent& event);
  virtual void OnMouseReleased(const MouseEvent& event);
  virtual void OnMouseCaptureLost();
  virtual void OnMouseMoved(const MouseEvent& event);
  virtual void OnMouseEntered(const MouseEvent& event);
  virtual void OnMouseExited(const MouseEvent& event);

  // Accelerators --------------------------------------------------------------

  virtual bool OnAcceleratorPressed(const Accelerator& accelerator);

  // Focus ---------------------------------------------------------------------

  virtual bool SkipDefaultKeyEventProcessing(const KeyEvent& event) const;
  virtual bool IsGroupFocusTraversable() const;
  // TODO(beng): kill these, move to focus manager.
  virtual bool IsFocusableInRootView() const;
  virtual bool IsAccessibilityFocusableInRootView() const;
  virtual FocusTraversable* GetPaneFocusTraversable() const;

  virtual void OnFocus(/* const FocusEvent& event */);
  virtual void OnBlur();

 private:
  friend internal::RootView;
  friend class FocusManager;
  friend class FocusSearch;

  // State collected during a MousePressed event to detect possible drag
  // operations.
  struct DragInfo {
    // Sets possible_drag to false and start_x/y to 0. This is invoked by
    // RootView prior to invoke MousePressed().
    void Reset();

    // Sets possible_drag to true and start_pt to the specified point.
    // This is invoked by the target view if it detects the press may generate
    // a drag.
    void PossibleDrag(const gfx::Point& point);

    // Whether the press may generate a drag.
    bool possible_drag;

    // Position of the mouse press in screen coordinates.
    gfx::Point press_point;
  };

  // Tree operations -----------------------------------------------------------
  void NotifyHierarchyChanged(View* parent, View* child, bool is_add);
  void NotifyHierarchyChangedUp(View* parent, View* child, bool is_add);
  void NotifyHierarchyChangedDown(View* parent, View* child, bool is_add,
                                  bool has_widget);
  void CallViewNotification(View* target,
                            View* parent,
                            View* child,
                            bool is_add,
                            bool has_widget);

  // Painting ------------------------------------------------------------------

  // Called by the framework to paint a View. Performs translation and clipping
  // for View coordinates and language direction as required, allows the View
  // to paint itself via the various OnPaint*() event handlers and then paints
  // the hierarchy beneath it.
  void Paint(gfx::Canvas* canvas);

  // Input --------------------------------------------------------------
  //  These methods are designed to be called by the RootView. The RootView
  //  should limit its interaction with the View to these methods and the public
  //  API.
  bool MousePressed(const MouseEvent& event, DragInfo* drag_info);
  bool MouseDragged(const MouseEvent& event, DragInfo* drag_info);
  void MouseReleased(const MouseEvent& event);

  // Focus ---------------------------------------------------------------------

  // Called when |child| is inserted into this View's children_ at |index|.
  // Sets up next/previous focus views
  // TODO(beng): Move this to FocusManager.
  void InitFocusSiblings(View* child, size_t index);

  // Drag & Drop ---------------------------------------------------------------
  int GetDragOperations(const gfx::Point& point);
  void WriteDragData(const gfx::Point& point, OSExchangeData* data);
  void StartShellDrag(const MouseEvent& event, const gfx::Point& press_point);

  //////////////////////////////////////////////////////////////////////////////

  // Creation and lifetime -----------------------------------------------------

  // True if the hierarchy (i.e. the parent View) is responsible for deleting
  // this View. Default is true.
  bool parent_owned_;

  // Size and disposition ------------------------------------------------------

  // The bounds of the View, in its parent's coordinates.
  gfx::Rect bounds_;

  scoped_ptr<Border> border_;

  // Whether or not this View is visible. The view still participates in layout
  // but will not be painted.
  bool visible_;

  // Whether or not this View is enabled. When disabled, the event system will
  // not propagate un-handled events beyond the View in the hierarchy.
  bool enabled_;

  // An optional helper that handles layout for child views.
  scoped_ptr<LayoutManager> layout_manager_;

  // Attributes ----------------------------------------------------------------

  // An identifier for this View. Caller must guarantee uniqueness.
  int id_;

  // An identifier for a group of potentially related Views.
  int group_;

  // Tree operations -----------------------------------------------------------

  // The View's parent view. This is set and reset when the View is added and
  // removed from a hierarchy.
  View* parent_;

  // The View's children.
  ViewVector children_;

  // Focus ---------------------------------------------------------------------

  // True if this View is focusable by the FocusManager.
  bool focusable_;

  // Focus siblings for this View.
  View* next_focusable_view_;
  View* prev_focusable_view_;

  // Context menus -------------------------------------------------------------

  // Shows the context menu.
  ContextMenuController* context_menu_controller_;

  // Drag & drop ---------------------------------------------------------------

  // Delegate for drag and drop related functionality.
  DragController* drag_controller_;

  DISALLOW_COPY_AND_ASSIGN(View);
};

}  // namespace ui

#endif  // UI_VIEWS_VIEW_H_

/*

TODO(beng):
- accessibility
- scrolling
- cursors
- tooltips
- rtl
- l10n
- mousewheel
- more on painting
- layer stuff
- investigate why assorted notifications are necessary
- native_widget_views
- native_widget_gtk

*/