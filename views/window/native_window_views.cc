// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/window/native_window_views.h"

#include "views/view.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeWindowViews, public:

NativeWindowViews::NativeWindowViews(View* host,
                                     internal::NativeWindowDelegate* delegate)
    : NativeWidgetViews(host, delegate->AsNativeWidgetDelegate()),
      delegate_(delegate) {
}

NativeWindowViews::~NativeWindowViews() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeWindowViews, NativeWindow implementation:

Window* NativeWindowViews::GetWindow() {
  return delegate_->AsWindow();
}

const Window* NativeWindowViews::GetWindow() const {
  return delegate_->AsWindow();
}

NativeWidget* NativeWindowViews::AsNativeWidget() {
  return this;
}

const NativeWidget* NativeWindowViews::AsNativeWidget() const {
  return this;
}

void NativeWindowViews::BecomeModal() {
  NOTIMPLEMENTED();
}

}  // namespace views
