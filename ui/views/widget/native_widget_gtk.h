// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_NATIVE_WIDGET_GTK_H_
#define UI_VIEWS_WIDGET_NATIVE_WIDGET_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/message_loop.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/base/x/active_window_watcher_x_observer.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/size.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/widget/native_widget_private.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class Rect;
}

namespace ui {
class OSExchangeData;
class OSExchangeDataProviderGtk;
class GtkSignalRegistrar;
}
using ui::OSExchangeData;
using ui::OSExchangeDataProviderGtk;

namespace views {

class DropTargetGtk;
class InputMethod;
class View;

namespace internal {
class NativeWidgetDelegate;
}

// Widget implementation for GTK.
class VIEWS_EXPORT NativeWidgetGtk : public internal::NativeWidgetPrivate,
                                     public ui::CompositorDelegate,
                                     public ui::ActiveWindowWatcherXObserver {
 public:
  explicit NativeWidgetGtk(internal::NativeWidgetDelegate* delegate);
  virtual ~NativeWidgetGtk();

  // Returns the transient parent. See make_transient_to_parent for details on
  // what the transient parent is.
  GtkWindow* GetTransientParent() const;

  // Makes the background of the window totally transparent. This must be
  // invoked before Init. This does a couple of checks and returns true if
  // the window can be made transparent. The actual work of making the window
  // transparent is done by ConfigureWidgetForTransparentBackground.
  // This works for both child and window types.
  bool MakeTransparent();
  bool is_transparent() const { return transparent_; }

  // Enable/Disable double buffering.This is neccessary to prevent
  // flickering in ScrollView, which has both native and view
  // controls.
  void EnableDoubleBuffer(bool enabled);
  bool is_double_buffered() const { return is_double_buffered_; }

  bool is_ignore_events() const { return ignore_events_; }

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

  // Parent GtkWidget all children are added to. When this NativeWidgetGtk
  // corresponds to a top level window, this is the GtkFixed within the
  // GtkWindow, not the GtkWindow itself. For child widgets, this is the same
  // GtkFixed as |widget_|.
  GtkWidget* window_contents() const { return window_contents_; }

  // Starts a drag on this widget. This blocks until the drag is done.
  void DoDrag(const OSExchangeData& data, int operation);

  // Invoked when the active status changes.
  virtual void OnActiveChanged();

  // Sets the drop target to NULL. This is invoked by DropTargetGTK when the
  // drop is done.
  void ResetDropTarget();

  // Gets the requested size of the widget.  This can be the size
  // stored in properties for a GtkViewsFixed, or in the requisitioned
  // size of other kinds of widgets.
  void GetRequestedSize(gfx::Size* out) const;

  // Overridden from ui::ActiveWindowWatcherXObserver.
  virtual void ActiveWindowChanged(GdkWindow* active_window) OVERRIDE;

  // Handles a keyboard event by sending it to our focus manager.
  // Returns true if it's handled by the focus manager.
  bool HandleKeyboardEvent(const KeyEvent& key);

  // Tells widget not to remove FREEZE_UPDATES property when the
  // widget is painted.  This is used if painting the gtk widget
  // is not enough to show the window and has to wait further like
  // keyboard overlay. Returns true if this is called before
  // FREEZE_UPDATES property is removed, or false otherwise.
  bool SuppressFreezeUpdates();

  // Sets and deletes FREEZE_UPDATES property on given |window|.
  // It adds the property when |enable| is true and remove if false.
  // Calling this method will realize the window if it's not realized yet.
  // This property is used to help WindowManager know when the window
  // is fully painted so that WM can map the fully painted window.
  // The property is based on Owen Taylor's proposal at
  // http://mail.gnome.org/archives/wm-spec-list/2009-June/msg00002.html.
  // This is just a hint to WM, and won't change the behavior for WM
  // which does not support this property.
  static void UpdateFreezeUpdatesProperty(GtkWindow* window, bool enable);

  // Registers a expose handler that removes FREEZE_UPDATES property.
  // If you are adding a GtkWidget with its own GdkWindow that may
  // fill the entire area of the NativeWidgetGtk to the view hierachy, you
  // need use this function to tell WM that when the widget is ready
  // to be shown.
  // Caller of this method do not need to disconnect this because the
  // handler will be removed upon the first invocation of the handler,
  // or when the widget is re-attached, and expose won't be emitted on
  // detached widget.
  static void RegisterChildExposeHandler(GtkWidget* widget);

  // Overridden from internal::NativeWidgetPrivate:
  virtual void InitNativeWidget(const Widget::InitParams& params) OVERRIDE;
  virtual NonClientFrameView* CreateNonClientFrameView() OVERRIDE;
  virtual void UpdateFrameAfterFrameChange() OVERRIDE;
  virtual bool ShouldUseNativeFrame() const OVERRIDE;
  virtual void FrameTypeChanged() OVERRIDE;
  virtual Widget* GetWidget() OVERRIDE;
  virtual const Widget* GetWidget() const OVERRIDE;
  virtual gfx::NativeView GetNativeView() const OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE;
  virtual Widget* GetTopLevelWidget() OVERRIDE;
  virtual const ui::Compositor* GetCompositor() const OVERRIDE;
  virtual ui::Compositor* GetCompositor() OVERRIDE;
  virtual void CalculateOffsetToAncestorWithLayer(
      gfx::Point* offset,
      ui::Layer** layer_parent) OVERRIDE;
  virtual void ReorderLayers() OVERRIDE;
  virtual void ViewRemoved(View* view) OVERRIDE;
  virtual void SetNativeWindowProperty(const char* name, void* value) OVERRIDE;
  virtual void* GetNativeWindowProperty(const char* name) const OVERRIDE;
  virtual TooltipManager* GetTooltipManager() const OVERRIDE;
  virtual bool IsScreenReaderActive() const OVERRIDE;
  virtual void SendNativeAccessibilityEvent(
      View* view,
      ui::AccessibilityTypes::Event event_type) OVERRIDE;
  virtual void SetMouseCapture() OVERRIDE;
  virtual void ReleaseMouseCapture() OVERRIDE;
  virtual bool HasMouseCapture() const OVERRIDE;
  virtual InputMethod* CreateInputMethod() OVERRIDE;
  virtual void CenterWindow(const gfx::Size& size) OVERRIDE;
  virtual void GetWindowPlacement(
      gfx::Rect* bounds,
      ui::WindowShowState* show_state) const OVERRIDE;
  virtual void SetWindowTitle(const string16& title) OVERRIDE;
  virtual void SetWindowIcons(const SkBitmap& window_icon,
                              const SkBitmap& app_icon) OVERRIDE;
  virtual void SetAccessibleName(const string16& name) OVERRIDE;
  virtual void SetAccessibleRole(ui::AccessibilityTypes::Role role) OVERRIDE;
  virtual void SetAccessibleState(ui::AccessibilityTypes::State state) OVERRIDE;
  virtual void BecomeModal() OVERRIDE;
  virtual gfx::Rect GetWindowScreenBounds() const OVERRIDE;
  virtual gfx::Rect GetClientAreaScreenBounds() const OVERRIDE;
  virtual gfx::Rect GetRestoredBounds() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void StackAbove(gfx::NativeView native_view) OVERRIDE;
  virtual void StackAtTop() OVERRIDE;
  virtual void SetShape(gfx::NativeRegion shape) OVERRIDE;
  virtual void Close() OVERRIDE;
  virtual void CloseNow() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void ShowMaximizedWithBounds(
      const gfx::Rect& restored_bounds) OVERRIDE;
  virtual void ShowWithWindowState(ui::WindowShowState window_state) OVERRIDE;
  virtual bool IsVisible() const OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual bool IsActive() const OVERRIDE;
  virtual void SetAlwaysOnTop(bool always_on_top) OVERRIDE;
  virtual void Maximize() OVERRIDE;
  virtual void Minimize() OVERRIDE;
  virtual bool IsMaximized() const OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual void Restore() OVERRIDE;
  virtual void SetFullscreen(bool fullscreen) OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual void SetOpacity(unsigned char opacity) OVERRIDE;
  virtual void SetUseDragFrame(bool use_drag_frame) OVERRIDE;
  virtual bool IsAccessibleWidget() const OVERRIDE;
  virtual void RunShellDrag(View* view,
                            const ui::OSExchangeData& data,
                            int operation) OVERRIDE;
  virtual void SchedulePaintInRect(const gfx::Rect& rect) OVERRIDE;
  virtual void SetCursor(gfx::NativeCursor cursor) OVERRIDE;
  virtual void ClearNativeFocus() OVERRIDE;
  virtual void FocusNativeView(gfx::NativeView native_view) OVERRIDE;
  virtual gfx::Rect GetWorkAreaBoundsInScreen() const OVERRIDE;
  virtual void SetInactiveRenderingDisabled(bool value) OVERRIDE;

 protected:
  // Modifies event coordinates to the targeted widget contained by this widget.
  template<class Event> GdkEvent* TransformEvent(Event* event) {
    GdkWindow* dest = GTK_WIDGET(window_contents_)->window;
    if (event && event->window != dest) {
      gint dest_x, dest_y;
      gdk_window_get_root_origin(dest, &dest_x, &dest_y);
      event->x = event->x_root - dest_x;
      event->y = event->y_root - dest_y;
    }
    return reinterpret_cast<GdkEvent*>(event);
  }

  // Event handlers:
  CHROMEGTK_VIRTUAL_CALLBACK_1(NativeWidgetGtk, gboolean, OnButtonPress,
                       GdkEventButton*);
  CHROMEGTK_VIRTUAL_CALLBACK_1(NativeWidgetGtk, void, OnSizeRequest,
                               GtkRequisition*);
  CHROMEGTK_VIRTUAL_CALLBACK_1(NativeWidgetGtk, void, OnSizeAllocate,
                               GtkAllocation*);
  CHROMEGTK_VIRTUAL_CALLBACK_1(NativeWidgetGtk, gboolean, OnPaint,
                               GdkEventExpose*);
  CHROMEGTK_VIRTUAL_CALLBACK_4(NativeWidgetGtk, void, OnDragDataGet,
                               GdkDragContext*, GtkSelectionData*, guint,
                               guint);
  CHROMEGTK_VIRTUAL_CALLBACK_6(NativeWidgetGtk, void, OnDragDataReceived,
                               GdkDragContext*, gint, gint, GtkSelectionData*,
                               guint, guint);
  CHROMEGTK_VIRTUAL_CALLBACK_4(NativeWidgetGtk, gboolean, OnDragDrop,
                               GdkDragContext*, gint, gint, guint);
  CHROMEGTK_VIRTUAL_CALLBACK_1(NativeWidgetGtk, void, OnDragEnd,
                               GdkDragContext*);
  CHROMEGTK_VIRTUAL_CALLBACK_2(NativeWidgetGtk, gboolean, OnDragFailed,
                               GdkDragContext*, GtkDragResult);
  CHROMEGTK_VIRTUAL_CALLBACK_2(NativeWidgetGtk, void, OnDragLeave,
                               GdkDragContext*, guint);
  CHROMEGTK_VIRTUAL_CALLBACK_4(NativeWidgetGtk, gboolean, OnDragMotion,
                               GdkDragContext*, gint, gint, guint);
  CHROMEGTK_VIRTUAL_CALLBACK_1(NativeWidgetGtk, gboolean, OnEnterNotify,
                               GdkEventCrossing*);
  CHROMEGTK_VIRTUAL_CALLBACK_1(NativeWidgetGtk, gboolean, OnLeaveNotify,
                               GdkEventCrossing*);
  CHROMEGTK_VIRTUAL_CALLBACK_1(NativeWidgetGtk, gboolean, OnMotionNotify,
                               GdkEventMotion*);
  CHROMEGTK_VIRTUAL_CALLBACK_1(NativeWidgetGtk, gboolean, OnButtonRelease,
                               GdkEventButton*);
  CHROMEGTK_VIRTUAL_CALLBACK_1(NativeWidgetGtk, gboolean, OnFocusIn,
                               GdkEventFocus*);
  CHROMEGTK_VIRTUAL_CALLBACK_1(NativeWidgetGtk, gboolean, OnFocusOut,
                               GdkEventFocus*);
  CHROMEGTK_VIRTUAL_CALLBACK_1(NativeWidgetGtk, gboolean, OnEventKey,
                               GdkEventKey*);
  CHROMEGTK_VIRTUAL_CALLBACK_4(NativeWidgetGtk, gboolean, OnQueryTooltip, gint,
                               gint, gboolean, GtkTooltip*);
  CHROMEGTK_VIRTUAL_CALLBACK_1(NativeWidgetGtk, gboolean, OnScroll,
                               GdkEventScroll*);
  CHROMEGTK_VIRTUAL_CALLBACK_1(NativeWidgetGtk, gboolean, OnVisibilityNotify,
                               GdkEventVisibility*);
  CHROMEGTK_VIRTUAL_CALLBACK_1(NativeWidgetGtk, gboolean, OnGrabBrokeEvent,
                               GdkEvent*);
  CHROMEGTK_VIRTUAL_CALLBACK_1(NativeWidgetGtk, void, OnGrabNotify, gboolean);
  CHROMEGTK_VIRTUAL_CALLBACK_0(NativeWidgetGtk, void, OnDestroy);
  CHROMEGTK_VIRTUAL_CALLBACK_0(NativeWidgetGtk, void, OnShow);
  CHROMEGTK_VIRTUAL_CALLBACK_0(NativeWidgetGtk, void, OnMap);
  CHROMEGTK_VIRTUAL_CALLBACK_0(NativeWidgetGtk, void, OnHide);
  CHROMEGTK_VIRTUAL_CALLBACK_1(NativeWidgetGtk, gboolean, OnWindowStateEvent,
                               GdkEventWindowState*);
  CHROMEGTK_VIRTUAL_CALLBACK_1(NativeWidgetGtk, gboolean, OnConfigureEvent,
                               GdkEventConfigure*);

  // Invoked when the widget is destroyed and right before the object
  // destruction. Useful for overriding.
  virtual void OnDestroyed(GObject *where_the_object_was);
  static void OnDestroyedThunk(gpointer data, GObject *where_the_object_was) {
    reinterpret_cast<NativeWidgetGtk*>(data)->OnDestroyed(where_the_object_was);
  }

  // Invoked when gtk grab is stolen by other GtkWidget in the same
  // application.
  virtual void HandleGtkGrabBroke();

  const internal::NativeWidgetDelegate* delegate() const { return delegate_; }
  internal::NativeWidgetDelegate* delegate() { return delegate_; }

 private:
  class DropObserver;
  friend class DropObserver;

  // Overridden from ui::CompositorDelegate
  virtual void ScheduleDraw() OVERRIDE;

  // Overridden from internal::InputMethodDelegate
  virtual void DispatchKeyEventPostIME(const KeyEvent& key) OVERRIDE;

  void SetInitParams(const Widget::InitParams& params);

  // This is called only when the window is transparent.
  CHROMEGTK_CALLBACK_1(NativeWidgetGtk, gboolean, OnWindowPaint,
                       GdkEventExpose*);

  // Callbacks for expose event on child widgets. See the description of
  // RegisterChildChildExposeHandler.
  void OnChildExpose(GtkWidget* child);
  static gboolean ChildExposeHandler(GtkWidget* widget, GdkEventExpose* event);

  // Creates the GtkWidget.
  void CreateGtkWidget(const Widget::InitParams& params);

  // Invoked from create widget to enable the various bits needed for a
  // transparent background. This is only invoked if MakeTransparent has been
  // invoked.
  void ConfigureWidgetForTransparentBackground(GtkWidget* parent);

  // Invoked from create widget to enable the various bits needed for a
  // window which doesn't receive events.
  void ConfigureWidgetForIgnoreEvents();

  // A utility function to draw a transparent background onto the |widget|.
  static void DrawTransparentBackground(GtkWidget* widget,
                                        GdkEventExpose* event);

  // Asks the delegate if any to save the window's location and size.
  void SaveWindowPosition();

  // A delegate implementation that handles events received here.
  // See class documentation for Widget in widget.h for a note about ownership.
  internal::NativeWidgetDelegate* delegate_;

  // Our native views. If we're a window/popup, then widget_ is the window and
  // window_contents_ is a GtkFixed. If we're not a window/popup, then widget_
  // and window_contents_ point to the same GtkFixed.
  GtkWidget* widget_;
  GtkWidget* window_contents_;

  // Child GtkWidgets created with no parent need to be parented to a valid top
  // level window otherwise Gtk throws a fit. |null_parent_| is an invisible
  // popup that such GtkWidgets are parented to.
  static GtkWidget* null_parent_;

  // True if the widget is a child of some other widget.
  bool child_;

  // The TooltipManager.
  // WARNING: RootView's destructor calls into the TooltipManager. As such, this
  // must be destroyed AFTER root_view_.
  scoped_ptr<TooltipManager> tooltip_manager_;

  scoped_ptr<DropTargetGtk> drop_target_;

  // The following factory is used to delay destruction.
  base::WeakPtrFactory<NativeWidgetGtk> close_widget_factory_;

  // See class documentation for Widget in widget.h for a note about ownership.
  Widget::InitParams::Ownership ownership_;

  // See description above make_transparent for details.
  bool transparent_;

  // Makes the window pass all events through to any windows behind it.
  // Set during SetInitParams before the widget is created. The actual work of
  // making the window ignore events is done by ConfigureWidgetForIgnoreEvents.
  bool ignore_events_;

  // See note in DropObserver for details on this.
  bool ignore_drag_leave_;

  unsigned char opacity_;

  // This is non-null during the life of DoDrag and contains the actual data
  // for the drag.
  const OSExchangeDataProviderGtk* drag_data_;

  // True to enable debug painting. Enabling causes the damaged
  // region to be painted to flash in red.
  static bool debug_paint_enabled_;

  // State of the window, such as fullscreen, hidden...
  GdkWindowState window_state_;

  // Are we active?
  bool is_active_;

  // See make_transient_to_parent for a description.
  bool transient_to_parent_;

  // Last size supplied to OnSizeAllocate. We cache this as any time the
  // size of a GtkWidget changes size_allocate is called, even if the size
  // didn't change. If we didn't cache this and ignore calls when the size
  // hasn't changed, we can end up getting stuck in a never ending loop.
  gfx::Size size_;

  // This is initially false and when the first focus-in event is received this
  // is set to true and no additional processing is done. Subsequently when
  // focus-in is received we do the normal focus manager processing.
  //
  // This behavior is necessitated by Gtk/X sending focus events
  // asynchronously. The initial sequence for windows is typically: show,
  // request focus on some widget. Because of async events on Gtk this becomes
  // show, request focus, get focus in event which ends up clearing focus
  // (first request to FocusManager::RestoreFocusedView ends up clearing focus).
  bool got_initial_focus_in_;

  // If true, we've received a focus-in event. If false we've received a
  // focus-out event. We can get multiple focus-out events in a row, we use
  // this to determine whether we should process the event.
  bool has_focus_;

  // If true, the window stays on top of the screen. This is only used
  // for types other than TYPE_CHILD.
  bool always_on_top_;

  // If true, we enable the content widget's double buffering.
  // This is false by default.
  bool is_double_buffered_;

  // Indicates if we should handle the upcoming Alt key release event.
  bool should_handle_menu_key_release_;

  // Valid for the lifetime of StartDragForViewFromMouseEvent, indicates the
  // view the drag started from.
  View* dragged_view_;

  // If the widget has ever been painted. This is used to guarantee
  // that window manager shows the window only after the window is painted.
  bool painted_;

  // The compositor for accelerated drawing.
  scoped_refptr<ui::Compositor> compositor_;

  // Have we done a pointer grab?
  bool has_pointer_grab_;

  // Have we done a keyboard grab?
  bool has_keyboard_grab_;

  // ID of the 'grab-notify' signal. If non-zero we're listening for
  // 'grab-notify' events.
  glong grab_notify_signal_id_;

  // If we were created for a menu.
  bool is_menu_;

  scoped_ptr<ui::GtkSignalRegistrar> signal_registrar_;

  DISALLOW_COPY_AND_ASSIGN(NativeWidgetGtk);
};

}  // namespace views

#endif  // UI_VIEWS_WIDGET_NATIVE_WIDGET_GTK_H_
