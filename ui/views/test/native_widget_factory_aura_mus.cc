// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/native_widget_factory.h"

#include "ui/views/mus/mus_client.h"

namespace views {
namespace test {

NativeWidget* CreatePlatformDesktopNativeWidgetImpl(
    const Widget::InitParams& init_params,
    Widget* widget,
    bool* destroyed) {
  return MusClient::Get()->CreateNativeWidget(init_params, widget);
}

}  // namespace test
}  // namespace views
