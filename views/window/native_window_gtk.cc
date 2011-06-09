// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/window/native_window_gtk.h"

#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "ui/gfx/path.h"
#include "ui/gfx/rect.h"
#include "views/events/event.h"
#include "views/window/hit_test.h"
#include "views/window/native_window_delegate.h"
#include "views/window/non_client_view.h"
#include "views/window/window_delegate.h"

namespace views {

NativeWindowGtk::NativeWindowGtk(internal::NativeWindowDelegate* delegate)
    : NativeWidgetGtk(delegate->AsNativeWidgetDelegate()),
      delegate_(delegate) {
  is_window_ = true;
}

NativeWindowGtk::~NativeWindowGtk() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeWindowGtk, NativeWindow implementation:

NativeWidget* NativeWindowGtk::AsNativeWidget() {
  return this;
}

const NativeWidget* NativeWindowGtk::AsNativeWidget() const {
  return this;
}

Window* NativeWindowGtk::GetWindow() {
  return delegate_->AsWindow();
}

const Window* NativeWindowGtk::GetWindow() const {
  return delegate_->AsWindow();
}

////////////////////////////////////////////////////////////////////////////////
// NativeWindowGtk, private:

////////////////////////////////////////////////////////////////////////////////
// NativeWindow, public:

// static
NativeWindow* NativeWindow::CreateNativeWindow(
    internal::NativeWindowDelegate* delegate) {
  return new NativeWindowGtk(delegate);
}

}  // namespace views
