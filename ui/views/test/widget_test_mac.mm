// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/test/widget_test.h"

#include <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "ui/views/widget/root_view.h"

namespace views {
namespace test {

// static
void WidgetTest::SimulateNativeDestroy(Widget* widget) {
  // Retain the window while closing it, otherwise the window may lose its last
  // owner before -[NSWindow close] completes (this offends AppKit). Usually
  // this reference will exist on an event delivered to the runloop.
  base::scoped_nsobject<NSWindow> window([widget->GetNativeWindow() retain]);
  [window close];
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
