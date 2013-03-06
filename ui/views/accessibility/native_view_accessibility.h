// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_H_
#define UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_H_

#include "ui/base/accessibility/accessibility_types.h"
#include "ui/gfx/native_widget_types.h"

namespace views {

class View;

class NativeViewAccessibility {
 public:
  static NativeViewAccessibility* Create(View* view);

  virtual void NotifyAccessibilityEvent(
      ui::AccessibilityTypes::Event event_type) {}

  virtual gfx::NativeViewAccessible GetNativeObject();

  // Call Destroy rather than deleting this, because the subclass may
  // use reference counting.
  virtual void Destroy();

 protected:
  NativeViewAccessibility();
  virtual ~NativeViewAccessibility();

 private:
  DISALLOW_COPY_AND_ASSIGN(NativeViewAccessibility);
};

}  // namespace views

#endif  // UI_VIEWS_ACCESSIBILITY_NATIVE_VIEW_ACCESSIBILITY_H_
