// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/native_widget_factory.h"

#include "ui/aura/env.h"
#include "ui/views/mus/window_manager_connection.h"

namespace views {
namespace test {

NativeWidget* CreatePlatformDesktopNativeWidgetImpl(
    const Widget::InitParams& init_params,
    Widget* widget,
    bool* destroyed) {
  // On X we need to reset the ContextFactory before every NativeWidgetMus
  // is created.
  // TODO(sad): this is a hack, figure out a better solution.
  ui::ContextFactory* factory = aura::Env::GetInstance()->context_factory();
  aura::Env::GetInstance()->set_context_factory(nullptr);
  NativeWidget* result = WindowManagerConnection::Get()->CreateNativeWidgetMus(
      std::map<std::string, std::vector<uint8_t>>(), init_params, widget);
  aura::Env::GetInstance()->set_context_factory(factory);
  return result;
}

}  // namespace test
}  // namespace views
