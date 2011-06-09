// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/window/window.h"

#include "base/string_util.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/font.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "views/widget/widget.h"
#include "views/widget/native_widget.h"
#include "views/window/native_window.h"
#include "views/window/window_delegate.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// Window, public:

Window::InitParams::InitParams(WindowDelegate* window_delegate)
    : window_delegate(window_delegate),
      parent_window(NULL),
      native_window(NULL),
      widget_init_params(Widget::InitParams::TYPE_WINDOW) {
}

Window::Window()
    : native_window_(NULL) {
}

Window::~Window() {
}

// static
Window* Window::CreateChromeWindow(gfx::NativeWindow parent,
                                   const gfx::Rect& bounds,
                                   WindowDelegate* window_delegate) {
  Window* window = new Window;
  Window::InitParams params(window_delegate);
  params.parent_window = parent;
#if defined(OS_WIN)
  params.widget_init_params.parent = parent;
#endif
  params.widget_init_params.bounds = bounds;
  window->InitWindow(params);
  return window;
}

void Window::InitWindow(const InitParams& params) {
  native_window_ =
      params.native_window ? params.native_window
                           : NativeWindow::CreateNativeWindow(this);
  InitParams modified_params = params;
  modified_params.widget_init_params.delegate = params.window_delegate;
  DCHECK(!modified_params.widget_init_params.delegate->window_);
  modified_params.widget_init_params.delegate->window_ = this;
  modified_params.widget_init_params.native_widget =
      native_window_->AsNativeWidget();
  Init(modified_params.widget_init_params);
}

////////////////////////////////////////////////////////////////////////////////
// Window, Widget overrides:

Window* Window::AsWindow() {
  return this;
}

const Window* Window::AsWindow() const {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// Window, internal::NativeWindowDelegate implementation:

internal::NativeWidgetDelegate* Window::AsNativeWidgetDelegate() {
  return this;
}

}  // namespace views
