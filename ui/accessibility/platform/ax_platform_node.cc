// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/platform/ax_platform_node.h"

#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/platform/ax_platform_node_delegate.h"

namespace ui {

#if !defined(OS_MACOSX)
// static
AXPlatformNode* AXPlatformNode::Create(AXPlatformNodeDelegate* delegate) {
  return NULL;
}
#endif

AXPlatformNode::AXPlatformNode() {
}

AXPlatformNode::~AXPlatformNode() {
}

}  // namespace ui
