// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/widget_test.h"

#include <Cocoa/Cocoa.h>

#include "ui/views/widget/root_view.h"

namespace views {
namespace test {

// static
void WidgetTest::SimulateNativeDestroy(Widget* widget) {
  DCHECK([widget->GetNativeWindow() isReleasedWhenClosed]);
  [widget->GetNativeWindow() close];
}

// static
bool WidgetTest::IsNativeWindowVisible(gfx::NativeWindow window) {
  return [window isVisible];
}

// static
ui::EventProcessor* WidgetTest::GetEventProcessor(Widget* widget) {
  return static_cast<internal::RootView*>(widget->GetRootView());
}

}  // namespace test
}  // namespace views
