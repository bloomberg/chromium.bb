// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_host_win.h"

#include "views/controls/menu/menu_host_views.h"
#include "views/controls/menu/native_menu_host_delegate.h"
#include "views/views_delegate.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// MenuHostWin, public:

MenuHostWin::MenuHostWin(internal::NativeMenuHostDelegate* delegate)
    : NativeWidgetWin(delegate->AsNativeWidgetDelegate()),
      delegate_(delegate) {
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
// MenuHostWin, NativeWidgetWin overrides:

void MenuHostWin::OnDestroy() {
  delegate_->OnNativeMenuHostDestroy();
  NativeWidgetWin::OnDestroy();
}

void MenuHostWin::OnCancelMode() {
  delegate_->OnNativeMenuHostCancelCapture();
  NativeWidgetWin::OnCancelMode();
}

////////////////////////////////////////////////////////////////////////////////
// NativeMenuHost, public:

// static
NativeMenuHost* NativeMenuHost::CreateNativeMenuHost(
    internal::NativeMenuHostDelegate* delegate) {
  if (Widget::IsPureViews() &&
      ViewsDelegate::views_delegate->GetDefaultParentView()) {
    return new MenuHostViews(delegate);
  }
  return new MenuHostWin(delegate);
}

}  // namespace views
