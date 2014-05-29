// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/test_views_delegate.h"

#include "ui/wm/core/wm_state.h"

#if !defined(OS_CHROMEOS)
#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#endif  // !defined(OS_CHROMEOS)


namespace views {

TestViewsDelegate::TestViewsDelegate()
    : use_desktop_native_widgets_(false),
      use_transparent_windows_(false) {
  DCHECK(!ViewsDelegate::views_delegate);
  ViewsDelegate::views_delegate = this;
#if defined(USE_AURA)
  wm_state_.reset(new wm::WMState);
#endif
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
#if !defined(OS_CHROMEOS)
  if (!params->native_widget && use_desktop_native_widgets_)
    params->native_widget = new DesktopNativeWidgetAura(delegate);
#endif  // !defined(OS_CHROMEOS)
}

}  // namespace views
