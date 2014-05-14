// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_views_delegate.h"

#include "ui/wm/core/wm_state.h"


namespace views {

TestViewsDelegate::TestViewsDelegate()
    : use_transparent_windows_(false) {
  DCHECK(!ViewsDelegate::views_delegate);
  ViewsDelegate::views_delegate = this;
  wm_state_.reset(new wm::WMState);
}

TestViewsDelegate::~TestViewsDelegate() {
  ViewsDelegate::views_delegate = NULL;
}

void TestViewsDelegate::SetUseTransparentWindows(bool transparent) {
  use_transparent_windows_ = transparent;
}

void TestViewsDelegate::OnBeforeWidgetInit(
    Widget::InitParams* params,
    internal::NativeWidgetDelegate* delegate) {
  if (params->opacity == Widget::InitParams::INFER_OPACITY) {
    params->opacity = use_transparent_windows_ ?
        Widget::InitParams::TRANSLUCENT_WINDOW :
        Widget::InitParams::OPAQUE_WINDOW;
  }
}

}  // namespace views
