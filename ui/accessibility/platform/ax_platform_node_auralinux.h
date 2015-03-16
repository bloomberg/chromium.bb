// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_PLATFORM_NODE_AURALINUX_H_
#define UI_ACCESSIBILITY_AX_PLATFORM_NODE_AURALINUX_H_

#include <atk/atk.h>

#include "ui/accessibility/ax_export.h"
#include "ui/accessibility/platform/ax_platform_node_base.h"

namespace ui {

// Implements accessibility on Aura Linux using ATK.
class AXPlatformNodeAuraLinux : public AXPlatformNodeBase {
 public:
  AXPlatformNodeAuraLinux();

  // Set or get the root-level Application object that's the parent of all
  // top-level windows.
  AX_EXPORT static void SetApplication(AXPlatformNode* application);
  static AXPlatformNode* application() { return application_; }

  AtkRole GetAtkRole();

  // AXPlatformNode overrides.
  void Destroy() override;
  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  void NotifyAccessibilityEvent(ui::AXEvent event_type) override;

  // AXPlatformNodeBase overrides.
  void Init(AXPlatformNodeDelegate* delegate) override;
  int GetIndexInParent() override;

 private:
  ~AXPlatformNodeAuraLinux() override;

  // We own a reference to this ref-counted object.
  AtkObject* atk_object_;

  // The root-level Application object that's the parent of all
  // top-level windows.
  static AXPlatformNode* application_;

  DISALLOW_COPY_AND_ASSIGN(AXPlatformNodeAuraLinux);
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_PLATFORM_NODE_AURALINUX_H_
