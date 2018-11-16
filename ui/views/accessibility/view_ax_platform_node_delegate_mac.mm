// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_ax_platform_node_delegate_mac.h"

#include <memory>

#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace views {

// static
std::unique_ptr<ViewAccessibility> ViewAccessibility::Create(View* view) {
  return std::make_unique<ViewAXPlatformNodeDelegateMac>(view);
}

ViewAXPlatformNodeDelegateMac::ViewAXPlatformNodeDelegateMac(View* view)
    : ViewAXPlatformNodeDelegate(view) {}

ViewAXPlatformNodeDelegateMac::~ViewAXPlatformNodeDelegateMac() = default;

gfx::NativeViewAccessible ViewAXPlatformNodeDelegateMac::GetParent() {
  if (view()->parent())
    return view()->parent()->GetNativeViewAccessible();

  // TODO(ccameron): This should return the NSAccessibilityRemoteUIElement for
  // the NSView in the viewer process. This requires that Widget have a
  // method to retrieve this object, which will not necessarily coincide with
  // GetNativeView.
  if (view()->GetWidget())
    return view()->GetWidget()->GetNativeView().GetNativeNSView();

  return nullptr;
}

}  // namespace views
