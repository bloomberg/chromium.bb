// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node.h"

#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {

#if !defined(OS_MACOSX) && !defined(OS_WIN) && !(defined(OS_LINUX) && !defined(OS_CHROMEOS))
// static
AXPlatformNode* AXPlatformNode::Create(AXPlatformNodeDelegate* delegate) {
  return nullptr;
}
#endif

#if !defined(OS_WIN)
// This is the default implementation for platforms where native views
// accessibility is unsupported or unfinished.
//
// static
AXPlatformNode* AXPlatformNode::FromNativeViewAccessible(
    gfx::NativeViewAccessible accessible) {
  return nullptr;
}
#endif

AXPlatformNode::AXPlatformNode() {
}

AXPlatformNode::~AXPlatformNode() {
}

}  // namespace ui
