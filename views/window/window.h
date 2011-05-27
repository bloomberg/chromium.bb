// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_WINDOW_H_
#define VIEWS_WINDOW_WINDOW_H_
#pragma once

#include "ui/gfx/native_widget_types.h"
#include "views/widget/widget.h"
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
class Window : public Widget,
               public internal::NativeWindowDelegate {
 public:
  struct InitParams {
    // |window_delegate| cannot be NULL.
    explicit InitParams(WindowDelegate* window_delegate);

    WindowDelegate* window_delegate;
    gfx::NativeWindow parent_window;
    NativeWindow* native_window;
    Widget::InitParams widget_init_params;
  };

  enum FrameType {
    FRAME_TYPE_DEFAULT,         // Use whatever the default would be.
    FRAME_TYPE_FORCE_CUSTOM,    // Force the custom frame.
    FRAME_TYPE_FORCE_NATIVE     // Force the native frame.
  };

  Window();
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

  // Initializes the window. Must be called before any post-configuration
  // operations are performed.
  void InitWindow(const InitParams& params);

  // Retrieves the window's bounds, including its frame.
  gfx::Rect GetBounds() const;

  // Retrieves the restored bounds for the window.
  gfx::Rect GetNormalBounds() const;

  // Sets the Window's bounds. The window is inserted after |other_window| in
  // the window Z-order. If this window is not yet visible, other_window's
  // monitor is used as the constraining rectangle, rather than this window's
  // monitor.
  void SetWindowBounds(const gfx::Rect& bounds, gfx::NativeWindow other_window);

  // Makes the window visible.
  void Show();

  // Like Show(), but does not activate the window.
  void ShowInactive();

  // Hides the window. This does not delete the window, it just hides it. This
  // always hides the window, it is separate from the stack maintained by
  // Push/PopForceHidden.
  virtual void HideWindow();

  // Prevents the window from being rendered as deactivated the next time it is.
  // This state is reset automatically as soon as the window becomes activated
  // again. There is no ability to control the state through this API as this
  // leads to sync problems.
  void DisableInactiveRendering();

  // Activates the window, assuming it already exists and is visible.
  void Activate();

  // Deactivates the window, making the next window in the Z order the active
  // window.
  void Deactivate();

  // Closes the window, ultimately destroying it. The window hides immediately,
  // and is destroyed after a return to the message loop. Close() can be called
  // multiple times.
  virtual void Close() OVERRIDE;

  // Maximizes/minimizes/restores the window.
  void Maximize();
  void Minimize();
  void Restore();

  // Whether or not the window is currently active.
  bool IsActive() const;

  // Whether or not the window is currently visible.
  bool IsVisible() const;

  // Whether or not the window is maximized or minimized.
  virtual bool IsMaximized() const;
  bool IsMinimized() const;

  // Accessors for fullscreen state.
  void SetFullscreen(bool fullscreen);
  bool IsFullscreen() const;

  // Sets whether or not the window should show its frame as a "transient drag
  // frame" - slightly transparent and without the standard window controls.
  void SetUseDragFrame(bool use_drag_frame);

  // Toggles the enable state for the Close button (and the Close menu item in
  // the system menu).
  void EnableClose(bool enable);

  // Tell the window to update its title from the delegate.
  void UpdateWindowTitle();

  // Tell the window to update its icon from the delegate.
  void UpdateWindowIcon();

  // Sets whether or not the window is always-on-top.
  void SetIsAlwaysOnTop(bool always_on_top);

  // Creates an appropriate NonClientFrameView for this window.
  virtual NonClientFrameView* CreateFrameViewForWindow();

  // Updates the frame after an event caused it to be changed.
  virtual void UpdateFrameAfterFrameChange();

  // Retrieves the Window's native window handle.
  gfx::NativeWindow GetNativeWindow() const;

  void set_frame_type(FrameType frame_type) { frame_type_ = frame_type; }
  FrameType frame_type() const { return frame_type_; }

  // Whether we should be using a native frame.
  bool ShouldUseNativeFrame() const;

  // Forces the frame into the alternate frame type (custom or native) depending
  // on its current state.
  void DebugToggleFrameType();

  // Tell the window that something caused the frame type to change.
  void FrameTypeChanged();

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

  NativeWindow* native_window() { return native_window_; }

 protected:
  // Overridden from NativeWindowDelegate:
  virtual bool CanActivate() const OVERRIDE;
  virtual bool IsInactiveRenderingDisabled() const OVERRIDE;
  virtual void EnableInactiveRendering() OVERRIDE;
  virtual bool IsModal() const OVERRIDE;
  virtual bool IsDialogBox() const OVERRIDE;
  virtual gfx::Size GetMinimumSize() const OVERRIDE;
  virtual int GetNonClientComponent(const gfx::Point& point) const OVERRIDE;
  virtual bool ExecuteCommand(int command_id) OVERRIDE;
  virtual void OnNativeWindowCreated(const gfx::Rect& bounds) OVERRIDE;
  virtual void OnNativeWindowActivationChanged(bool active) OVERRIDE;
  virtual void OnNativeWindowBeginUserBoundsChange() OVERRIDE;
  virtual void OnNativeWindowEndUserBoundsChange() OVERRIDE;
  virtual void OnNativeWindowDestroying() OVERRIDE;
  virtual void OnNativeWindowDestroyed() OVERRIDE;
  virtual void OnNativeWindowBoundsChanged() OVERRIDE;
  virtual Window* AsWindow() OVERRIDE;
  virtual internal::NativeWidgetDelegate* AsNativeWidgetDelegate() OVERRIDE;

 private:
  // Sizes and positions the window just after it is created.
  void SetInitialBounds(const gfx::Rect& bounds);

  // Persists the window's restored position and maximized state using the
  // window delegate.
  void SaveWindowPosition();

  NativeWindow* native_window_;

  // Our window delegate (see InitWindow() method for documentation).
  WindowDelegate* window_delegate_;

  // The View that provides the non-client area of the window (title bar,
  // window controls, sizing borders etc). To use an implementation other than
  // the default, this class must be sub-classed and this value set to the
  // desired implementation before calling |InitWindow()|.
  NonClientView* non_client_view_;

  // The saved maximized state for this window. See note in SetInitialBounds
  // that explains why we save this.
  bool saved_maximized_state_;

  // The smallest size the window can be.
  gfx::Size minimum_size_;

  // True when the window should be rendered as active, regardless of whether
  // or not it actually is.
  bool disable_inactive_rendering_;

  // Set to true if the window is in the process of closing .
  bool window_closed_;

  // The current frame type in use by this window. Defaults to
  // FRAME_TYPE_DEFAULT.
  FrameType frame_type_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace views

#endif  // #ifndef VIEWS_WINDOW_WINDOW_H_
