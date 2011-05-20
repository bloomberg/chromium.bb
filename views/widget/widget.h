// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WIDGET_WIDGET_H_
#define VIEWS_WIDGET_WIDGET_H_
#pragma once

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "ui/base/accessibility/accessibility_types.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect.h"
#include "views/focus/focus_manager.h"
#include "views/widget/native_widget_delegate.h"

namespace gfx {
class Canvas;
class Path;
class Point;
class Rect;
}

namespace ui {
class Accelerator;
class Compositor;
class OSExchangeData;
class ThemeProvider;
}
using ui::ThemeProvider;

namespace views {

class DefaultThemeProvider;
class InputMethod;
class NativeWidget;
class TooltipManager;
class View;
class WidgetDelegate;
class Window;
namespace internal {
class RootView;
}

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
//  A special note on ownership:
//
//    Depending on the value of "delete_on_destroy", the Widget either owns or
//    is owned by its NativeWidget:
//
//    delete_on_destroy = true (default)
//      The Widget instance is owned by its NativeWidget. When the NativeWidget
//      is destroyed (in response to a native destruction message), it deletes
//      the Widget from its destructor.
//    delete_on_destroy = false (non-default)
//      The Widget instance owns its NativeWidget. This state implies someone
//      else wants to control the lifetime of this object. When they destroy
//      the Widget it is responsible for destroying the NativeWidget (from its
//      destructor).
//
class Widget : public internal::NativeWidgetDelegate,
               public FocusTraversable {
 public:
  struct InitParams {
    enum Type {
      TYPE_WINDOW,      // A Window, like a frame window.
      TYPE_CONTROL,     // A control, like a button.
      TYPE_POPUP,       // An undecorated Window, with transient properties.
      TYPE_MENU         // An undecorated Window, with transient properties
                        // specialized to menus.
    };

    InitParams();
    explicit InitParams(Type type);

    Type type;
    bool child;
    bool transient;
    bool transparent;
    bool accept_events;
    bool can_activate;
    bool keep_on_top;
    bool delete_on_destroy;
    bool mirror_origin_in_rtl;
    bool has_dropshadow;
    // Should the widget be double buffered? Default is false.
    bool double_buffer;
    gfx::NativeView parent;
    Widget* parent_widget;
    gfx::Rect bounds;
    NativeWidget* native_widget;
  };
  static InitParams WindowInitParams();

  Widget();
  virtual ~Widget();

  // Enumerates all windows pertaining to us and notifies their
  // view hierarchies that the locale has changed.
  static void NotifyLocaleChanged();

  // Closes all Widgets that aren't identified as "secondary widgets". Called
  // during application shutdown when the last non-secondary widget is closed.
  static void CloseAllSecondaryWidgets();

  // Converts a rectangle from one Widget's coordinate system to another's.
  // Returns false if the conversion couldn't be made, because either these two
  // Widgets do not have a common ancestor or they are not on the screen yet.
  // The value of |*rect| won't be changed when false is returned.
  static bool ConvertRect(const Widget* source,
                          const Widget* target,
                          gfx::Rect* rect);

  // SetPureViews and IsPureViews update and return the state of a global
  // setting that tracks whether to use available pure Views implementations.
  static void SetPureViews(bool pure);
  static bool IsPureViews();

  void Init(const InitParams& params);

  // Unconverted methods -------------------------------------------------------

  // TODO(beng): reorder, they've been converted now.

  // Returns the gfx::NativeView associated with this Widget.
  gfx::NativeView GetNativeView() const;

  // Returns the gfx::NativeWindow associated with this Widget. This may return
  // NULL on some platforms if the widget was created with a type other than
  // TYPE_WINDOW.
  gfx::NativeWindow GetNativeWindow() const;

  // Returns the accelerator given a command id. Returns false if there is
  // no accelerator associated with a given id, which is a common condition.
  virtual bool GetAccelerator(int cmd_id, ui::Accelerator* accelerator);

  // Returns the Window containing this Widget, or NULL if not contained in a
  // window.
  Window* GetContainingWindow();
  const Window* GetContainingWindow() const;

  // Forwarded from the RootView so that the widget can do any cleanup.
  void ViewHierarchyChanged(bool is_add, View* parent, View* child);

  // Performs any necessary cleanup and forwards to RootView.
  void NotifyNativeViewHierarchyChanged(bool attached,
                                        gfx::NativeView native_view);

  // Converted methods ---------------------------------------------------------

  // TODO(beng):
  // Widget subclasses are implementing these methods by implementing
  // NativeWidget. Remove this comment once complete.

  // Returns the topmost Widget in a hierarchy. Will return NULL if called
  // before the underlying Native Widget has been initialized.
  Widget* GetTopLevelWidget();
  const Widget* GetTopLevelWidget() const;

  // Gets/Sets the WidgetDelegate.
  WidgetDelegate* widget_delegate() const { return widget_delegate_; }
  void set_widget_delegate(WidgetDelegate* widget_delegate) {
    widget_delegate_ = widget_delegate;
  }

  // Sets the specified view as the contents of this Widget. There can only
  // be one contents view child of this Widget's RootView. This view is sized to
  // fit the entire size of the RootView. The RootView takes ownership of this
  // View, unless it is set as not being parent-owned.
  void SetContentsView(View* view);

  // Returns the bounds of the Widget in screen coordinates.
  gfx::Rect GetWindowScreenBounds() const;

  // Returns the bounds of the Widget's client area in screen coordinates.
  gfx::Rect GetClientAreaScreenBounds() const;

  // Sizes and/or places the widget to the specified bounds, size or position.
  void SetBounds(const gfx::Rect& bounds);
  void SetSize(const gfx::Size& size);

  // Places the widget in front of the specified widget in z-order.
  void MoveAboveWidget(Widget* widget);
  void MoveAbove(gfx::NativeView native_view);

  // Sets a shape on the widget. This takes ownership of shape.
  void SetShape(gfx::NativeRegion shape);

  // Hides the widget then closes it after a return to the message loop.
  virtual void Close();

  // TODO(beng): Move off public API.
  // Closes the widget immediately. Compare to |Close|. This will destroy the
  // window handle associated with this Widget, so should not be called from
  // any code that expects it to be valid beyond this call.
  void CloseNow();

  // Shows or hides the widget, without changing activation state.
  void Show();
  void Hide();

  // Sets the opacity of the widget. This may allow widgets behind the widget
  // in the Z-order to become visible, depending on the capabilities of the
  // underlying windowing system. Note that the caller must then schedule a
  // repaint to allow this change to take effect.
  void SetOpacity(unsigned char opacity);

  // Sets the widget to be on top of all other widgets in the windowing system.
  void SetAlwaysOnTop(bool on_top);

  // Returns the View at the root of the View hierarchy contained by this
  // Widget.
  View* GetRootView();

  // A secondary widget is one that is automatically closed (via Close()) when
  // all non-secondary widgets are closed.
  // Default is true.
  // TODO(beng): This is an ugly API, should be handled implicitly via
  //             transience.
  void set_is_secondary_widget(bool is_secondary_widget) {
    is_secondary_widget_ = is_secondary_widget;
  }
  bool is_secondary_widget() const { return is_secondary_widget_; }

  // Returns whether the Widget is visible to the user.
  bool IsVisible() const;

  // Returns whether the Widget is the currently active window.
  bool IsActive() const;

  // Returns whether the Widget is customized for accessibility.
  bool IsAccessibleWidget() const;

  // Returns the ThemeProvider that provides theme resources for this Widget.
  virtual ThemeProvider* GetThemeProvider() const;

  // Returns the FocusManager for this widget.
  // Note that all widgets in a widget hierarchy share the same focus manager.
  // TODO(beng): remove virtual.
  virtual FocusManager* GetFocusManager();

  // Returns the InputMethod for this widget.
  // Note that all widgets in a widget hierarchy share the same input method.
  InputMethod* GetInputMethod();

  // Returns true if the native view |native_view| is contained in the
  // views::View hierarchy rooted at this widget.
  // TODO(beng): const.
  bool ContainsNativeView(gfx::NativeView native_view);

  // Starts a drag operation for the specified view. This blocks until the drag
  // operation completes. |view| can be NULL.
  // If the view is non-NULL it can be accessed during the drag by calling
  // dragged_view(). If the view has not been deleted during the drag,
  // OnDragDone() is called on it.
  void RunShellDrag(View* view, const ui::OSExchangeData& data, int operation);

  // Returns the view that requested the current drag operation via
  // RunShellDrag(), or NULL if there is no such view or drag operation.
  View* dragged_view() { return dragged_view_; }

  // Adds the specified |rect| in client area coordinates to the rectangle to be
  // redrawn.
  void SchedulePaintInRect(const gfx::Rect& rect);

  // Sets the currently visible cursor. If |cursor| is NULL, the cursor used
  // before the current is restored.
  void SetCursor(gfx::NativeCursor cursor);

  // Resets the last move flag so that we can go around the optimization
  // that disregards duplicate mouse moves when ending animation requires
  // a new hit-test to do some highlighting as in TabStrip::RemoveTabAnimation
  // to cause the close button to highlight.
  void ResetLastMouseMoveFlag();

  // Retrieves the focus traversable for this widget.
  FocusTraversable* GetFocusTraversable();

  // Notifies the view hierarchy contained in this widget that theme resources
  // changed.
  void ThemeChanged();

  // Notifies the view hierarchy contained in this widget that locale resources
  // changed.
  void LocaleChanged();

  void SetFocusTraversableParent(FocusTraversable* parent);
  void SetFocusTraversableParentView(View* parent_view);

  const ui::Compositor* compositor() const { return compositor_.get(); }
  ui::Compositor* compositor() { return compositor_.get(); }

  // Notifies assistive technology that an accessibility event has
  // occurred on |view|, such as when the view is focused or when its
  // value changes. Pass true for |send_native_event| except for rare
  // cases where the view is a native control that's already sending a
  // native accessibility event and the duplicate event would cause
  // problems.
  void NotifyAccessibilityEvent(
      View* view,
      ui::AccessibilityTypes::Event event_type,
      bool send_native_event);

  const NativeWidget* native_widget() const { return native_widget_; }
  NativeWidget* native_widget() { return native_widget_; }

  // Overridden from NativeWidgetDelegate:
  virtual void OnNativeFocus(gfx::NativeView focused_view) OVERRIDE;
  virtual void OnNativeBlur(gfx::NativeView focused_view) OVERRIDE;
  virtual void OnNativeWidgetCreated() OVERRIDE;
  virtual void OnSizeChanged(const gfx::Size& new_size) OVERRIDE;
  virtual bool HasFocusManager() const OVERRIDE;
  virtual bool OnNativeWidgetPaintAccelerated(
      const gfx::Rect& dirty_region) OVERRIDE;
  virtual void OnNativeWidgetPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual bool OnKeyEvent(const KeyEvent& event) OVERRIDE;
  virtual bool OnMouseEvent(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseCaptureLost() OVERRIDE;
  virtual Widget* AsWidget() OVERRIDE;
  virtual const Widget* AsWidget() const OVERRIDE;

  // Overridden from FocusTraversable:
  virtual FocusSearch* GetFocusSearch() OVERRIDE;
  virtual FocusTraversable* GetFocusTraversableParent() OVERRIDE;
  virtual View* GetFocusTraversableParentView() OVERRIDE;

 protected:
  // TODO(beng): Remove NativeWidgetGtk's dependence on the mouse state flags.
  friend class NativeWidgetGtk;

  // Creates the RootView to be used within this Widget. Subclasses may override
  // to create custom RootViews that do specialized event processing.
  // TODO(beng): Investigate whether or not this is needed.
  virtual internal::RootView* CreateRootView();

  // Provided to allow the NativeWidget implementations to destroy the RootView
  // _before_ the focus manager/tooltip manager.
  // TODO(beng): remove once we fold those objects onto this one.
  void DestroyRootView();

  // Used for testing.
  void ReplaceFocusManager(FocusManager* focus_manager);

  // TODO(beng): Remove NativeWidgetGtk's dependence on these:
  // TODO(msw): Make this mouse state member private.
  // If true, the mouse is currently down.
  bool is_mouse_button_pressed_;

  // TODO(beng): Remove NativeWidgetGtk's dependence on these:
  // TODO(msw): Make these mouse state members private.
  // The following are used to detect duplicate mouse move events and not
  // deliver them. Displaying a window may result in the system generating
  // duplicate move events even though the mouse hasn't moved.
  bool last_mouse_event_was_move_;
  gfx::Point last_mouse_event_position_;

 private:
  // Try to create a compositor if one hasn't been created yet.
  void EnsureCompositor();

  // Returns whether capture should be released on mouse release.
  virtual bool ShouldReleaseCaptureOnMouseReleased() const;

  NativeWidget* native_widget_;

  // Non-owned pointer to the Widget's delegate.  May be NULL if no delegate is
  // being used.
  WidgetDelegate* widget_delegate_;

  // The root of the View hierarchy attached to this window.
  // WARNING: see warning in tooltip_manager_ for ordering dependencies with
  // this and tooltip_manager_.
  scoped_ptr<internal::RootView> root_view_;

  // The focus manager keeping track of focus for this Widget and any of its
  // children.  NULL for non top-level widgets.
  // WARNING: RootView's destructor calls into the FocusManager. As such, this
  // must be destroyed AFTER root_view_. This is enforced in DestroyRootView().
  scoped_ptr<FocusManager> focus_manager_;

  // A theme provider to use when no other theme provider is specified.
  scoped_ptr<DefaultThemeProvider> default_theme_provider_;

  // Valid for the lifetime of RunShellDrag(), indicates the view the drag
  // started from.
  View* dragged_view_;

  // The compositor for accelerated drawing.
  scoped_refptr<ui::Compositor> compositor_;

  // See class documentation for Widget above for a note about ownership.
  bool delete_on_destroy_;

  // See set_is_secondary_widget().
  bool is_secondary_widget_;

  DISALLOW_COPY_AND_ASSIGN(Widget);
};

}  // namespace views

#endif // VIEWS_WIDGET_WIDGET_H_
