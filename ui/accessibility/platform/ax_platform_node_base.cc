// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node_base.h"

#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {

AXPlatformNodeBase::AXPlatformNodeBase() {
}

AXPlatformNodeBase::~AXPlatformNodeBase() {
}

void AXPlatformNodeBase::Init(AXPlatformNodeDelegate* delegate) {
  delegate_ = delegate;
}

AXRole AXPlatformNodeBase::GetRole() const {
  return delegate_ ? delegate_->GetData()->role : AX_ROLE_UNKNOWN;
}

gfx::Rect AXPlatformNodeBase::GetBoundsInScreen() const {
  if (!delegate_)
    return gfx::Rect();
  gfx::Rect bounds = delegate_->GetData()->location;
  bounds.Offset(delegate_->GetGlobalCoordinateOffset());
  return bounds;
}

gfx::NativeViewAccessible AXPlatformNodeBase::GetParent() {
  return delegate_ ? delegate_->GetParent() : NULL;
}

int AXPlatformNodeBase::GetChildCount() {
  return delegate_ ? delegate_->GetChildCount() : 0;
}

gfx::NativeViewAccessible AXPlatformNodeBase::ChildAtIndex(int index) {
  return delegate_ ? delegate_->ChildAtIndex(index) : NULL;
}

// AXPlatformNode

void AXPlatformNodeBase::Destroy() {
  delegate_ = NULL;
  delete this;
}

gfx::NativeViewAccessible AXPlatformNodeBase::GetNativeViewAccessible() {
  return NULL;
}


}  // namespace ui
