// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_BASE_H_
#define UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_BASE_H_

#include "ui/accessibility/ax_enums.h"
#include "ui/accessibility/platform/ax_platform_node.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"

namespace ui {

class AXPlatformNodeDelegate;

class AXPlatformNodeBase : public AXPlatformNode {
 public:
   virtual void Init(AXPlatformNodeDelegate* delegate);

  // These are simple wrappers to our delegate.
  AXRole GetRole() const;
  gfx::Rect GetBoundsInScreen() const;
  gfx::NativeViewAccessible GetParent();
  int GetChildCount();
  gfx::NativeViewAccessible ChildAtIndex(int index);

  // AXPlatformNode
  virtual void Destroy() OVERRIDE;
  virtual gfx::NativeViewAccessible GetNativeViewAccessible() OVERRIDE;

 protected:
  AXPlatformNodeBase();
  virtual ~AXPlatformNodeBase();

  AXPlatformNodeDelegate* delegate_;  // Weak. Owns this.

 private:
  DISALLOW_COPY_AND_ASSIGN(AXPlatformNodeBase);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_PLATFORM_AX_PLATFORM_NODE_BASE_H_
