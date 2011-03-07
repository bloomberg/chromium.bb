// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_WINDOW_H_
#define VIEWS_WINDOW_WINDOW_H_
#pragma once

#include "ui/gfx/native_widget_types.h"
#include "views/window/client_view.h"
#include "views/window/native_window_delegate.h"
#include "views/window/non_client_view.h"

namespace gfx {
class Font;
class Rect;
class Size;
}  // namespace gfx

namespace views {

class NativeWindow;
class NonClientFrameView;
class Widget;
class WindowDelegate;

////////////////////////////////////////////////////////////////////////////////
// Window class
//
// Encapsulates window-like behavior. See WindowDelegate.
//
//  TODO(beng): Subclass Widget as part of V2.
//
//  TODO(beng): Note that this class being non-abstract means that we have a
//              violation of Google style in that we are using multiple
//              inheritance. The intention is to split this into a separate
//              object associated with but not equal to a NativeWidget
//              implementation. Multiple inheritance is required for this
//              transitional step.
//
class Window : public internal::NativeWindowDelegate {
 public:
  explicit Window(WindowDelegate* window_delegate);
  virtual ~Window();

  // Creates an instance of an object implementing this interface.
  // TODO(beng): create a version of this function that takes a NativeView, for
  //             constrained windows.
  static Window* CreateChromeWindow(gfx::NativeWindow parent,
                                    const gfx::Rect& bounds,
                                    WindowDelegate* window_delegate);

  // Returns the preferred size of the contents view of this window based on
  // its localized size data. The width in cols is held in a localized string
  // resource identified by |col_resource_id|, the height in the same fashion.
  // TODO(beng): This should eventually live somewhere else, probably closer to
  //             ClientView.
  static int GetLocalizedContentsWidth(int col_resource_id);
  static int GetLocalizedContentsHeight(int row_resource_id);
  static gfx::Size GetLocalizedContentsSize(int col_resource_id,
                                            int row_resource_id);

  // Closes all windows that aren't identified as "app windows" via
  // IsAppWindow. Called during application shutdown when the last "app window"
  // is closed.
  static void CloseAllSecondaryWindows();

  // Used by |CloseAllSecondaryWindows|. If |widget|'s window is a secondary
  // window, the window is closed. If |widget| has no window, it is closed.
  // Does nothing if |widget| is null.
  static void CloseSecondaryWidget(Widget* widget);

  // Retrieves the window's bounds, including its frame.
  virtual gfx::Rect GetBounds() const;

  // Retrieves the restored bounds for the window.
  virtual gfx::Rect GetNormalBounds() const;

  // Sets the Window's bounds. The window is inserted after |other_window| in
  // the window Z-order. If this window is not yet visible, other_window's
  // monitor is used as the constraining rectangle, rather than this window's
  // monitor.
  virtual void SetWindowBounds(const gfx::Rect& bounds,
                               gfx::NativeWindow other_window);

  // Makes the window visible.
  virtual void Show();

  // Hides the window. This does not delete the window, it just hides it. This
  // always hides the window, it is separate from the stack maintained by
  // Push/PopForceHidden.
  virtual void HideWindow();

  // Sets/Gets a native window property on the underlying native window object.
  // Returns NULL if the property does not exist. Setting the property value to
  // NULL removes the property.
  virtual void SetNativeWindowProperty(const char* name, void* value);
  virtual void* GetNativeWindowProperty(const char* name);

#if defined(OS_WIN)
  // TODO(beng): remove these platform-specific methods.

  // Hides the window if it hasn't already been force-hidden. The force hidden
  // count is tracked, so calling multiple times is allowed, you just have to
  // be sure to call PopForceHidden the same number of times.
  virtual void PushForceHidden() = 0;

  // Decrements the force hidden count, showing the window if we have reached
  // the top of the stack. See PushForceHidden.
  virtual void PopForceHidden() = 0;

  // Prevents the window from being rendered as deactivated the next time it is.
  // This state is reset automatically as soon as the window becomes activated
  // again. There is no ability to control the state through this API as this
  // leads to sync problems.
  // For Gtk use WidgetGtk::make_transient_to_parent.
  virtual void DisableInactiveRendering() = 0;
#endif

  // Activates the window, assuming it already exists and is visible.
  virtual void Activate();

  // Deactivates the window, making the next window in the Z order the active
  // window.
  virtual void Deactivate();

  // Closes the window, ultimately destroying it. This isn't immediate (it
  // occurs after a return to the message loop. Implementors must also make sure
  // that invoking Close multiple times doesn't cause bad things to happen,
  // since it can happen.
  virtual void Close();

  // Maximizes/minimizes/restores the window.
  virtual void Maximize();
  virtual void Minimize();
  virtual void Restore();

  // Whether or not the window is currently active.
  virtual bool IsActive() const;

  // Whether or not the window is currently visible.
  virtual bool IsVisible() const;

  // Whether or not the window is maximized or minimized.
  virtual bool IsMaximized() const;
  virtual bool IsMinimized() const;

  // Accessors for fullscreen state.
  virtual void SetFullscreen(bool fullscreen);
  virtual bool IsFullscreen() const;

  // Sets whether or not the window should show its frame as a "transient drag
  // frame" - slightly transparent and without the standard window controls.
  virtual void SetUseDragFrame(bool use_drag_frame);

  // Returns true if the Window is considered to be an "app window" - i.e.
  // any window which when it is the last of its type closed causes the
  // application to exit.
  virtual bool IsAppWindow() const;

  // Toggles the enable state for the Close button (and the Close menu item in
  // the system menu).
  virtual void EnableClose(bool enable);

  // Tell the window to update its title from the delegate.
  virtual void UpdateWindowTitle();

  // Tell the window to update its icon from the delegate.
  virtual void UpdateWindowIcon();

  // Sets whether or not the window is always-on-top.
  virtual void SetIsAlwaysOnTop(bool always_on_top);

  // Creates an appropriate NonClientFrameView for this window.
  virtual NonClientFrameView* CreateFrameViewForWindow();

  // Updates the frame after an event caused it to be changed.
  virtual void UpdateFrameAfterFrameChange();

  // Retrieves the Window's native window handle.
  virtual gfx::NativeWindow GetNativeWindow() const;

  // Whether we should be using a native frame.
  virtual bool ShouldUseNativeFrame() const;

  // Tell the window that something caused the frame type to change.
  virtual void FrameTypeChanged();

  WindowDelegate* window_delegate() {
    return const_cast<WindowDelegate*>(
        const_cast<const Window*>(this)->window_delegate());
  }
  const WindowDelegate* window_delegate() const {
    return window_delegate_;
  }

  NonClientView* non_client_view() {
    return const_cast<NonClientView*>(
        const_cast<const Window*>(this)->non_client_view());
  }
  const NonClientView* non_client_view() const {
    return non_client_view_;
  }

  ClientView* client_view() {
    return const_cast<ClientView*>(
        const_cast<const Window*>(this)->client_view());
  }
  const ClientView* client_view() const {
    return non_client_view()->client_view();
  }

 protected:
  // TODO(beng): Temporarily provided as a way to associate the subclass'
  //             implementation of NativeWidget with this.
  void set_native_window(NativeWindow* native_window) {
    native_window_ = native_window;
  }

  // Overridden from NativeWindowDelegate:
  virtual void OnWindowDestroying();
  virtual void OnWindowDestroyed();

 private:
  Window();

  NativeWindow* native_window_;

  // Our window delegate (see InitWindow() method for documentation).
  WindowDelegate* window_delegate_;

  // The View that provides the non-client area of the window (title bar,
  // window controls, sizing borders etc). To use an implementation other than
  // the default, this class must be sub-classed and this value set to the
  // desired implementation before calling |InitWindow()|.
  NonClientView* non_client_view_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace views

#endif  // #ifndef VIEWS_WINDOW_WINDOW_H_
