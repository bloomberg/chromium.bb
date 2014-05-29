// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_views_delegate.h"

#include "ui/views/widget/native_widget_mac.h"

namespace views {

TestViewsDelegate::TestViewsDelegate()
    : use_desktop_native_widgets_(false),
      use_transparent_windows_(false) {
  DCHECK(!ViewsDelegate::views_delegate);
  ViewsDelegate::views_delegate = this;
}

TestViewsDelegate::~TestViewsDelegate() {
  if (ViewsDelegate::views_delegate == this)
    ViewsDelegate::views_delegate = NULL;
}

void TestViewsDelegate::OnBeforeWidgetInit(
    Widget::InitParams* params,
    internal::NativeWidgetDelegate* delegate) {
  if (params->opacity == Widget::InitParams::INFER_OPACITY) {
    params->opacity = use_transparent_windows_ ?
        Widget::InitParams::TRANSLUCENT_WINDOW :
        Widget::InitParams::OPAQUE_WINDOW;
  }
  // TODO(tapted): This should return a *Desktop*NativeWidgetMac.
  if (!params->native_widget && use_desktop_native_widgets_)
    params->native_widget = new NativeWidgetMac(delegate);
}

}  // namespace views
