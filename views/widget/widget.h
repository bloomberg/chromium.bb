// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_WIDGET_H_
#define VIEWS_WIDGET_WIDGET_H_
#pragma once

#include <vector>

#include "base/scoped_ptr.h"
#include "ui/gfx/native_widget_types.h"
#include "views/focus/focus_manager.h"

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

class DefaultThemeProvider;
class RootView;
class TooltipManager;
class View;
class WidgetDelegate;
class Window;

////////////////////////////////////////////////////////////////////////////////
// Widget class
//
//  Encapsulates the platform-specific rendering, event receiving and widget
//  management aspects of the UI framework.
//
//  Owns a RootView and thus a View hierarchy. Can contain child Widgets.
//  Widget is a platform-independent type that communicates with a platform or
//  context specific NativeWidget implementation.
//
//  TODO(beng): Note that this class being non-abstract means that we have a
//              violation of Google style in that we are using multiple
//              inheritance. The intention is to split this into a separate
//              object associated with but not equal to a NativeWidget
//              implementation. Multiple inheritance is required for this
//              transitional step.
//
class Widget : public FocusTraversable {
 public:
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

  virtual ~Widget();

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

  Widget();

  // Initialize the Widget with a parent and an initial desired size.
  // |contents_view| is the view that will be the single child of RootView
  // within this Widget. As contents_view is inserted into RootView's tree,
  // RootView assumes ownership of this view and cleaning it up. If you remove
  // this view, you are responsible for its destruction. If this value is NULL,
  // the caller is responsible for populating the RootView, and sizing its
  // contents as the window is sized.
  virtual void Init(gfx::NativeView parent, const gfx::Rect& bounds);

  // Initialize the widget with a views::Widget parent and an initial
  // desired size.  This internally invokes |Init(gfx::NativeView,
  // const gfx::Rect&)| but it determines the correct native view
  // for each platform and the type of widget. Passing NULL to
  // |parent| is same as invoking |Init(NULL, bounds)|.
  virtual void InitWithWidget(Widget* parent, const gfx::Rect& bounds);

  // Returns the WidgetDelegate for delegating certain events.
  virtual WidgetDelegate* GetWidgetDelegate();

  // Sets the WidgetDelegate.
  virtual void SetWidgetDelegate(WidgetDelegate* delegate);

  // Sets the specified view as the contents of this Widget. There can only
  // be one contents view child of this Widget's RootView. This view is sized to
  // fit the entire size of the RootView. The RootView takes ownership of this
  // View, unless it is set as not being parent-owned.
  virtual void SetContentsView(View* view);

  // Returns the bounds of this Widget in the screen coordinate system.
  // If the receiving Widget is a frame which is larger than its client area,
  // this method returns the client area if including_frame is false and the
  // frame bounds otherwise. If the receiving Widget is not a frame,
  // including_frame is ignored.
  virtual void GetBounds(gfx::Rect* out, bool including_frame) const;

  // Sizes and/or places the widget to the specified bounds, size or position.
  virtual void SetBounds(const gfx::Rect& bounds);

  // Places the widget in front of the specified widget in z-order.
  virtual void MoveAbove(Widget* widget);

  // Sets a shape on the widget. This takes ownership of shape.
  virtual void SetShape(gfx::NativeRegion shape);

  // Hides the widget then closes it after a return to the message loop.
  virtual void Close();

  // Closes the widget immediately. Compare to |Close|. This will destroy the
  // window handle associated with this Widget, so should not be called from
  // any code that expects it to be valid beyond this call.
  virtual void CloseNow();

  // Shows or hides the widget, without changing activation state.
  virtual void Show();
  virtual void Hide();

  // Returns the gfx::NativeView associated with this Widget.
  virtual gfx::NativeView GetNativeView() const;

  // Sets the opacity of the widget. This may allow widgets behind the widget
  // in the Z-order to become visible, depending on the capabilities of the
  // underlying windowing system. Note that the caller must then schedule a
  // repaint to allow this change to take effect.
  virtual void SetOpacity(unsigned char opacity);

  // Sets the widget to be on top of all other widgets in the windowing system.
  virtual void SetAlwaysOnTop(bool on_top);

  // Returns the RootView contained by this Widget.
  virtual RootView* GetRootView();

  // Returns the Widget associated with the root ancestor.
  virtual Widget* GetRootWidget() const;

  // Returns whether the Widget is visible to the user.
  virtual bool IsVisible() const;

  // Returns whether the Widget is the currently active window.
  virtual bool IsActive() const;

  // Returns whether the Widget is customized for accessibility.
  virtual bool IsAccessibleWidget() const;

  // Starts a drag operation for the specified view. |point| is a position in
  // |view| coordinates that the drag was initiated from.
  virtual void GenerateMousePressedForView(View* view,
                                           const gfx::Point& point);

  // Returns the TooltipManager for this Widget. If this Widget does not support
  // tooltips, NULL is returned.
  virtual TooltipManager* GetTooltipManager();

  // Returns the accelerator given a command id. Returns false if there is
  // no accelerator associated with a given id, which is a common condition.
  virtual bool GetAccelerator(int cmd_id, ui::Accelerator* accelerator);

  // Returns the Window containing this Widget, or NULL if not contained in a
  // window.
  virtual Window* GetWindow();
  virtual const Window* GetWindow() const;

  // Sets/Gets a native window property on the underlying native window object.
  // Returns NULL if the property does not exist. Setting the property value to
  // NULL removes the property.
  virtual void SetNativeWindowProperty(const char* name, void* value);
  virtual void* GetNativeWindowProperty(const char* name);

  // Gets the theme provider.
  virtual ThemeProvider* GetThemeProvider() const;

  // Gets the default theme provider; this is necessary for when a widget has
  // no profile (and ThemeProvider) associated with it. The default theme
  // provider provides a default set of bitmaps that such widgets can use.
  virtual ThemeProvider* GetDefaultThemeProvider() const;

  // Returns the FocusManager for this widget.
  // Note that all widgets in a widget hierarchy share the same focus manager.
  virtual FocusManager* GetFocusManager();

  // Forwarded from the RootView so that the widget can do any cleanup.
  virtual void ViewHierarchyChanged(bool is_add, View* parent, View* child);

  // Returns true if the native view |native_view| is contained in the
  // views::View hierarchy rooted at this widget.
  virtual bool ContainsNativeView(gfx::NativeView native_view);

  // Starts a drag operation for the specified view. This blocks until done.
  // If the view has not been deleted during the drag, OnDragDone is invoked
  // on the view.
  // NOTE: |view| may be NULL.
  virtual void StartDragForViewFromMouseEvent(View* view,
                                              const ui::OSExchangeData& data,
                                              int operation);

  // If a view is dragging, this returns it. Otherwise returns NULL.
  virtual View* GetDraggedView();

  virtual void SchedulePaintInRect(const gfx::Rect& rect);

  virtual void SetCursor(gfx::NativeCursor cursor);

  // Retrieves the focus traversable for this widget.
  virtual FocusTraversable* GetFocusTraversable();

  // Notifies the view hierarchy contained in this widget that theme resources
  // changed.
  virtual void ThemeChanged();

  // Notifies the view hierarchy contained in this widget that locale resources
  // changed.
  virtual void LocaleChanged();

  void SetFocusTraversableParent(FocusTraversable* parent);
  void SetFocusTraversableParentView(View* parent_view);

 protected:
  // Creates the RootView to be used within this Widget. Subclasses may override
  // to create custom RootViews that do specialized event processing.
  // TODO(beng): Investigate whether or not this is needed.
  virtual RootView* CreateRootView();

  // Provided to allow the WidgetWin/Gtk implementations to destroy the RootView
  // _before_ the focus manager/tooltip manager.
  // TODO(beng): remove once we fold those objects onto this one.
  void DestroyRootView();

  // Overridden from FocusTraversable:
  virtual FocusSearch* GetFocusSearch();
  virtual FocusTraversable* GetFocusTraversableParent();
  virtual View* GetFocusTraversableParentView();

 private:
  // Non-owned pointer to the Widget's delegate.  May be NULL if no delegate is
  // being used.
  WidgetDelegate* delegate_;

  // The root of the View hierarchy attached to this window.
  // WARNING: see warning in tooltip_manager_ for ordering dependencies with
  // this and tooltip_manager_.
  scoped_ptr<RootView> root_view_;

  // A theme provider to use when no other theme provider is specified.
  scoped_ptr<DefaultThemeProvider> default_theme_provider_;

  DISALLOW_COPY_AND_ASSIGN(Widget);
};

}  // namespace views

#endif // VIEWS_WIDGET_WIDGET_H_
