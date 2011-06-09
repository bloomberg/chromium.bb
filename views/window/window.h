// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_WINDOW_WINDOW_H_
#define VIEWS_WINDOW_WINDOW_H_
#pragma once

#include "ui/gfx/native_widget_types.h"
#include "views/widget/widget.h"
#include "views/window/native_window_delegate.h"

namespace gfx {
class Rect;
class Size;
}  // namespace gfx

namespace views {

class NativeWindow;
class Widget;
class WindowDelegate;

////////////////////////////////////////////////////////////////////////////////
// Window class
//
// Encapsulates window-like behavior. See WindowDelegate.
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

  Window();
  virtual ~Window();

  // Creates an instance of an object implementing this interface.
  // TODO(beng): create a version of this function that takes a NativeView, for
  //             constrained windows.
  static Window* CreateChromeWindow(gfx::NativeWindow parent,
                                    const gfx::Rect& bounds,
                                    WindowDelegate* window_delegate);

  // Initializes the window. Must be called before any post-configuration
  // operations are performed.
  void InitWindow(const InitParams& params);

  // Overridden from Widget:
  virtual Window* AsWindow() OVERRIDE;
  virtual const Window* AsWindow() const OVERRIDE;

  WindowDelegate* window_delegate() {
    return const_cast<WindowDelegate*>(
        const_cast<const Window*>(this)->window_delegate());
  }
  const WindowDelegate* window_delegate() const {
    return reinterpret_cast<WindowDelegate*>(widget_delegate());
  }

  NativeWindow* native_window() { return native_window_; }

 protected:
  // Overridden from NativeWindowDelegate:
  virtual internal::NativeWidgetDelegate* AsNativeWidgetDelegate() OVERRIDE;

 private:
  NativeWindow* native_window_;

  DISALLOW_COPY_AND_ASSIGN(Window);
};

}  // namespace views

#endif  // #ifndef VIEWS_WINDOW_WINDOW_H_
