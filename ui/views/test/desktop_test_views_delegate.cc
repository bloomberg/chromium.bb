// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/desktop_test_views_delegate.h"

#include "ui/views/widget/desktop_aura/desktop_native_widget_aura.h"
#include "ui/views/widget/native_widget_aura.h"

namespace views {

DesktopTestViewsDelegate::DesktopTestViewsDelegate() {}

DesktopTestViewsDelegate::~DesktopTestViewsDelegate() {}

NativeWidget* DesktopTestViewsDelegate::CreateNativeWidget(
    Widget::InitParams::Type type,
    internal::NativeWidgetDelegate* delegate,
    gfx::NativeView parent,
    gfx::NativeView context) {
#if defined(USE_AURA) && !defined(OS_CHROMEOS)
  if (parent && type != views::Widget::InitParams::TYPE_MENU)
    return new views::NativeWidgetAura(delegate);

  if (!parent && !context)
    return new views::DesktopNativeWidgetAura(delegate);
#endif

  return NULL;
}

}  // namespace views
