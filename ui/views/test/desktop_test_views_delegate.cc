// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/desktop_test_views_delegate.h"

#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/native_widget_aura.h"

namespace views {

DesktopTestViewsDelegate::DesktopTestViewsDelegate() {}

DesktopTestViewsDelegate::~DesktopTestViewsDelegate() {}

void DesktopTestViewsDelegate::OnBeforeWidgetInit(
    Widget::InitParams* params,
    internal::NativeWidgetDelegate* delegate) {
#if defined(USE_AURA) && !defined(OS_CHROMEOS)
  if (params->parent && params->type != views::Widget::InitParams::TYPE_MENU) {
    params->native_widget = new views::NativeWidgetAura(delegate);
  } else if (!params->parent && !params->context) {
    params->native_widget = new views::DesktopNativeWidgetAura(delegate);
  }
#endif
}

}  // namespace views
