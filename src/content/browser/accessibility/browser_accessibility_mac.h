// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MAC_H_
#define CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MAC_H_

#include "content/browser/accessibility/browser_accessibility.h"
#include "content/common/content_export.h"
#include "ui/accessibility/ax_node.h"

@class BrowserAccessibilityCocoa;

namespace ui {

class AXPlatformNodeMac;

}  // namespace ui

namespace content {

#if __OBJC__
CONTENT_EXPORT const BrowserAccessibilityCocoa* ToBrowserAccessibilityCocoa(
    const BrowserAccessibility* obj);
CONTENT_EXPORT BrowserAccessibilityCocoa* ToBrowserAccessibilityCocoa(
    BrowserAccessibility* obj);
#endif

class BrowserAccessibilityMac : public BrowserAccessibility {
 public:
  ~BrowserAccessibilityMac() override;
  BrowserAccessibilityMac(const BrowserAccessibilityMac&) = delete;
  BrowserAccessibilityMac& operator=(const BrowserAccessibilityMac&) = delete;

  // BrowserAccessibility overrides.
  void OnDataChanged() override;
  uint32_t PlatformChildCount() const override;
  BrowserAccessibility* PlatformGetChild(uint32_t child_index) const override;

  BrowserAccessibility* PlatformGetFirstChild() const override;
  BrowserAccessibility* PlatformGetLastChild() const override;
  BrowserAccessibility* PlatformGetNextSibling() const override;
  BrowserAccessibility* PlatformGetPreviousSibling() const override;

  gfx::NativeViewAccessible GetNativeViewAccessible() override;
  ui::AXPlatformNode* GetAXPlatformNode() const override;

  // The BrowserAccessibilityCocoa associated with us.
  BrowserAccessibilityCocoa* GetNativeWrapper() const;

  // Refresh the native object associated with this.
  // Useful for re-announcing the current focus when properties have changed.
  void ReplaceNativeObject();

 protected:
  BrowserAccessibilityMac(BrowserAccessibilityManager* manager,
                          ui::AXNode* node);

  friend class BrowserAccessibility;  // Needs access to our constructor.

 private:
  // Creates platform and cocoa node if not yet created.
  void CreatePlatformNodes();

  // Creates a new cocoa node. Returns an old node in the swap_node.
  BrowserAccessibilityCocoa* CreateNativeWrapper();

  // Manager of the native cocoa node. We own this object.
  ui::AXPlatformNodeMac* platform_node_ = nullptr;
};

}  // namespace content

#endif  // CONTENT_BROWSER_ACCESSIBILITY_BROWSER_ACCESSIBILITY_MAC_H_
