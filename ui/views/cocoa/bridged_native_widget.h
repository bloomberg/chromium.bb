// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_H_
#define UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_H_

#import <Cocoa/Cocoa.h>

#import "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/ime/input_method_delegate.h"
#include "ui/views/views_export.h"

@class BridgedContentView;
@class ViewsNSWindowDelegate;

namespace ui {
class InputMethod;
}

namespace views {

class InputMethod;
class NativeWidgetMac;
class View;

// A bridge to an NSWindow managed by an instance of NativeWidgetMac or
// DesktopNativeWidgetMac. Serves as a helper class to bridge requests from the
// NativeWidgetMac to the Cocoa window. Behaves a bit like an aura::Window.
class VIEWS_EXPORT BridgedNativeWidget : public internal::InputMethodDelegate {
 public:
  // Creates one side of the bridge. |parent| can be NULL in tests.
  explicit BridgedNativeWidget(NativeWidgetMac* parent);
  virtual ~BridgedNativeWidget();

  // Initialize the bridge, "retains" ownership of |window|.
  void Init(base::scoped_nsobject<NSWindow> window);

  // Set or clears the views::View bridged by the content view. This does NOT
  // take ownership of |view|.
  void SetRootView(views::View* view);

  // See widget.h for documentation.
  InputMethod* CreateInputMethod();
  ui::InputMethod* GetHostInputMethod();

  NativeWidgetMac* native_widget_mac() { return native_widget_mac_; }
  BridgedContentView* ns_view() { return bridged_view_; }
  NSWindow* ns_window() { return window_; }

  // Overridden from internal::InputMethodDelegate:
  virtual void DispatchKeyEventPostIME(const ui::KeyEvent& key) OVERRIDE;

 private:
  views::NativeWidgetMac* native_widget_mac_;  // Weak. Owns this.
  base::scoped_nsobject<NSWindow> window_;
  base::scoped_nsobject<ViewsNSWindowDelegate> window_delegate_;
  base::scoped_nsobject<BridgedContentView> bridged_view_;
  scoped_ptr<ui::InputMethod> input_method_;

  DISALLOW_COPY_AND_ASSIGN(BridgedNativeWidget);
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_BRIDGED_NATIVE_WIDGET_H_
