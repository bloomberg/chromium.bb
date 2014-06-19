// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/widget_test.h"

#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/views/widget/widget.h"

namespace views {
namespace test {

// static
void WidgetTest::SimulateNativeDestroy(Widget* widget) {
  delete widget->GetNativeView();
}

// static
bool WidgetTest::IsNativeWindowVisible(gfx::NativeWindow window) {
  return window->IsVisible();
}

// static
ui::EventProcessor* WidgetTest::GetEventProcessor(Widget* widget) {
  return widget->GetNativeWindow()->GetHost()->event_processor();
}

}  // namespace test
}  // namespace views
