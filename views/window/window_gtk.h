// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_WINDOW_GTK_H_
#define VIEWS_WINDOW_WINDOW_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "views/widget/widget_gtk.h"
#include "views/window/native_window.h"
#include "views/window/window.h"

namespace gfx {
class Point;
class Size;
};

namespace views {
namespace internal {
class NativeWindowDelegate;
}

class Client;
class WindowDelegate;

// Window implementation for GTK.
class WindowGtk : public WidgetGtk, public NativeWindow, public Window {
 public:
  virtual ~WindowGtk();

  virtual Window* AsWindow();
  virtual const Window* AsWindow() const;

  // Overridden from WidgetGtk:
  virtual gboolean OnButtonPress(GtkWidget* widget, GdkEventButton* event);
  virtual gboolean OnConfigureEvent(GtkWidget* widget,
                                    GdkEventConfigure* event);
  virtual gboolean OnMotionNotify(GtkWidget* widget, GdkEventMotion* event);
  virtual void OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation);
  virtual gboolean OnWindowStateEvent(GtkWidget* widget,
                                      GdkEventWindowState* event);
  virtual gboolean OnLeaveNotify(GtkWidget* widget, GdkEventCrossing* event);
  virtual void IsActiveChanged();
  virtual void SetInitialFocus();

 protected:
  // Overridden from NativeWindow:
  virtual NativeWidget* AsNativeWidget() OVERRIDE;
  virtual const NativeWidget* AsNativeWidget() const OVERRIDE;
  virtual gfx::Rect GetRestoredBounds() const OVERRIDE;
  virtual void ShowNativeWindow(ShowState state) OVERRIDE;
  virtual void BecomeModal() OVERRIDE;
  virtual void CenterWindow(const gfx::Size& size) OVERRIDE;
  virtual void GetWindowBoundsAndMaximizedState(gfx::Rect* bounds,
                                                bool* maximized) const OVERRIDE;
  virtual void EnableClose(bool enable) OVERRIDE;
  virtual void SetWindowTitle(const std::wstring& title) OVERRIDE;
  virtual void SetWindowIcons(const SkBitmap& window_icon,
                              const SkBitmap& app_icon) OVERRIDE;
  virtual void SetAccessibleName(const std::wstring& name) OVERRIDE;
  virtual void SetAccessibleRole(ui::AccessibilityTypes::Role role) OVERRIDE;
  virtual void SetAccessibleState(ui::AccessibilityTypes::State state) OVERRIDE;
  virtual Window* GetWindow() OVERRIDE;
  virtual void SetWindowBounds(const gfx::Rect& bounds,
                               gfx::NativeWindow other_window) OVERRIDE;
  virtual void HideWindow() OVERRIDE;
  virtual void Activate() OVERRIDE;
  virtual void Deactivate() OVERRIDE;
  virtual void Maximize() OVERRIDE;
  virtual void Minimize() OVERRIDE;
  virtual void Restore() OVERRIDE;
  virtual bool IsActive() const OVERRIDE;
  virtual bool IsVisible() const OVERRIDE;
  virtual bool IsMaximized() const OVERRIDE;
  virtual bool IsMinimized() const OVERRIDE;
  virtual void SetFullscreen(bool fullscreen) OVERRIDE;
  virtual bool IsFullscreen() const OVERRIDE;
  virtual void SetAlwaysOnTop(bool always_on_top) OVERRIDE;
  virtual bool IsAppWindow() const OVERRIDE;
  virtual void SetUseDragFrame(bool use_drag_frame) OVERRIDE;
  virtual NonClientFrameView* CreateFrameViewForWindow() OVERRIDE;
  virtual void UpdateFrameAfterFrameChange() OVERRIDE;
  virtual gfx::NativeWindow GetNativeWindow() const OVERRIDE;
  virtual bool ShouldUseNativeFrame() const OVERRIDE;
  virtual void FrameTypeChanged() OVERRIDE;

  // For  the constructor.
  friend class Window;

  // Constructs the WindowGtk. |window_delegate| cannot be NULL.
  explicit WindowGtk(WindowDelegate* window_delegate);

  // Initializes the window to the passed in bounds.
  virtual void InitWindow(GtkWindow* parent, const gfx::Rect& bounds);

  virtual void OnDestroy(GtkWidget* widget);

 private:
  static gboolean CallConfigureEvent(GtkWidget* widget,
                                     GdkEventConfigure* event,
                                     WindowGtk* window_gtk);
  static gboolean CallWindowStateEvent(GtkWidget* widget,
                                       GdkEventWindowState* event,
                                       WindowGtk* window_gtk);

  // Asks the delegate if any to save the window's location and size.
  void SaveWindowPosition();

  // A delegate implementation that handles events received here.
  internal::NativeWindowDelegate* delegate_;

  // Our window delegate.
  WindowDelegate* window_delegate_;

  // The View that provides the non-client area of the window (title bar,
  // window controls, sizing borders etc). To use an implementation other than
  // the default, this class must be subclassed and this value set to the
  // desired implementation before calling |Init|.
  NonClientView* non_client_view_;

  // State of the window, such as fullscreen, hidden...
  GdkWindowState window_state_;

  // Set to true if the window is in the process of closing.
  bool window_closed_;

  DISALLOW_COPY_AND_ASSIGN(WindowGtk);
};

}  // namespace views

#endif  // VIEWS_WINDOW_WINDOW_GTK_H_
