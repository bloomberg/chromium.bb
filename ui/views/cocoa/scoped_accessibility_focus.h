// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_SCOPED_ACCESSIBILITY_FOCUS_H_
#define UI_VIEWS_COCOA_SCOPED_ACCESSIBILITY_FOCUS_H_

#include "ui/views/views_export.h"

namespace views {

class BridgedNativeWidgetHostImpl;

// This object, while instantiated, will swizzle -[NSApplication
// accessibilityFocusedUIElement] to return the NSWindow of the specified
// BridgedNativeWidgetHostHelper. This is needed to handle remote accessibility
// queries. If two of these objects are instantiated at the same time, the most
// recently created object will take precedence.
class VIEWS_EXPORT ScopedAccessibilityFocus {
 public:
  ScopedAccessibilityFocus(BridgedNativeWidgetHostImpl* host);
  ~ScopedAccessibilityFocus();

 private:
  BridgedNativeWidgetHostImpl* const host_;
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_SCOPED_ACCESSIBILITY_FOCUS_H_
