// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_host_win.h"

#include "views/controls/menu/native_menu_host_delegate.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// MenuHostWin, public:

MenuHostWin::MenuHostWin(internal::NativeMenuHostDelegate* delegate)
    : delegate_(delegate) {
}

MenuHostWin::~MenuHostWin() {
}

////////////////////////////////////////////////////////////////////////////////
// MenuHostWin, NativeMenuHost implementation:

void MenuHostWin::StartCapturing() {
  SetMouseCapture();
}

NativeWidget* MenuHostWin::AsNativeWidget() {
  return this;
}

////////////////////////////////////////////////////////////////////////////////
// MenuHostWin, WidgetWin overrides:

void MenuHostWin::OnDestroy() {
  delegate_->OnNativeMenuHostDestroy();
  WidgetWin::OnDestroy();
}

void MenuHostWin::OnCancelMode() {
  delegate_->OnNativeMenuHostCancelCapture();
  WidgetWin::OnCancelMode();
}

// TODO(beng): remove once MenuHost is-a Widget
RootView* MenuHostWin::CreateRootView() {
  return delegate_->CreateRootView();
}

// TODO(beng): remove once MenuHost is-a Widget
bool MenuHostWin::ShouldReleaseCaptureOnMouseReleased() const {
  return delegate_->ShouldReleaseCaptureOnMouseRelease();
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuHost, public:

// static
NativeMenuHost* NativeMenuHost::CreateNativeMenuHost(
    internal::NativeMenuHostDelegate* delegate) {
  return new MenuHostWin(delegate);
}

}  // namespace views
