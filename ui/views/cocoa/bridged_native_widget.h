// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_H_
#define UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "ui/views/views_export.h"

@class BridgedContentView;

namespace views {

class View;

// A bridge to an NSWindow managed by an instance of NativeWidgetMac or
// DesktopNativeWidgetMac. Serves as a helper class to bridge requests from the
// NativeWidgetMac to the Cocoa window. Behaves a bit like an aura::Window.
class VIEWS_EXPORT BridgedNativeWidget {
 public:
  BridgedNativeWidget();
  ~BridgedNativeWidget();

  // Initialize the bridge, "retains" ownership of |window|.
  void Init(base::scoped_nsobject<NSWindow> window);

  // Set or clears the views::View bridged by the content view. This does NOT
  // take ownership of |view|.
  void SetRootView(views::View* view);

  BridgedContentView* ns_view() { return bridged_view_; }
  NSWindow* ns_window() { return window_; }

 private:
  base::scoped_nsobject<NSWindow> window_;
  base::scoped_nsobject<BridgedContentView> bridged_view_;

  DISALLOW_COPY_AND_ASSIGN(BridgedNativeWidget);
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_H_
