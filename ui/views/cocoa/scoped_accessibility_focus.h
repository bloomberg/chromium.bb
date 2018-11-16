// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_SCOPED_ACCESSIBILITY_FOCUS_H_
#define UI_VIEWS_COCOA_SCOPED_ACCESSIBILITY_FOCUS_H_

#include "ui/views/views_export.h"

namespace views_bridge_mac {
class BridgedNativeWidgetHostHelper;
}  // namespace views_bridge_mac

namespace views {

// This object, while instantiated, will swizzle -[NSApplication
// accessibilityFocusedUIElement] to return the GetNativeViewAccessible of the
// specified BridgedNativeWidgetHostHelper. This is needed to handle remote
// accessibility queries. If two of these objects are instantiated at the same
// time, the most recently created object will take precedence.
class VIEWS_EXPORT ScopedAccessibilityFocus {
 public:
  ScopedAccessibilityFocus(
      views_bridge_mac::BridgedNativeWidgetHostHelper* host_helper);
  ~ScopedAccessibilityFocus();

 private:
  views_bridge_mac::BridgedNativeWidgetHostHelper* const host_helper_;
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_SCOPED_ACCESSIBILITY_FOCUS_H_
