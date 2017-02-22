// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/native_view_accessibility_mac.h"

#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

// static
NativeViewAccessibility* NativeViewAccessibility::Create(View* view) {
  return new NativeViewAccessibilityMac(view);
}

NativeViewAccessibilityMac::NativeViewAccessibilityMac(View* view)
    : NativeViewAccessibility(view) {}

gfx::NativeViewAccessible NativeViewAccessibilityMac::GetParent() {
  if (view_->parent())
    return view_->parent()->GetNativeViewAccessible();

  if (view_->GetWidget())
    return view_->GetWidget()->GetNativeView();

  if (parent_widget_)
    return parent_widget_->GetRootView()->GetNativeViewAccessible();

  return nullptr;
}

}  // namespace views
