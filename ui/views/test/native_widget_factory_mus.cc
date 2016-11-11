// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/native_widget_factory.h"

#include "ui/views/mus/window_manager_connection.h"
#include "ui/views/test/test_platform_native_widget.h"
#include "ui/views/widget/native_widget_aura.h"

namespace views {
namespace test {

NativeWidget* CreatePlatformNativeWidgetImpl(
    const Widget::InitParams& init_params,
    Widget* widget,
    uint32_t type,
    bool* destroyed) {
  return new TestPlatformNativeWidget<NativeWidgetAura>(
      widget, type == kStubCapture, destroyed);
}

NativeWidget* CreatePlatformDesktopNativeWidgetImpl(
    const Widget::InitParams& init_params,
    Widget* widget,
    bool* destroyed) {
  return WindowManagerConnection::Get()->CreateNativeWidgetMus(
      std::map<std::string, std::vector<uint8_t>>(), init_params, widget);
}

}  // namespace test
}  // namespace views
