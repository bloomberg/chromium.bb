// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_VIEW_H_
#define VIEWS_VIEW_H_
#pragma once

#include "build/build_config.h"

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/i18n/rtl.h"
#include "base/scoped_ptr.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "views/accelerator.h"
#include "views/accessibility/accessibility_types.h"
#include "views/background.h"
#include "views/border.h"

using ui::OSExchangeData;

// TODO(sad): Use platform independent wrapper for transform matrix.
class SkMatrix;
class ViewAccessibility;

namespace gfx {
class Canvas;
class Insets;
class Path;
}

namespace ui {
class ThemeProvider;
}
using ui::ThemeProvider;

namespace views {

class Background;
class Border;
class FocusManager;
class FocusTraversable;
class LayoutManager;
class RootView;
class ScrollView;
class Widget;
class Window;

// ContextMenuController is responsible for showing the context menu for a
// View. To use a ContextMenuController invoke SetContextMenuController on a
// View. When the appropriate user gesture occurs ShowContextMenu is invoked
// on the ContextMenuController.
//
// Setting a ContextMenuController on a view makes the view process mouse
// events.
//
// It is up to subclasses that do their own mouse processing to invoke
// the appropriate ContextMenuController method, typically by invoking super's
// implementation for mouse processing.
//
class ContextMenuController {
 public:
  // Invoked to show the context menu for the source view. If |is_mouse_gesture|
  // is true, |p| is the location of the mouse. If |is_mouse_gesture| is false,
  // this method was not invoked by a mouse gesture and |p| is the recommended
  // location to show the menu at.
  //
  // |p| is in screen coordinates.
  virtual void ShowContextMenu(View* source,
                               const gfx::Point& p,
                               bool is_mouse_gesture) = 0;

 protected:
  virtual ~ContextMenuController() {}
};

// DragController is responsible for writing drag data for a view, as well as
// supplying the supported drag operations. Use DragController if you don't
// want to subclass.

class DragController {
 public:
  // Writes the data for the drag.
  virtual void WriteDragData(View* sender,
                             const gfx::Point& press_pt,
                             OSExchangeData* data) = 0;

  // Returns the supported drag operations (see DragDropTypes for possible
  // values). A drag is only started if this returns a non-zero value.
  virtual int GetDragOperations(View* sender, const gfx::Point& p) = 0;

  // Returns true if a drag operation can be started.
  // |press_pt| represents the coordinates where the mouse was initially
  // pressed down. |p| is the current mouse coordinates.
  virtual bool CanStartDrag(View* sender,
                            const gfx::Point& press_pt,
                            const gfx::Point& p) = 0;

 protected:
  virtual ~DragController() {}
};

/////////////////////////////////////////////////////////////////////////////
//
// View class
//
//   A View is a rectangle within the views View hierarchy. It is the base
//   class for all Views.
//
//   A View is a container of other Views (there is no such thing as a Leaf
//   View - makes code simpler, reduces type conversion headaches, design
//   mistakes etc)
//
//   The View contains basic properties for sizing (bounds), layout (flex,
//   orientation, etc), painting of children and event dispatch.
//
//   The View also uses a simple Box Layout Manager similar to XUL's
//   SprocketLayout system. Alternative Layout Managers implementing the
//   LayoutManager interface can be used to lay out children if required.
//
//   It is up to the subclass to implement Painting and storage of subclass -
//   specific properties and functionality.
//
/////////////////////////////////////////////////////////////////////////////
class View : public AcceleratorTarget {
 public:
#if defined(TOUCH_UI)
  enum TouchStatus {
    TOUCH_STATUS_UNKNOWN = 0,  // Unknown touch status. This is used to indicate
                               // that the touch event was not handled.
    TOUCH_STATUS_START,        // The touch event initiated a touch sequence.
    TOUCH_STATUS_CONTINUE,     // The touch event is part of a previously
                               // started touch sequence.
    TOUCH_STATUS_END,          // The touch event ended the touch sequence.
    TOUCH_STATUS_CANCEL        // The touch event was cancelled, but didn't
                               // terminate the touch sequence.
  };
#endif

  // TO BE MOVED ---------------------------------------------------------------
  // TODO(beng): These methods are to be moved to other files/classes.

  // TODO(beng): delete
  // Set whether this view is hottracked. A disabled view cannot be hottracked.
  // If flag differs from the current value, SchedulePaint is invoked.
  virtual void SetHotTracked(bool flag);

  // TODO(beng): delete
  // Returns whether the view is hot-tracked.
  virtual bool IsHotTracked() const { return false; }

  // TODO(beng): delete
  // Returns whether the view is pushed.
  virtual bool IsPushed() const { return false; }

  // FATE TBD ------------------------------------------------------------------
  // TODO(beng): Figure out what these methods are for and delete them.

  // TODO(beng): this one isn't even google3-style. wth.
  virtual Widget* child_widget() { return NULL; }

  // Creation and lifetime -----------------------------------------------------

  View();
  virtual ~View();

  // Set whether this view is owned by its parent. A view that is owned by its
  // parent is automatically deleted when the parent is deleted. The default is
  // true. Set to false if the view is owned by another object and should not
  // be deleted by its parent.
  void set_parent_owned(bool is_parent_owned) {
    is_parent_owned_ = is_parent_owned;
  }

  // Return whether a view is owned by its parent.
  bool IsParentOwned() const { return is_parent_owned_; }

  // Tree operations -----------------------------------------------------------

  // Get the Widget that hosts this View, if any.
  virtual const Widget* GetWidget() const;
  virtual Widget* GetWidget();

  // Add a child View, optionally at |index|.
  void AddChildView(View* view);
  void AddChildViewAt(View* view, int index);

  // Remove a child view from this view. The view's parent will change to NULL.
  void RemoveChildView(View* views);

  // Remove all child view from this view.  If |delete_views| is true, the views
  // are deleted, unless marked as not parent owned.
  void RemoveAllChildViews(bool delete_views);

  // Returns the View at the specified |index|.
  const View* GetChildViewAt(int index) const;
  View* GetChildViewAt(int index);

  // Get the number of child Views.
  int child_count() const { return static_cast<int>(children_.size()); }
  bool has_children() const { return !children_.empty(); }

  // Get the parent View
  const View* parent() const { return parent_; }
  View* parent() { return parent_; }

  // Returns true if |child| is contained within this View's hierarchy, even as
  // an indirect descendant. Will return true if child is also this View.
  bool Contains(const View* view) const;

  // Returns the index of the specified |view| in this view's children, or -1
  // if the specified view is not a child of this view.
  int GetIndexOf(const View* view) const;

  // TODO(beng): REMOVE (Views need not know about Window).
  // Gets the Widget that most closely contains this View, if any.
  // NOTE: almost all views displayed on screen have a Widget, but not
  // necessarily a Window. This is due to widgets being able to create top
  // level windows (as is done for popups, bubbles and menus).
  virtual const Window* GetWindow() const;
  virtual Window* GetWindow();

  // TODO(beng): REMOVE (TBD)
  // Returns true if the native view |native_view| is contained in the view
  // hierarchy beneath this view.
  virtual bool ContainsNativeView(gfx::NativeView native_view) const;

  // TODO(beng): REMOVE (RootView->internal API)
  // Get the containing RootView
  virtual RootView* GetRootView();

  // Size and disposition ------------------------------------------------------
  // Methods for obtaining and modifying the position and size of the view.
  // Position is in the coordinate system of the view's parent.
  // Position is NOT flipped for RTL. See "RTL positioning" for RTL-sensitive
  // position accessors.
  // Transformations are not applied on the size/position. For example, if
  // bounds is (0, 0, 100, 100) and it is scaled by 0.5 along the X axis, the
  // width will still be 100 (although when painted, it will be 50x50, painted
  // at location (0, 0)).

  void SetBounds(int x, int y, int width, int height);
  void SetBoundsRect(const gfx::Rect& bounds);
  void SetSize(const gfx::Size& size);
  void SetPosition(const gfx::Point& position);
  void SetX(int x);
  void SetY(int y);

  // Override to be notified when the bounds of the view have changed.
  virtual void OnBoundsChanged();

  // No transformation is applied on the size or the locations.
  const gfx::Rect& bounds() const { return bounds_; }
  int x() const { return bounds_.x(); }
  int y() const { return bounds_.y(); }
  int width() const { return bounds_.width(); }
  int height() const { return bounds_.height(); }
  const gfx::Size& size() const { return bounds_.size(); }

  // Returns the bounds of the content area of the view, i.e. the rectangle
  // enclosed by the view's border.
  gfx::Rect GetContentsBounds() const;

  // Returns the bounds of the view in its own coordinates (i.e. position is
  // 0, 0).
  gfx::Rect GetLocalBounds() const;

  // Returns the insets of the current border. If there is no border an empty
  // insets is returned.
  virtual gfx::Insets GetInsets() const;

  // Returns the visible bounds of the receiver in the receivers coordinate
  // system.
  //
  // When traversing the View hierarchy in order to compute the bounds, the
  // function takes into account the mirroring setting for each View and
  // therefore it will return the mirrored version of the visible bounds if
  // need be.
  // TODO(beng): const.
  gfx::Rect GetVisibleBounds();

  // Return the bounds of the View in screen coordinate system.
  gfx::Rect GetScreenBounds() const;

  // Returns the baseline of this view, or -1 if this view has no baseline. The
  // return value is relative to the preferred height.
  virtual int GetBaseline();

  // Get the size the View would like to be, if enough space were available.
  virtual gfx::Size GetPreferredSize();

  // Convenience method that sizes this view to its preferred size.
  void SizeToPreferredSize();

  // Gets the minimum size of the view. View's implementation invokes
  // GetPreferredSize.
  virtual gfx::Size GetMinimumSize();

  // Return the height necessary to display this view with the provided width.
  // View's implementation returns the value from getPreferredSize.cy.
  // Override if your View's preferred height depends upon the width (such
  // as with Labels).
  virtual int GetHeightForWidth(int w);

  // Set whether the receiving view is visible. Painting is scheduled as needed
  virtual void SetVisible(bool flag);

  // Return whether a view is visible
  virtual bool IsVisible() const { return is_visible_; }

  // Return whether a view and its ancestors are visible. Returns true if the
  // path from this view to the root view is visible.
  virtual bool IsVisibleInRootView() const;

  // Set whether this view is enabled. A disabled view does not receive keyboard
  // or mouse inputs. If flag differs from the current value, SchedulePaint is
  // invoked.
  virtual void SetEnabled(bool flag);

  // Returns whether the view is enabled.
  virtual bool IsEnabled() const;

  // Transformations -----------------------------------------------------------

  // Methods for setting transformations for a view (e.g. rotation, scaling).

  const SkMatrix* transform() { return transform_.get(); }

  // Clipping parameters. Clipping happens from the right and/or bottom. The
  // clipping amount is in parent's coordinate system, as in, if the view is
  // rotated, then the clipping will be applied after the rotation (and other
  // transformations, if any).
  void set_clip_x(int x) { clip_x_ = x; }
  void set_clip_y(int y) { clip_y_ = y; }
  void set_clip(int x, int y) { clip_x_ = x; clip_y_ = y; }

  void SetRotation(double degree);

  void SetScaleX(double x);
  void SetScaleY(double y);
  void SetScale(double x, double y);

  void SetTranslateX(int x);
  void SetTranslateY(int y);
  void SetTranslate(int x, int y);

  // Reset the transformation matrix.
  void ResetTransform();

  // RTL positioning -----------------------------------------------------------

  // Methods for accessing the bounds and position of the view, relative to its
  // parent. The position returned is mirrored if the parent view is using a RTL
  // layout.
  //
  // NOTE: in the vast majority of the cases, the mirroring implementation is
  //       transparent to the View subclasses and therefore you should use the
  //       bounds() accessor instead.
  gfx::Rect GetMirroredBounds() const;
  gfx::Point GetMirroredPosition() const;
  int GetMirroredX() const;

  // Given a rectangle specified in this View's coordinate system, the function
  // computes the 'left' value for the mirrored rectangle within this View. If
  // the View's UI layout is not right-to-left, then bounds.x() is returned.
  //
  // UI mirroring is transparent to most View subclasses and therefore there is
  // no need to call this routine from anywhere within your subclass
  // implementation.
  int GetMirroredXForRect(const gfx::Rect& rect) const;

  // Given the X coordinate of a point inside the View, this function returns
  // the mirrored X coordinate of the point if the View's UI layout is
  // right-to-left. If the layout is left-to-right, the same X coordinate is
  // returned.
  //
  // Following are a few examples of the values returned by this function for
  // a View with the bounds {0, 0, 100, 100} and a right-to-left layout:
  //
  // GetMirroredXCoordinateInView(0) -> 100
  // GetMirroredXCoordinateInView(20) -> 80
  // GetMirroredXCoordinateInView(99) -> 1
  int GetMirroredXInView(int x) const;

  // Given a X coordinate and a width inside the View, this function returns
  // the mirrored X coordinate if the View's UI layout is right-to-left. If the
  // layout is left-to-right, the same X coordinate is returned.
  //
  // Following are a few examples of the values returned by this function for
  // a View with the bounds {0, 0, 100, 100} and a right-to-left layout:
  //
  // GetMirroredXCoordinateInView(0, 10) -> 90
  // GetMirroredXCoordinateInView(20, 20) -> 60
  int GetMirroredXWithWidthInView(int x, int w) const;

  // Layout --------------------------------------------------------------------

  // Lay out the child Views (set their bounds based on sizing heuristics
  // specific to the current Layout Manager)
  virtual void Layout();

  // TODO(beng): I think we should remove this.
  // Mark this view and all parents to require a relayout. This ensures the
  // next call to Layout() will propagate to this view, even if the bounds of
  // parent views do not change.
  void InvalidateLayout();

  // Gets/Sets the Layout Manager used by this view to size and place its
  // children.
  // The LayoutManager is owned by the View and is deleted when the view is
  // deleted, or when a new LayoutManager is installed.
  LayoutManager* GetLayoutManager() const;
  void SetLayoutManager(LayoutManager* layout);

  // Attributes ----------------------------------------------------------------

  // The view class name.
  static char kViewClassName[];

  // Return the receiving view's class name. A view class is a string which
  // uniquely identifies the view class. It is intended to be used as a way to
  // find out during run time if a view can be safely casted to a specific view
  // subclass. The default implementation returns kViewClassName.
  virtual std::string GetClassName() const;

  // Returns the first ancestor, starting at this, whose class name is |name|.
  // Returns null if no ancestor has the class name |name|.
  View* GetAncestorWithClassName(const std::string& name);

  // Recursively descends the view tree starting at this view, and returns
  // the first child that it encounters that has the given ID.
  // Returns NULL if no matching child view is found.
  virtual const View* GetViewByID(int id) const;
  virtual View* GetViewByID(int id);

  // Sets and gets the ID for this view.  ID should be unique within the subtree
  // that you intend to search for it.  0 is the default ID for views.
  void SetID(int id);
  int GetID() const;

  // A group id is used to tag views which are part of the same logical group.
  // Focus can be moved between views with the same group using the arrow keys.
  // Groups are currently used to implement radio button mutual exclusion.
  // The group id is immutable once it's set.
  void SetGroup(int gid);
  // Returns the group id of the view, or -1 if the id is not set yet.
  int GetGroup() const;

  // If this returns true, the views from the same group can each be focused
  // when moving focus with the Tab/Shift-Tab key.  If this returns false,
  // only the selected view from the group (obtained with
  // GetSelectedViewForGroup()) is focused.
  virtual bool IsGroupFocusTraversable() const { return true; }

  // Fills the provided vector with all the available views which belong to the
  // provided group.
  void GetViewsWithGroup(int group_id, std::vector<View*>* out);

  // Return the View that is currently selected in the specified group.
  // The default implementation simply returns the first View found for that
  // group.
  virtual View* GetSelectedViewForGroup(int group_id);

  // Coordinate conversion -----------------------------------------------------

  // Note that the utility coordinate conversions functions always operate on
  // the mirrored position of the child Views if the parent View uses a
  // right-to-left UI layout.

  // Convert a point from source coordinate system to dst coordinate system.
  //
  // source is a parent or a child of dst, directly or transitively.
  // If source and dst are not in the same View hierarchy, the result is
  // undefined.
  // Source can be NULL in which case it means the screen coordinate system
  static void ConvertPointToView(const View* src,
                                 const View* dst,
                                 gfx::Point* point);

  // Convert a point from the coordinate system of a View to that of the
  // Widget. This is useful for example when sizing HWND children of the
  // Widget that don't know about the View hierarchy and need to be placed
  // relative to the Widget that is their parent.
  static void ConvertPointToWidget(const View* src, gfx::Point* point);

  // Convert a point from a view Widget to a View dest
  static void ConvertPointFromWidget(const View* dest, gfx::Point* p);

  // Convert a point from the coordinate system of a View to that of the
  // screen. This is useful for example when placing popup windows.
  static void ConvertPointToScreen(const View* src, gfx::Point* point);

  // Applies transformation on the rectangle, which is in the view's coordinate
  // system, to convert it into the parent's coordinate system.
  gfx::Rect ConvertRectToParent(const gfx::Rect& rect) const;

  // Painting ------------------------------------------------------------------

  // Mark all or part of the View's bounds as dirty (needing repaint).
  // |r| is in the View's coordinates.
  // Rectangle |r| should be in the view's coordinate system. The
  // transformations are applied to it to convert it into the parent coordinate
  // system before propagating SchedulePaint up the view hierarchy.
  virtual void SchedulePaint();
  virtual void SchedulePaintInRect(const gfx::Rect& r);

  // Called by the framework to paint a View. Performs translation and clipping
  // for View coordinates and language direction as required, allows the View
  // to paint itself via the various OnPaint*() event handlers and then paints
  // the hierarchy beneath it.
  virtual void Paint(gfx::Canvas* canvas);

  // Paint this View immediately.
  virtual void PaintNow();

  // The background object is owned by this object and may be NULL.
  void set_background(Background* b) { background_.reset(b); }
  const Background* background() const { return background_.get(); }

  // The border object is owned by this object and may be NULL.
  void set_border(Border* b) { border_.reset(b); }
  const Border* border() const { return border_.get(); }

  // Get the theme provider from the parent widget.
  ThemeProvider* GetThemeProvider() const;

  // RTL painting --------------------------------------------------------------

  // This method determines whether the gfx::Canvas object passed to
  // View::Paint() needs to be transformed such that anything drawn on the
  // canvas object during View::Paint() is flipped horizontally.
  //
  // By default, this function returns false (which is the initial value of
  // |flip_canvas_on_paint_for_rtl_ui_|). View subclasses that need to paint on
  // a flipped gfx::Canvas when the UI layout is right-to-left need to call
  // EnableCanvasFlippingForRTLUI().
  bool FlipCanvasOnPaintForRTLUI() const {
    return flip_canvas_on_paint_for_rtl_ui_ ? base::i18n::IsRTL() : false;
  }

  // Enables or disables flipping of the gfx::Canvas during View::Paint().
  // Note that if canvas flipping is enabled, the canvas will be flipped only
  // if the UI layout is right-to-left; that is, the canvas will be flipped
  // only if base::i18n::IsRTL() returns true.
  //
  // Enabling canvas flipping is useful for leaf views that draw a bitmap that
  // needs to be flipped horizontally when the UI layout is right-to-left
  // (views::Button, for example). This method is helpful for such classes
  // because their drawing logic stays the same and they can become agnostic to
  // the UI directionality.
  void EnableCanvasFlippingForRTLUI(bool enable) {
    flip_canvas_on_paint_for_rtl_ui_ = enable;
  }

  // Input ---------------------------------------------------------------------

  // Returns the deepest descendant that contains the specified point.
  virtual View* GetViewForPoint(const gfx::Point& point);

  // Return the cursor that should be used for this view or NULL if
  // the default cursor should be used. The provided point is in the
  // receiver's coordinate system. The caller is responsible for managing the
  // lifetime of the returned object, though that lifetime may vary from
  // platform to platform. On Windows, the cursor is a shared resource but in
  // Gtk, the framework destroys the returned cursor after setting it.
  virtual gfx::NativeCursor GetCursorForPoint(ui::EventType event_type,
                                              const gfx::Point& p);

  // Convenience to test whether a point is within this view's bounds
  virtual bool HitTest(const gfx::Point& l) const;

  // This method is invoked when the user clicks on this view.
  // The provided event is in the receiver's coordinate system.
  //
  // Return true if you processed the event and want to receive subsequent
  // MouseDraggged and MouseReleased events.  This also stops the event from
  // bubbling.  If you return false, the event will bubble through parent
  // views.
  //
  // If you remove yourself from the tree while processing this, event bubbling
  // stops as if you returned true, but you will not receive future events.
  // The return value is ignored in this case.
  //
  // Default implementation returns true if a ContextMenuController has been
  // set, false otherwise. Override as needed.
  //
  virtual bool OnMousePressed(const MouseEvent& event);

  // This method is invoked when the user clicked on this control.
  // and is still moving the mouse with a button pressed.
  // The provided event is in the receiver's coordinate system.
  //
  // Return true if you processed the event and want to receive
  // subsequent MouseDragged and MouseReleased events.
  //
  // Default implementation returns true if a ContextMenuController has been
  // set, false otherwise. Override as needed.
  //
  virtual bool OnMouseDragged(const MouseEvent& event);

  // This method is invoked when the user releases the mouse
  // button. The event is in the receiver's coordinate system.
  //
  // If canceled is true it indicates the mouse press/drag was canceled by a
  // system/user gesture.
  //
  // Default implementation notifies the ContextMenuController is appropriate.
  // Subclasses that wish to honor the ContextMenuController should invoke
  // super.
  virtual void OnMouseReleased(const MouseEvent& event, bool canceled);

  // This method is invoked when the mouse is above this control
  // The event is in the receiver's coordinate system.
  //
  // Default implementation does nothing. Override as needed.
  virtual void OnMouseMoved(const MouseEvent& e);

  // This method is invoked when the mouse enters this control.
  //
  // Default implementation does nothing. Override as needed.
  virtual void OnMouseEntered(const MouseEvent& event);

  // This method is invoked when the mouse exits this control
  // The provided event location is always (0, 0)
  // Default implementation does nothing. Override as needed.
  virtual void OnMouseExited(const MouseEvent& event);

#if defined(TOUCH_UI)
  // This method is invoked for each touch event. Default implementation
  // does nothing. Override as needed.
  virtual TouchStatus OnTouchEvent(const TouchEvent& event);
#endif

  // Set the MouseHandler for a drag session.
  //
  // A drag session is a stream of mouse events starting
  // with a MousePressed event, followed by several MouseDragged
  // events and finishing with a MouseReleased event.
  //
  // This method should be only invoked while processing a
  // MouseDragged or MousePressed event.
  //
  // All further mouse dragged and mouse up events will be sent
  // the MouseHandler, even if it is reparented to another window.
  //
  // The MouseHandler is automatically cleared when the control
  // comes back from processing the MouseReleased event.
  //
  // Note: if the mouse handler is no longer connected to a
  // view hierarchy, events won't be sent.
  //
  virtual void SetMouseHandler(View* new_mouse_handler);

  // Invoked when a key is pressed or released.
  // Subclasser should return true if the event has been processed and false
  // otherwise. If the event has not been processed, the parent will be given a
  // chance.
  virtual bool OnKeyPressed(const KeyEvent& e);
  virtual bool OnKeyReleased(const KeyEvent& e);

  // Invoked when the user uses the mousewheel. Implementors should return true
  // if the event has been processed and false otherwise. This message is sent
  // if the view is focused. If the event has not been processed, the parent
  // will be given a chance.
  virtual bool OnMouseWheel(const MouseWheelEvent& e);

  // Accelerators --------------------------------------------------------------

  // Sets a keyboard accelerator for that view. When the user presses the
  // accelerator key combination, the AcceleratorPressed method is invoked.
  // Note that you can set multiple accelerators for a view by invoking this
  // method several times.
  virtual void AddAccelerator(const Accelerator& accelerator);

  // Removes the specified accelerator for this view.
  virtual void RemoveAccelerator(const Accelerator& accelerator);

  // Removes all the keyboard accelerators for this view.
  virtual void ResetAccelerators();

  // TODO(beng): Move to an AcceleratorTarget override section.
  // Called when a keyboard accelerator is pressed.
  // Derived classes should implement desired behavior and return true if they
  // handled the accelerator.
  virtual bool AcceleratorPressed(const Accelerator& accelerator) {
    return false;
  }

  // Focus ---------------------------------------------------------------------

  // Returns whether this view currently has the focus.
  virtual bool HasFocus();

  // Returns the view that should be selected next when pressing Tab.
  View* GetNextFocusableView();
  const View* GetNextFocusableView() const;

  // Returns the view that should be selected next when pressing Shift-Tab.
  View* GetPreviousFocusableView();

  // Sets the component that should be selected next when pressing Tab, and
  // makes the current view the precedent view of the specified one.
  // Note that by default views are linked in the order they have been added to
  // their container. Use this method if you want to modify the order.
  // IMPORTANT NOTE: loops in the focus hierarchy are not supported.
  void SetNextFocusableView(View* view);

  // Sets whether this view can accept the focus.
  // Note that this is false by default so that a view used as a container does
  // not get the focus.
  virtual void SetFocusable(bool focusable);

  // Returns true if the view is focusable (IsFocusable) and visible in the root
  // view. See also IsFocusable.
  bool IsFocusableInRootView() const;

  // Return whether this view is focusable when the user requires full keyboard
  // access, even though it may not be normally focusable.
  bool IsAccessibilityFocusableInRootView() const;

  // Set whether this view can be made focusable if the user requires
  // full keyboard access, even though it's not normally focusable.
  // Note that this is false by default.
  virtual void set_accessibility_focusable(bool accessibility_focusable) {
    accessibility_focusable_ = accessibility_focusable;
  }

  // Convenience method to retrieve the FocusManager associated with the
  // Widget that contains this view.  This can return NULL if this view is not
  // part of a view hierarchy with a Widget.
  virtual FocusManager* GetFocusManager();

  // Request the keyboard focus. The receiving view will become the
  // focused view.
  virtual void RequestFocus();

  // Invoked when a view is about to be requested for focus due to the focus
  // traversal. Reverse is this request was generated going backward
  // (Shift-Tab).
  virtual void AboutToRequestFocusFromTabTraversal(bool reverse) { }

  // Invoked when a key is pressed before the key event is processed (and
  // potentially eaten) by the focus manager for tab traversal, accelerators and
  // other focus related actions.
  // The default implementation returns false, ensuring that tab traversal and
  // accelerators processing is performed.
  // Subclasses should return true if they want to process the key event and not
  // have it processed as an accelerator (if any) or as a tab traversal (if the
  // key event is for the TAB key).  In that case, OnKeyPressed will
  // subsequently be invoked for that event.
  virtual bool SkipDefaultKeyEventProcessing(const KeyEvent& e) {
    return false;
  }

  // Subclasses that contain traversable children that are not directly
  // accessible through the children hierarchy should return the associated
  // FocusTraversable for the focus traversal to work properly.
  virtual FocusTraversable* GetFocusTraversable() { return NULL; }

  // Subclasses that can act as a "pane" must implement their own
  // FocusTraversable to keep the focus trapped within the pane.
  // If this method returns an object, any view that's a direct or
  // indirect child of this view will always use this FocusTraversable
  // rather than the one from the widget.
  virtual FocusTraversable* GetPaneFocusTraversable() { return NULL; }

  // Tooltips ------------------------------------------------------------------

  // Gets the tooltip for this View. If the View does not have a tooltip,
  // return false. If the View does have a tooltip, copy the tooltip into
  // the supplied string and return true.
  // Any time the tooltip text that a View is displaying changes, it must
  // invoke TooltipTextChanged.
  // |p| provides the coordinates of the mouse (relative to this view).
  virtual bool GetTooltipText(const gfx::Point& p, std::wstring* tooltip);

  // Returns the location (relative to this View) for the text on the tooltip
  // to display. If false is returned (the default), the tooltip is placed at
  // a default position.
  virtual bool GetTooltipTextOrigin(const gfx::Point& p, gfx::Point* loc);

  // Context menus -------------------------------------------------------------

  // Sets the ContextMenuController. Setting this to non-null makes the View
  // process mouse events.
  void SetContextMenuController(ContextMenuController* menu_controller);
  ContextMenuController* GetContextMenuController() {
    return context_menu_controller_;
  }

  // Provides default implementation for context menu handling. The default
  // implementation calls the ShowContextMenu of the current
  // ContextMenuController (if it is not NULL). Overridden in subclassed views
  // to provide right-click menu display triggerd by the keyboard (i.e. for the
  // Chrome toolbar Back and Forward buttons). No source needs to be specified,
  // as it is always equal to the current View.
  virtual void ShowContextMenu(const gfx::Point& p,
                               bool is_mouse_gesture);

  // Drag and drop -------------------------------------------------------------

  // Set/get the DragController. See description of DragController for more
  // information.
  void SetDragController(DragController* drag_controller);
  DragController* GetDragController();

  // During a drag and drop session when the mouse moves the view under the
  // mouse is queried for the drop types it supports by way of the
  // GetDropFormats methods. If the view returns true and the drag site can
  // provide data in one of the formats, the view is asked if the drop data
  // is required before any other drop events are sent. Once the
  // data is available the view is asked if it supports the drop (by way of
  // the CanDrop method). If a view returns true from CanDrop,
  // OnDragEntered is sent to the view when the mouse first enters the view,
  // as the mouse moves around within the view OnDragUpdated is invoked.
  // If the user releases the mouse over the view and OnDragUpdated returns a
  // valid drop, then OnPerformDrop is invoked. If the mouse moves outside the
  // view or over another view that wants the drag, OnDragExited is invoked.
  //
  // Similar to mouse events, the deepest view under the mouse is first checked
  // if it supports the drop (Drop). If the deepest view under
  // the mouse does not support the drop, the ancestors are walked until one
  // is found that supports the drop.

  // Override and return the set of formats that can be dropped on this view.
  // |formats| is a bitmask of the formats defined bye OSExchangeData::Format.
  // The default implementation returns false, which means the view doesn't
  // support dropping.
  virtual bool GetDropFormats(
      int* formats,
      std::set<OSExchangeData::CustomFormat>* custom_formats);

  // Override and return true if the data must be available before any drop
  // methods should be invoked. The default is false.
  virtual bool AreDropTypesRequired();

  // A view that supports drag and drop must override this and return true if
  // data contains a type that may be dropped on this view.
  virtual bool CanDrop(const OSExchangeData& data);

  // OnDragEntered is invoked when the mouse enters this view during a drag and
  // drop session and CanDrop returns true. This is immediately
  // followed by an invocation of OnDragUpdated, and eventually one of
  // OnDragExited or OnPerformDrop.
  virtual void OnDragEntered(const DropTargetEvent& event);

  // Invoked during a drag and drop session while the mouse is over the view.
  // This should return a bitmask of the DragDropTypes::DragOperation supported
  // based on the location of the event. Return 0 to indicate the drop should
  // not be accepted.
  virtual int OnDragUpdated(const DropTargetEvent& event);

  // Invoked during a drag and drop session when the mouse exits the views, or
  // when the drag session was canceled and the mouse was over the view.
  virtual void OnDragExited();

  // Invoked during a drag and drop session when OnDragUpdated returns a valid
  // operation and the user release the mouse.
  virtual int OnPerformDrop(const DropTargetEvent& event);

  // Invoked from DoDrag after the drag completes. This implementation does
  // nothing, and is intended for subclasses to do cleanup.
  virtual void OnDragDone();

  // Returns true if the mouse was dragged enough to start a drag operation.
  // delta_x and y are the distance the mouse was dragged.
  static bool ExceededDragThreshold(int delta_x, int delta_y);

  // Accessibility -------------------------------------------------------------
  // TODO(ctguil): Move all this out to a AccessibleInfo wrapper class.

  // Notify the platform specific accessibility client of changes in the user
  // interface.  This will always raise native notifications.
  virtual void NotifyAccessibilityEvent(AccessibilityTypes::Event event_type);

  // Raise an accessibility notification with an option to also raise a native
  // notification.
  virtual void NotifyAccessibilityEvent(AccessibilityTypes::Event event_type,
      bool send_native_event);

  // Returns the MSAA default action of the current view. The string returned
  // describes the default action that will occur when executing
  // IAccessible::DoDefaultAction. For instance, default action of a button is
  // 'Press'.
  virtual string16 GetAccessibleDefaultAction() { return string16(); }

  // Returns a string containing the mnemonic, or the keyboard shortcut, for a
  // given control.
  virtual string16 GetAccessibleKeyboardShortcut() {
    return string16();
  }

  // Returns a brief, identifying string, containing a unique, readable name of
  // a given control. Sets the input string appropriately, and returns true if
  // successful.
  bool GetAccessibleName(string16* name);

  // Returns the accessibility role of the current view. The role is what
  // assistive technologies (ATs) use to determine what behavior to expect from
  // a given control.
  virtual AccessibilityTypes::Role GetAccessibleRole();

  // Returns the accessibility state of the current view.
  virtual AccessibilityTypes::State GetAccessibleState() {
    return 0;
  }

  // Returns the current value associated with a view.
  virtual string16 GetAccessibleValue() { return string16(); }

  // Assigns a string name to the given control. Needed as a View does not know
  // which name will be associated with it until it is created to be a
  // certain type.
  void SetAccessibleName(const string16& name);

  // Returns an instance of the (platform-specific) accessibility interface for
  // the View.
  ViewAccessibility* GetViewAccessibility();

  // Scrolling -----------------------------------------------------------------
  // TODO(beng): Figure out if this can live somewhere other than View, i.e.
  //             closer to ScrollView.

  // Scrolls the specified region, in this View's coordinate system, to be
  // visible. View's implementation passes the call onto the parent View (after
  // adjusting the coordinates). It is up to views that only show a portion of
  // the child view, such as Viewport, to override appropriately.
  virtual void ScrollRectToVisible(const gfx::Rect& rect);

  // The following methods are used by ScrollView to determine the amount
  // to scroll relative to the visible bounds of the view. For example, a
  // return value of 10 indicates the scrollview should scroll 10 pixels in
  // the appropriate direction.
  //
  // Each method takes the following parameters:
  //
  // is_horizontal: if true, scrolling is along the horizontal axis, otherwise
  //                the vertical axis.
  // is_positive: if true, scrolling is by a positive amount. Along the
  //              vertical axis scrolling by a positive amount equates to
  //              scrolling down.
  //
  // The return value should always be positive and gives the number of pixels
  // to scroll. ScrollView interprets a return value of 0 (or negative)
  // to scroll by a default amount.
  //
  // See VariableRowHeightScrollHelper and FixedRowHeightScrollHelper for
  // implementations of common cases.
  virtual int GetPageScrollIncrement(ScrollView* scroll_view,
                                     bool is_horizontal, bool is_positive);
  virtual int GetLineScrollIncrement(ScrollView* scroll_view,
                                     bool is_horizontal, bool is_positive);

 protected:
  // Size and disposition ------------------------------------------------------

  // Called when the preferred size of a child view changed.  This gives the
  // parent an opportunity to do a fresh layout if that makes sense.
  virtual void ChildPreferredSizeChanged(View* child) {}

  // Invalidates the layout and calls ChildPreferredSizeChanged on the parent
  // if there is one. Be sure to call View::PreferredSizeChanged when
  // overriding such that the layout is properly invalidated.
  virtual void PreferredSizeChanged();

  // Override returning true when the view needs to be notified when its visible
  // bounds relative to the root view may have changed. Only used by
  // NativeViewHost.
  virtual bool NeedsNotificationWhenVisibleBoundsChange() const;

  // Notification that this View's visible bounds relative to the root view may
  // have changed. The visible bounds are the region of the View not clipped by
  // its ancestors. This is used for clipping NativeViewHost.
  virtual void OnVisibleBoundsChanged();

  // TODO(beng): eliminate in protected.
  // Whether this view is enabled.
  bool enabled_;

  // Attributes ----------------------------------------------------------------

  // TODO(beng): Eliminate in protected
  // The id of this View. Used to find this View.
  int id_;

  // TODO(beng): Eliminate in protected
  // The group of this view. Some view subclasses use this id to find other
  // views of the same group. For example radio button uses this information
  // to find other radio buttons.
  int group_;

  // Tree operations -----------------------------------------------------------

  // This method is invoked when the tree changes.
  //
  // When a view is removed, it is invoked for all children and grand
  // children. For each of these views, a notification is sent to the
  // view and all parents.
  //
  // When a view is added, a notification is sent to the view, all its
  // parents, and all its children (and grand children)
  //
  // Default implementation does nothing. Override to perform operations
  // required when a view is added or removed from a view hierarchy
  //
  // parent is the new or old parent. Child is the view being added or
  // removed.
  //
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);

  // When SetVisible() changes the visibility of a view, this method is
  // invoked for that view as well as all the children recursively.
  virtual void VisibilityChanged(View* starting_from, bool is_visible);

  // Called when the native view hierarchy changed.
  // |attached| is true if that view has been attached to a new NativeView
  // hierarchy, false if it has been detached.
  // |native_view| is the NativeView this view was attached/detached from, and
  // |root_view| is the root view associated with the NativeView.
  // Views created without a native view parent don't have a focus manager.
  // When this function is called they could do the processing that requires
  // it - like registering accelerators, for example.
  virtual void NativeViewHierarchyChanged(bool attached,
                                          gfx::NativeView native_view,
                                          RootView* root_view);

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

  // Called by HitTest to see if this View has a custom hit test mask. If the
  // return value is true, GetHitTestMask will be called to obtain the mask.
  // Default value is false, in which case the View will hit-test against its
  // bounds.
  virtual bool HasHitTestMask() const;

  // Called by HitTest to retrieve a mask for hit-testing against. Subclasses
  // override to provide custom shaped hit test regions.
  virtual void GetHitTestMask(gfx::Path* mask) const;

  // Focus ---------------------------------------------------------------------

  // Returns whether this view can accept focus.
  // A view can accept focus if it's enabled, focusable and visible.
  // This method is intended for views to use when calculating preferred size.
  // The FocusManager and other places use IsFocusableInRootView.
  virtual bool IsFocusable() const;

  // Override to be notified when focus has changed either to or from this View.
  virtual void OnFocus();
  virtual void OnBlur();

  // Handle view focus/blur events for this view.
  void Focus();
  void Blur();

  // Whether the view can be focused.
  bool focusable_;

  // Whether this view is focusable if the user requires full keyboard access,
  // even though it may not be normally focusable.
  bool accessibility_focusable_;

  // System events -------------------------------------------------------------

  // Called when the UI theme has changed, overriding allows individual Views to
  // do special cleanup and processing (such as dropping resource caches).
  // To dispatch a theme changed notification, call Widget::ThemeChanged().
  virtual void OnThemeChanged() { }

  // Called when the locale has changed, overriding allows individual Views to
  // update locale-dependent strings.
  // To dispatch a locale changed notification, call Widget::LocaleChanged().
  virtual void OnLocaleChanged() { }

  // Tooltips ------------------------------------------------------------------

  // Views must invoke this when the tooltip text they are to display changes.
  void TooltipTextChanged();

  // Context menus -------------------------------------------------------------

  // Returns the location, in screen coordinates, to show the context menu at
  // when the context menu is shown from the keyboard. This implementation
  // returns the middle of the visible region of this view.
  //
  // This method is invoked when the context menu is shown by way of the
  // keyboard.
  virtual gfx::Point GetKeyboardContextMenuLocation();

  // Drag and drop -------------------------------------------------------------

  // These are cover methods that invoke the method of the same name on
  // the DragController. Subclasses may wish to override rather than install
  // a DragController.
  // See DragController for a description of these methods.
  virtual int GetDragOperations(const gfx::Point& press_pt);
  virtual void WriteDragData(const gfx::Point& press_pt, OSExchangeData* data);

  // Returns whether we're in the middle of a drag session that was initiated
  // by us.
  bool InDrag();

  // Returns how much the mouse needs to move in one direction to start a
  // drag. These methods cache in a platform-appropriate way. These values are
  // used by the public static method ExceededDragThreshold().
  static int GetHorizontalDragThreshold();
  static int GetVerticalDragThreshold();

 private:
  friend class RootView;
  friend class FocusManager;
  friend class ViewStorage;

  // Used to track a drag. RootView passes this into
  // ProcessMousePressed/Dragged.
  struct DragInfo {
    // Sets possible_drag to false and start_x/y to 0. This is invoked by
    // RootView prior to invoke ProcessMousePressed.
    void Reset();

    // Sets possible_drag to true and start_pt to the specified point.
    // This is invoked by the target view if it detects the press may generate
    // a drag.
    void PossibleDrag(const gfx::Point& p);

    // Whether the press may generate a drag.
    bool possible_drag;

    // Coordinates of the mouse press.
    gfx::Point start_pt;
  };

  // Tree operations -----------------------------------------------------------

  // Removes |view| from the hierarchy tree.  If |update_focus_cycle| is true,
  // the next and previous focusable views of views pointing to this view are
  // updated.  If |update_tool_tip| is true, the tooltip is updated.  If
  // |delete_removed_view| is true, the view is also deleted (if it is parent
  // owned).
  void DoRemoveChildView(View* view,
                         bool update_focus_cycle,
                         bool update_tool_tip,
                         bool delete_removed_view);

  // Sets the parent View. This is called automatically by AddChild and is
  // thus private.
  void SetParent(View* parent);

  // Call ViewHierarchyChanged for all child views on all parents
  void PropagateRemoveNotifications(View* parent);

  // Call ViewHierarchyChanged for all children
  void PropagateAddNotifications(View* parent, View* child);

  // Propagates NativeViewHierarchyChanged() notification through all the
  // children.
  void PropagateNativeViewHierarchyChanged(bool attached,
                                           gfx::NativeView native_view,
                                           RootView* root_view);

  // Takes care of registering/unregistering accelerators if
  // |register_accelerators| true and calls ViewHierarchyChanged().
  void ViewHierarchyChangedImpl(bool register_accelerators,
                                bool is_add,
                                View* parent,
                                View* child);

  // Size and disposition ------------------------------------------------------

  // Call VisibilityChanged() recursively for all children.
  void PropagateVisibilityNotifications(View* from, bool is_visible);

  // Registers/unregisters accelerators as necessary and calls
  // VisibilityChanged().
  void VisibilityChangedImpl(View* starting_from, bool is_visible);

  // Responsible for propagating bounds change notifications to relevant
  // views.
  void BoundsChanged();

  // Visible bounds notification registration.
  // When a view is added to a hierarchy, it and all its children are asked if
  // they need to be registered for "visible bounds within root" notifications
  // (see comment on OnVisibleBoundsChanged()). If they do, they are registered
  // with every ancestor between them and the root of the hierarchy.
  static void RegisterChildrenForVisibleBoundsNotification(View* view);
  static void UnregisterChildrenForVisibleBoundsNotification(View* view);
  void RegisterForVisibleBoundsNotification();
  void UnregisterForVisibleBoundsNotification();

  // Adds/removes view to the list of descendants that are notified any time
  // this views location and possibly size are changed.
  void AddDescendantToNotify(View* view);
  void RemoveDescendantToNotify(View* view);

  // Transformations -----------------------------------------------------------

  // Initialize the transform matrix when necessary.
  void InitTransform();

  // Coordinate conersion ------------------------------------------------------

  // This is the actual implementation for ConvertPointToView()
  // Attempts a parent -> child conversion and then a
  // child -> parent conversion if try_other_direction is true
  static void ConvertPointToView(const View* src,
                                 const View* dst,
                                 gfx::Point* point,
                                 bool try_other_direction);

  // Input ---------------------------------------------------------------------

  // RootView invokes these. These in turn invoke the appropriate OnMouseXXX
  // method. If a drag is detected, DoDrag is invoked.
  bool ProcessMousePressed(const MouseEvent& e, DragInfo* drop_info);
  bool ProcessMouseDragged(const MouseEvent& e, DragInfo* drop_info);
  void ProcessMouseReleased(const MouseEvent& e, bool canceled);

#if defined(TOUCH_UI)
  // RootView will invoke this with incoming TouchEvents. Returns the
  // the result of OnTouchEvent.
  TouchStatus ProcessTouchEvent(const TouchEvent& e);
#endif

  // Accelerators --------------------------------------------------------------

  // Registers this view's keyboard accelerators that are not registered to
  // FocusManager yet, if possible.
  void RegisterPendingAccelerators();

  // Unregisters all the keyboard accelerators associated with this view.
  // |leave_data_intact| if true does not remove data from accelerators_ array,
  // so it could be re-registered with other focus manager
  void UnregisterAccelerators(bool leave_data_intact);

  // Focus ---------------------------------------------------------------------

  // Initialize the previous/next focusable views of the specified view relative
  // to the view at the specified index.
  void InitFocusSiblings(View* view, int index);

  // System events -------------------------------------------------------------

  // Used to propagate theme changed notifications from the root view to all
  // views in the hierarchy.
  virtual void PropagateThemeChanged();

  // Used to propagate locale changed notifications from the root view to all
  // views in the hierarchy.
  virtual void PropagateLocaleChanged();

  // Tooltips ------------------------------------------------------------------

  // Propagates UpdateTooltip() to the TooltipManager for the Widget.
  // This must be invoked any time the View hierarchy changes in such a way
  // the view under the mouse differs. For example, if the bounds of a View is
  // changed, this is invoked. Similarly, as Views are added/removed, this
  // is invoked.
  void UpdateTooltip();

  // Drag and drop -------------------------------------------------------------

  // Starts a drag and drop operation originating from this view. This invokes
  // WriteDragData to write the data and GetDragOperations to determine the
  // supported drag operations. When done, OnDragDone is invoked.
  void DoDrag(const MouseEvent& e, const gfx::Point& press_pt);

  //////////////////////////////////////////////////////////////////////////////

  // Creation and lifetime -----------------------------------------------------

  // Whether this view is owned by its parent.
  bool is_parent_owned_;

  // Tree operations -----------------------------------------------------------

  // This view's parent
  View* parent_;

  // This view's children.
  typedef std::vector<View*> ViewVector;
  ViewVector children_;

  // Size and disposition ------------------------------------------------------

  // This View's bounds in the parent coordinate system.
  gfx::Rect bounds_;

  // Visible state
  bool is_visible_;

  // Whether or not RegisterViewForVisibleBoundsNotification on the RootView
  // has been invoked.
  bool registered_for_visible_bounds_notification_;

  // List of descendants wanting notification when their visible bounds change.
  scoped_ptr<ViewVector> descendants_to_notify_;

  // Transformations -----------------------------------------------------------

  // The transformation matrix (rotation, translate, scale).
  scoped_ptr<SkMatrix> transform_;

  // Clipping parameters. skia transformation matrix does not give us clipping.
  // So we do it ourselves.
  int clip_x_;
  int clip_y_;

  // Layout --------------------------------------------------------------------

  // Whether the view needs to be laid out.
  bool needs_layout_;

  // The View's LayoutManager defines the sizing heuristics applied to child
  // Views. The default is absolute positioning according to bounds_.
  scoped_ptr<LayoutManager> layout_manager_;

  // Painting ------------------------------------------------------------------

  // Background
  scoped_ptr<Background> background_;

  // Border.
  scoped_ptr<Border> border_;

  // RTL painting --------------------------------------------------------------

  // Indicates whether or not the gfx::Canvas object passed to View::Paint()
  // is going to be flipped horizontally (using the appropriate transform) on
  // right-to-left locales for this View.
  bool flip_canvas_on_paint_for_rtl_ui_;

  // Accelerators --------------------------------------------------------------

  // true if when we were added to hierarchy we were without focus manager
  // attempt addition when ancestor chain changed.
  bool accelerator_registration_delayed_;

  // Focus manager accelerators registered on.
  FocusManager* accelerator_focus_manager_;

  // The list of accelerators. List elements in the range
  // [0, registered_accelerator_count_) are already registered to FocusManager,
  // and the rest are not yet.
  scoped_ptr<std::vector<Accelerator> > accelerators_;
  size_t registered_accelerator_count_;

  // Focus ---------------------------------------------------------------------

  // Next view to be focused when the Tab key is pressed.
  View* next_focusable_view_;

  // Next view to be focused when the Shift-Tab key combination is pressed.
  View* previous_focusable_view_;

  // Context menus -------------------------------------------------------------

  // The menu controller.
  ContextMenuController* context_menu_controller_;

  // Drag and drop -------------------------------------------------------------

  DragController* drag_controller_;

  // Accessibility -------------------------------------------------------------

  // Name for this view, which can be retrieved by accessibility APIs.
  string16 accessible_name_;

#if defined(OS_WIN)
  // The accessibility implementation for this View.
  scoped_refptr<ViewAccessibility> view_accessibility_;
#endif

  DISALLOW_COPY_AND_ASSIGN(View);
};

}  // namespace views

#endif  // VIEWS_VIEW_H_
