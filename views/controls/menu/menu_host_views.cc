// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/menu_host_views.h"

#include "views/controls/menu/native_menu_host_delegate.h"

namespace views {

MenuHostViews::MenuHostViews(internal::NativeMenuHostDelegate* delegate)
    : NativeWidgetViews(delegate->AsNativeWidgetDelegate()),
      delegate_(delegate) {
}

MenuHostViews::~MenuHostViews() {
}

void MenuHostViews::StartCapturing() {
  SetMouseCapture();
}

NativeWidget* MenuHostViews::AsNativeWidget() {
  return this;
}

void MenuHostViews::InitNativeWidget(const Widget::InitParams& params) {
  NativeWidgetViews::InitNativeWidget(params);
  GetView()->SetVisible(false);
}

}  // namespace views

