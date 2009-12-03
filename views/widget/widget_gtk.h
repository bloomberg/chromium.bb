// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_WIDGET_GTK_H_
#define VIEWS_WIDGET_WIDGET_GTK_H_

#include <gtk/gtk.h>

#include "app/active_window_watcher_x.h"
#include "base/gfx/size.h"
#include "base/message_loop.h"
#include "views/focus/focus_manager.h"
#include "views/widget/widget.h"

class OSExchangeData;
class OSExchangeDataProviderGtk;

namespace gfx {
class Rect;
}

namespace views {

class DefaultThemeProvider;
class DropTargetGtk;
class TooltipManagerGtk;
class View;
class WindowGtk;

// Widget implementation for GTK.
class WidgetGtk
    : public Widget,
      public MessageLoopForUI::Observer,
      public FocusTraversable,
      public ActiveWindowWatcherX::Observer {
 public:
  // Type of widget.
  enum Type {
    // Used for popup type windows (bubbles, menus ...).
    TYPE_POPUP,
    // A top level window with no title, no control buttons.
    // control.
    TYPE_WINDOW,
    // A top level, decorated window.
    TYPE_DECORATED_WINDOW,
    // A child widget.
    TYPE_CHILD
  };

  explicit WidgetGtk(Type type);
  virtual ~WidgetGtk();

  // Marks this window as transient to its parent. A window that is transient
  // to its parent results in the parent rendering active when the child is
  // active.
  // This must be invoked before Init. This is only used for types other than
  // TYPE_CHILD. The default is false.
  // See gtk_window_set_transient_for for details.
  void make_transient_to_parent() {
    DCHECK(!widget_);
    transient_to_parent_ = true;
  }

  // Returns the transient parent. See make_transient_to_parent for details on
  // what the transient parent is.
  GtkWindow* GetTransientParent() const;

  // Makes the background of the window totally transparent. This must be
  // invoked before Init. This does a couple of checks and returns true if
  // the window can be made transparent. The actual work of making the window
  // transparent is done by ConfigureWidgetForTransparentBackground.
  bool MakeTransparent();
  bool is_transparent() const { return transparent_; }

  // Sets whether or not we are deleted when the widget is destroyed. The
  // default is true.
  void set_delete_on_destroy(bool delete_on_destroy) {
    delete_on_destroy_ = delete_on_destroy;
  }

  // Adds and removes the specified widget as a child of this widget's contents.
  // These methods make sure to add the widget to the window's contents
  // container if this widget is a window.
  void AddChild(GtkWidget* child);
  void RemoveChild(GtkWidget* child);

  // A safe way to reparent a child widget to this widget. Calls
  // gtk_widget_reparent which handles refcounting to avoid destroying the
  // widget when removing it from its old parent.
  void ReparentChild(GtkWidget* child);

  // Positions a child GtkWidget at the specified location and bounds.
  void PositionChild(GtkWidget* child, int x, int y, int w, int h);

  // Parent GtkWidget all children are added to. When this WidgetGtk corresponds
  // to a top level window, this is the GtkFixed within the GtkWindow, not the
  // GtkWindow itself. For child widgets, this is the same GtkFixed as
  // |widget_|.
  GtkWidget* window_contents() const { return window_contents_; }

  // Starts a drag on this widget. This blocks until the drag is done.
  void DoDrag(const OSExchangeData& data, int operation);

  // Are we in PaintNow? See use in root_view_gtk for details on what this is
  // used for.
  bool in_paint_now() const { return in_paint_now_; }

  // Sets the focus traversable parents.
  void SetFocusTraversableParent(FocusTraversable* parent);
  void SetFocusTraversableParentView(View* parent_view);

  // Invoked when the active status changes.
  virtual void IsActiveChanged() {}

  // Gets the WidgetGtk in the userdata section of the widget.
  static WidgetGtk* GetViewForNative(GtkWidget* widget);

  // Gets the WindowGtk in the userdata section of the widget.
  // TODO(beng): move to WindowGtk
  static WindowGtk* GetWindowForNative(GtkWidget* widget);

  // Sets the drop target to NULL. This is invoked by DropTargetGTK when the
  // drop is done.
  void ResetDropTarget();

  // Returns the RootView for |widget|.
  static RootView* GetRootViewForWidget(GtkWidget* widget);

  // Overriden from ActiveWindowWatcherX::Observer.
  virtual void ActiveWindowChanged(GdkWindow* active_window);

  // Overridden from Widget:
  virtual void Init(gfx::NativeView parent, const gfx::Rect& bounds);
  virtual void SetContentsView(View* view);
  virtual void GetBounds(gfx::Rect* out, bool including_frame) const;
  virtual void SetBounds(const gfx::Rect& bounds);
  virtual void MoveAbove(Widget* other);
  virtual void SetShape(gfx::NativeRegion region);
  virtual void Close();
  virtual void CloseNow();
  virtual void Show();
  virtual void Hide();
  virtual gfx::NativeView GetNativeView() const;
  virtual void PaintNow(const gfx::Rect& update_rect);
  virtual void SetOpacity(unsigned char opacity);
  virtual void SetAlwaysOnTop(bool on_top);
  virtual RootView* GetRootView();
  virtual Widget* GetRootWidget() const;
  virtual bool IsVisible() const;
  virtual bool IsActive() const;
  virtual void GenerateMousePressedForView(View* view,
                                           const gfx::Point& point);
  virtual TooltipManager* GetTooltipManager();
  virtual bool GetAccelerator(int cmd_id, Accelerator* accelerator);
  virtual Window* GetWindow();
  virtual const Window* GetWindow() const;
  virtual void SetNativeWindowProperty(const std::wstring& name,
                                       void* value);
  virtual void* GetNativeWindowProperty(const std::wstring& name);
  virtual ThemeProvider* GetThemeProvider() const;
  virtual ThemeProvider* GetDefaultThemeProvider() const;
  virtual FocusManager* GetFocusManager();
  virtual void ViewHierarchyChanged(bool is_add, View *parent,
                                    View *child);

  // Overridden from MessageLoopForUI::Observer:
  virtual void WillProcessEvent(GdkEvent* event);
  virtual void DidProcessEvent(GdkEvent* event);

  // Overridden from FocusTraversable:
  virtual View* FindNextFocusableView(View* starting_view,
                                      bool reverse,
                                      Direction direction,
                                      bool check_starting_view,
                                      FocusTraversable** focus_traversable,
                                      View** focus_traversable_view);
  virtual FocusTraversable* GetFocusTraversableParent();
  virtual View* GetFocusTraversableParentView();

 protected:
  // Returns the view::Event::flags for a GdkEventButton.
  static int GetFlagsForEventButton(const GdkEventButton& event);

  // Event handlers:
  virtual void OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation);
  virtual void OnPaint(GtkWidget* widget, GdkEventExpose* event);
  virtual void OnDragDataGet(GdkDragContext* context,
                             GtkSelectionData* data,
                             guint info,
                             guint time);
  virtual void OnDragDataReceived(GdkDragContext* context,
                                  gint x,
                                  gint y,
                                  GtkSelectionData* data,
                                  guint info,
                                  guint time);
  virtual gboolean OnDragDrop(GdkDragContext* context,
                              gint x,
                              gint y,
                              guint time);
  virtual void OnDragEnd(GdkDragContext* context);
  virtual gboolean OnDragFailed(GdkDragContext* context,
                                GtkDragResult result);
  virtual void OnDragLeave(GdkDragContext* context,
                           guint time);
  virtual gboolean OnDragMotion(GdkDragContext* context,
                                gint x,
                                gint y,
                                guint time);
  virtual gboolean OnEnterNotify(GtkWidget* widget, GdkEventCrossing* event);
  virtual gboolean OnLeaveNotify(GtkWidget* widget, GdkEventCrossing* event);
  virtual gboolean OnMotionNotify(GtkWidget* widget, GdkEventMotion* event);
  virtual gboolean OnButtonPress(GtkWidget* widget, GdkEventButton* event);
  virtual gboolean OnButtonRelease(GtkWidget* widget, GdkEventButton* event);
  virtual gboolean OnFocusIn(GtkWidget* widget, GdkEventFocus* event);
  virtual gboolean OnFocusOut(GtkWidget* widget, GdkEventFocus* event);
  virtual gboolean OnKeyPress(GtkWidget* widget, GdkEventKey* event);
  virtual gboolean OnKeyRelease(GtkWidget* widget, GdkEventKey* event);
  virtual gboolean OnQueryTooltip(gint x,
                                  gint y,
                                  gboolean keyboard_mode,
                                  GtkTooltip* tooltip);
  virtual gboolean OnScroll(GtkWidget* widget, GdkEventScroll* event) {
    return false;
  }
  virtual gboolean OnVisibilityNotify(GtkWidget* widget,
                                      GdkEventVisibility* event) {
    return false;
  }
  virtual gboolean OnGrabBrokeEvent(GtkWidget* widget, GdkEvent* event);
  virtual void OnGrabNotify(GtkWidget* widget, gboolean was_grabbed);
  virtual void OnDestroy();

  void set_mouse_down(bool mouse_down) { is_mouse_down_ = mouse_down; }

  // Do we own the mouse grab?
  bool has_capture() const { return has_capture_; }

  // Returns whether capture should be released on mouse release. The default
  // is true.
  virtual bool ReleaseCaptureOnMouseReleased() { return true; }

  // Does a mouse grab on this widget.
  void DoGrab();

  // Releases a grab done by this widget.
  virtual void ReleaseGrab();

  // Sets the WindowGtk in the userdata section of the widget.
  static void SetWindowForNative(GtkWidget* widget, WindowGtk* window);

  // Are we a subclass of WindowGtk?
  bool is_window_;

 private:
  class DropObserver;
  friend class DropObserver;

  virtual RootView* CreateRootView();

  void OnWindowPaint(GtkWidget* widget, GdkEventExpose* event);

  // Process a mouse click.
  bool ProcessMousePressed(GdkEventButton* event);
  void ProcessMouseReleased(GdkEventButton* event);

  static void SetRootViewForWidget(GtkWidget* widget, RootView* root_view);

  // A set of static signal handlers that bridge.
  static gboolean CallButtonPress(GtkWidget* widget, GdkEventButton* event);
  static gboolean CallButtonRelease(GtkWidget* widget, GdkEventButton* event);
  static gboolean CallDragDrop(GtkWidget* widget,
                               GdkDragContext* context,
                               gint x,
                               gint y,
                               guint time,
                               WidgetGtk* host);
  static gboolean CallDragFailed(GtkWidget* widget,
                                 GdkDragContext* context,
                                 GtkDragResult result,
                                 WidgetGtk* host);
  static gboolean CallDragMotion(GtkWidget* widget,
                                 GdkDragContext* context,
                                 gint x,
                                 gint y,
                                 guint time,
                                 WidgetGtk* host);
  static gboolean CallEnterNotify(GtkWidget* widget, GdkEventCrossing* event);
  static gboolean CallFocusIn(GtkWidget* widget, GdkEventFocus* event);
  static gboolean CallFocusOut(GtkWidget* widget, GdkEventFocus* event);
  static gboolean CallGrabBrokeEvent(GtkWidget* widget, GdkEvent* event);
  static gboolean CallKeyPress(GtkWidget* widget, GdkEventKey* event);
  static gboolean CallKeyRelease(GtkWidget* widget, GdkEventKey* event);
  static gboolean CallLeaveNotify(GtkWidget* widget, GdkEventCrossing* event);
  static gboolean CallMotionNotify(GtkWidget* widget, GdkEventMotion* event);
  static gboolean CallPaint(GtkWidget* widget, GdkEventExpose* event);
  static gboolean CallQueryTooltip(GtkWidget* widget,
                                   gint x,
                                   gint y,
                                   gboolean keyboard_mode,
                                   GtkTooltip* tooltip,
                                   WidgetGtk* host);
  static gboolean CallScroll(GtkWidget* widget, GdkEventScroll* event);
  static gboolean CallVisibilityNotify(GtkWidget* widget,
                                       GdkEventVisibility* event);
  static gboolean CallWindowPaint(GtkWidget* widget,
                                  GdkEventExpose* event,
                                  WidgetGtk* widget_gtk);
  static void CallDestroy(GtkObject* object);
  static void CallDragDataGet(GtkWidget* widget,
                              GdkDragContext* context,
                              GtkSelectionData* data,
                              guint info,
                              guint time,
                              WidgetGtk* host);
  static void CallDragDataReceived(GtkWidget* widget,
                                   GdkDragContext* context,
                                   gint x,
                                   gint y,
                                   GtkSelectionData* data,
                                   guint info,
                                   guint time,
                                   WidgetGtk* host);
  static void CallDragEnd(GtkWidget* widget,
                          GdkDragContext* context,
                          WidgetGtk* host);
  static void CallDragLeave(GtkWidget* widget,
                            GdkDragContext* context,
                            guint time,
                            WidgetGtk* host);
  static void CallGrabNotify(GtkWidget* widget, gboolean was_grabbed);
  static void CallSizeAllocate(GtkWidget* widget, GtkAllocation* allocation);

  // Returns the first ancestor of |widget| that is a window.
  static Window* GetWindowImpl(GtkWidget* widget);

  // Creates the GtkWidget.
  void CreateGtkWidget(GtkWidget* parent, const gfx::Rect& bounds);

  // Invoked from create widget to enable the various bits needed for a
  // transparent background. This is only invoked if MakeTransparent has been
  // invoked.
  void ConfigureWidgetForTransparentBackground();

  // TODO(sky): documentation
  void HandleGrabBroke();

  const Type type_;

  // Our native views. If we're a window/popup, then widget_ is the window and
  // window_contents_ is a GtkFixed. If we're not a window/popup, then widget_
  // and window_contents_ point to the same GtkFixed.
  GtkWidget* widget_;
  GtkWidget* window_contents_;

  // Child GtkWidgets created with no parent need to be parented to a valid top
  // level window otherwise Gtk throws a fit. |null_parent_| is an invisible
  // popup that such GtkWidgets are parented to.
  static GtkWidget* null_parent_;

  // The TooltipManager.
  // WARNING: RootView's destructor calls into the TooltipManager. As such, this
  // must be destroyed AFTER root_view_.
  scoped_ptr<TooltipManagerGtk> tooltip_manager_;

  scoped_ptr<DropTargetGtk> drop_target_;

  // The focus manager keeping track of focus for this Widget and any of its
  // children.  NULL for non top-level widgets.
  // WARNING: RootView's destructor calls into the FocusManager. As such, this
  // must be destroyed AFTER root_view_.
  scoped_ptr<FocusManager> focus_manager_;

  // The root of the View hierarchy attached to this window.
  scoped_ptr<RootView> root_view_;

  // If true, the mouse is currently down.
  bool is_mouse_down_;

  // Have we done a mouse grab?
  bool has_capture_;

  // The following are used to detect duplicate mouse move events and not
  // deliver them. Displaying a window may result in the system generating
  // duplicate move events even though the mouse hasn't moved.

  // If true, the last event was a mouse move event.
  bool last_mouse_event_was_move_;

  // Coordinates of the last mouse move event, in screen coordinates.
  int last_mouse_move_x_;
  int last_mouse_move_y_;

  // The following factory is used to delay destruction.
  ScopedRunnableMethodFactory<WidgetGtk> close_widget_factory_;

  // See description above setter.
  bool delete_on_destroy_;

  // See description above make_transparent for details.
  bool transparent_;

  scoped_ptr<DefaultThemeProvider> default_theme_provider_;

  // See note in DropObserver for details on this.
  bool ignore_drag_leave_;

  unsigned char opacity_;

  // This is non-null during the life of DoDrag and contains the actual data
  // for the drag.
  const OSExchangeDataProviderGtk* drag_data_;

  // See description above getter for details.
  bool in_paint_now_;

  // Are we active?
  bool is_active_;

  // See make_transient_to_parent for a description.
  bool transient_to_parent_;

  // Last size supplied to OnSizeAllocate. We cache this as any time the
  // size of a GtkWidget changes size_allocate is called, even if the size
  // didn't change. If we didn't cache this and ignore calls when the size
  // hasn't changed, we can end up getting stuck in a never ending loop.
  gfx::Size size_;

  DISALLOW_COPY_AND_ASSIGN(WidgetGtk);
};

}  // namespace views

#endif  // VIEWS_WIDGET_WIDGET_GTK_H_
