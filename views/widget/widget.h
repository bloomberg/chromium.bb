// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_WIDGET_H_
#define VIEWS_WIDGET_WIDGET_H_
#pragma once

#include <vector>

#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Path;
class Point;
class Rect;
}

namespace ui {
class Accelerator;
class OSExchangeData;
class ThemeProvider;
}
using ui::ThemeProvider;

namespace views {

class FocusManager;
class FocusTraversable;
class RootView;
class TooltipManager;
class View;
class WidgetDelegate;
class Window;

////////////////////////////////////////////////////////////////////////////////
//
// Widget interface
//
//   Widget is an abstract class that defines the API that should be implemented
//   by a native window in order to host a view hierarchy.
//
//   Widget wraps a hierarchy of View objects (see view.h) that implement
//   painting and flexible layout within the bounds of the Widget's window.
//
//   The Widget is responsible for handling various system events and forwarding
//   them to the appropriate view.
//
/////////////////////////////////////////////////////////////////////////////

class Widget {
 public:
  virtual ~Widget() { }

  enum TransparencyParam {
    Transparent,
    NotTransparent
  };

  enum EventsParam {
    AcceptEvents,
    NotAcceptEvents
  };

  enum DeleteParam {
    DeleteOnDestroy,
    NotDeleteOnDestroy
  };

  enum MirroringParam {
    MirrorOriginInRTL,
    DontMirrorOriginInRTL
  };

  // Creates a transient popup widget specific to the current platform.
  // If |mirror_in_rtl| is set to MirrorOriginInRTL, the contents of the
  // popup will be mirrored if the current locale is RTL.  You should use
  // DontMirrorOriginInRTL if you are aleady handling the RTL layout within
  // the widget.
  static Widget* CreatePopupWidget(TransparencyParam transparent,
                                   EventsParam accept_events,
                                   DeleteParam delete_on_destroy,
                                   MirroringParam mirror_in_rtl);

  // Returns the root view for |native_window|. If |native_window| does not have
  // a rootview, this recurses through all of |native_window|'s children until
  // one is found. If a root view isn't found, null is returned.
  static RootView* FindRootView(gfx::NativeWindow native_window);

  // Returns list of all root views for the native window and its
  // children.
  static void FindAllRootViews(gfx::NativeWindow native_window,
                               std::vector<RootView*>* root_views);

  // Retrieve the Widget corresponding to the specified native_view, or NULL
  // if there is no such Widget.
  static Widget* GetWidgetFromNativeView(gfx::NativeView native_view);
  static Widget* GetWidgetFromNativeWindow(gfx::NativeWindow native_window);

  // Enumerates all windows pertaining to us and notifies their
  // view hierarchies that the locale has changed.
  static void NotifyLocaleChanged();

  // Initialize the Widget with a parent and an initial desired size.
  // |contents_view| is the view that will be the single child of RootView
  // within this Widget. As contents_view is inserted into RootView's tree,
  // RootView assumes ownership of this view and cleaning it up. If you remove
  // this view, you are responsible for its destruction. If this value is NULL,
  // the caller is responsible for populating the RootView, and sizing its
  // contents as the window is sized.
  virtual void Init(gfx::NativeView parent, const gfx::Rect& bounds) = 0;

  // Initialize the widget with a views::Widget parent and an initial
  // desired size.  This internally invokes |Init(gfx::NativeView,
  // const gfx::Rect&)| but it determines the correct native view
  // for each platform and the type of widget. Passing NULL to
  // |parent| is same as invoking |Init(NULL, bounds)|.
  virtual void InitWithWidget(Widget* parent, const gfx::Rect& bounds) = 0;

  // Returns the WidgetDelegate for delegating certain events.
  virtual WidgetDelegate* GetWidgetDelegate() = 0;

  // Sets the WidgetDelegate.
  virtual void SetWidgetDelegate(WidgetDelegate* delegate) = 0;

  // Sets the specified view as the contents of this Widget. There can only
  // be one contents view child of this Widget's RootView. This view is sized to
  // fit the entire size of the RootView. The RootView takes ownership of this
  // View, unless it is set as not being parent-owned.
  virtual void SetContentsView(View* view) = 0;

  // Returns the bounds of this Widget in the screen coordinate system.
  // If the receiving Widget is a frame which is larger than its client area,
  // this method returns the client area if including_frame is false and the
  // frame bounds otherwise. If the receiving Widget is not a frame,
  // including_frame is ignored.
  virtual void GetBounds(gfx::Rect* out, bool including_frame) const = 0;

  // Sizes and/or places the widget to the specified bounds, size or position.
  virtual void SetBounds(const gfx::Rect& bounds) = 0;

  // Places the widget in front of the specified widget in z-order.
  virtual void MoveAbove(Widget* widget) = 0;

  // Sets a shape on the widget. This takes ownership of shape.
  virtual void SetShape(gfx::NativeRegion shape) = 0;

  // Hides the widget then closes it after a return to the message loop.
  virtual void Close() = 0;

  // Closes the widget immediately. Compare to |Close|. This will destroy the
  // window handle associated with this Widget, so should not be called from
  // any code that expects it to be valid beyond this call.
  virtual void CloseNow() = 0;

  // Shows or hides the widget, without changing activation state.
  virtual void Show() = 0;
  virtual void Hide() = 0;

  // Returns the gfx::NativeView associated with this Widget.
  virtual gfx::NativeView GetNativeView() const = 0;

  // Sets the opacity of the widget. This may allow widgets behind the widget
  // in the Z-order to become visible, depending on the capabilities of the
  // underlying windowing system. Note that the caller must then schedule a
  // repaint to allow this change to take effect.
  virtual void SetOpacity(unsigned char opacity) = 0;

  // Sets the widget to be on top of all other widgets in the windowing system.
  virtual void SetAlwaysOnTop(bool on_top) = 0;

  // Returns the RootView contained by this Widget.
  virtual RootView* GetRootView() = 0;

  // Returns the Widget associated with the root ancestor.
  virtual Widget* GetRootWidget() const = 0;

  // Returns whether the Widget is visible to the user.
  virtual bool IsVisible() const = 0;

  // Returns whether the Widget is the currently active window.
  virtual bool IsActive() const = 0;

  // Returns whether the Widget is customized for accessibility.
  virtual bool IsAccessibleWidget() const = 0;

  // Starts a drag operation for the specified view. |point| is a position in
  // |view| coordinates that the drag was initiated from.
  virtual void GenerateMousePressedForView(View* view,
                                           const gfx::Point& point) = 0;

  // Returns the TooltipManager for this Widget. If this Widget does not support
  // tooltips, NULL is returned.
  virtual TooltipManager* GetTooltipManager() = 0;

  // Returns the accelerator given a command id. Returns false if there is
  // no accelerator associated with a given id, which is a common condition.
  virtual bool GetAccelerator(int cmd_id, ui::Accelerator* accelerator) = 0;

  // Returns the Window containing this Widget, or NULL if not contained in a
  // window.
  virtual Window* GetWindow() = 0;
  virtual const Window* GetWindow() const = 0;

  // Sets/Gets a native window property on the underlying native window object.
  // Returns NULL if the property does not exist. Setting the property value to
  // NULL removes the property.
  virtual void SetNativeWindowProperty(const char* name, void* value) = 0;
  virtual void* GetNativeWindowProperty(const char* name) = 0;

  // Gets the theme provider.
  virtual ThemeProvider* GetThemeProvider() const = 0;

  // Gets the default theme provider; this is necessary for when a widget has
  // no profile (and ThemeProvider) associated with it. The default theme
  // provider provides a default set of bitmaps that such widgets can use.
  virtual ThemeProvider* GetDefaultThemeProvider() const = 0;

  // Returns the FocusManager for this widget.
  // Note that all widgets in a widget hierarchy share the same focus manager.
  virtual FocusManager* GetFocusManager() = 0;

  // Forwarded from the RootView so that the widget can do any cleanup.
  virtual void ViewHierarchyChanged(bool is_add, View *parent,
                                    View *child) = 0;

  // Returns true if the native view |native_view| is contained in the
  // views::View hierarchy rooted at this widget.
  virtual bool ContainsNativeView(gfx::NativeView native_view) = 0;

  // Starts a drag operation for the specified view. This blocks until done.
  // If the view has not been deleted during the drag, OnDragDone is invoked
  // on the view.
  // NOTE: |view| may be NULL.
  virtual void StartDragForViewFromMouseEvent(View* view,
                                              const ui::OSExchangeData& data,
                                              int operation) = 0;

  // If a view is dragging, this returns it. Otherwise returns NULL.
  virtual View* GetDraggedView() = 0;

  virtual void SchedulePaintInRect(const gfx::Rect& rect) = 0;

  virtual void SetCursor(gfx::NativeCursor cursor) = 0;

  // Retrieves the focus traversable for this widget.
  virtual FocusTraversable* GetFocusTraversable() = 0;

  // Notifies the view hierarchy contained in this widget that theme resources
  // changed.
  virtual void ThemeChanged() = 0;

  // Notifies the view hierarchy contained in this widget that locale resources
  // changed.
  virtual void LocaleChanged() = 0;
};

}  // namespace views

#endif // VIEWS_WIDGET_WIDGET_H_
