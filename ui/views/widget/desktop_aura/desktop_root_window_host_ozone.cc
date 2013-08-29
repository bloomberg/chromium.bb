// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/root_window_host.h"
#include "ui/views/widget/desktop_aura/desktop_root_window_host.h"
#include "ui/views/widget/desktop_aura/desktop_factory_ozone.h"

namespace views {

DesktopRootWindowHost* DesktopRootWindowHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    DesktopNativeWidgetAura* desktop_native_widget_aura,
    const gfx::Rect& initial_bounds) {
  DesktopFactoryOzone* d_factory = DesktopFactoryOzone::GetInstance();

  return d_factory->CreateRootWindowHost(native_widget_delegate,
                                         desktop_native_widget_aura,
                                         initial_bounds);
}

}  // namespace views
