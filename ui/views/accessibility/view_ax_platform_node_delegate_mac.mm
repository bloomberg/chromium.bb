// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/accessibility/view_ax_platform_node_delegate_mac.h"

#include <memory>

#include "ui/views/cocoa/bridged_native_widget_host_impl.h"
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

gfx::NativeViewAccessible ViewAXPlatformNodeDelegateMac::GetNSWindow() {
  auto* widget = view()->GetWidget();
  if (!widget)
    return nil;

  auto* top_level_widget = widget->GetTopLevelWidget();
  if (!top_level_widget)
    return nil;

  auto* bridge_host = BridgedNativeWidgetHostImpl::GetFromNativeWindow(
      top_level_widget->GetNativeWindow());
  if (!bridge_host)
    return nil;

  return bridge_host->GetNativeViewAccessibleForNSWindow();
}

gfx::NativeViewAccessible ViewAXPlatformNodeDelegateMac::GetParent() {
  if (view()->parent())
    return view()->parent()->GetNativeViewAccessible();

  auto* widget = view()->GetWidget();
  if (!widget)
    return nil;

  auto* bridge_host = BridgedNativeWidgetHostImpl::GetFromNativeWindow(
      view()->GetWidget()->GetNativeWindow());
  if (!bridge_host)
    return nil;

  return bridge_host->GetNativeViewAccessibleForNSView();
}

}  // namespace views
