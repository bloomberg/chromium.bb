// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"

#include "ui/base/win/atl_module.h"

namespace content {

// static
BrowserAccessibility* BrowserAccessibility::Create() {
  return new BrowserAccessibilityWin();
}

BrowserAccessibilityWin::BrowserAccessibilityWin() {
  ui::win::CreateATLModuleIfNeeded();
  HRESULT hr = CComObject<BrowserAccessibilityComWin>::CreateInstance(
      &browser_accessibility_com_);
  DCHECK(SUCCEEDED(hr));

  browser_accessibility_com_->AddRef();

  // Set the delegate to us
  browser_accessibility_com_->Init(this);
}

BrowserAccessibilityWin::~BrowserAccessibilityWin() {
  if (browser_accessibility_com_) {
    browser_accessibility_com_->Destroy();
    browser_accessibility_com_ = nullptr;
  }
}

void BrowserAccessibilityWin::UpdatePlatformAttributes() {
  GetCOM()->UpdateStep1ComputeWinAttributes();
  GetCOM()->UpdateStep2ComputeHypertext();
  GetCOM()->UpdateStep3FireEvents();
}

bool BrowserAccessibilityWin::PlatformIsLeafIncludingIgnored() const {
  // On Windows, we want to hide the subtree of a collapsed <select> element.
  // Otherwise, ATs are always going to announce its options whether it's
  // collapsed or expanded. In the AXTree, this element corresponds to a node
  // with role ax::mojom::Role::kPopUpButton parent of a node with role
  // ax::mojom::Role::kMenuListPopup.
  if (IsCollapsedMenuListPopUpButton())
    return true;

  return BrowserAccessibility::PlatformIsLeafIncludingIgnored();
}

bool BrowserAccessibilityWin::CanFireEvents() const {
  // On Windows, we want to hide the subtree of a collapsed <select> element but
  // we still need to fire events on those hidden nodes.
  if (!IsIgnored() && GetCollapsedMenuListPopUpButtonAncestor())
    return true;

  return BrowserAccessibility::CanFireEvents();
}

ui::AXPlatformNode* BrowserAccessibilityWin::GetAXPlatformNode() const {
  return GetCOM();
}

void BrowserAccessibilityWin::OnLocationChanged() {
  GetCOM()->FireNativeEvent(EVENT_OBJECT_LOCATIONCHANGE);
}

base::string16 BrowserAccessibilityWin::GetText() const {
  return GetHypertext();
}

base::string16 BrowserAccessibilityWin::GetHypertext() const {
  return GetCOM()->AXPlatformNodeWin::GetHypertext();
}

const std::vector<gfx::NativeViewAccessible>
BrowserAccessibilityWin::GetUIADescendants() const {
  std::vector<gfx::NativeViewAccessible> descendants;
  if (!IsIgnored() && !ShouldHideChildrenForUIA() && PlatformChildCount() > 0) {
    BrowserAccessibility* next_sibling_node = PlatformGetNextSibling();
    BrowserAccessibility* next_descendant_node =
        BrowserAccessibilityManager::NextInTreeOrder(this);

    while (next_descendant_node && next_descendant_node != next_sibling_node) {
      // Don't add an ignored node to the returned descendants.
      if (!next_descendant_node->IsIgnored()) {
        descendants.emplace_back(
            next_descendant_node->GetNativeViewAccessible());

        if (!ToBrowserAccessibilityWin(next_descendant_node)
                 ->ShouldHideChildrenForUIA()) {
          next_descendant_node = BrowserAccessibilityManager::NextInTreeOrder(
              next_descendant_node);
          continue;
        }
      }
      // When a node is ignored or hides its children, don't return any of its
      // descendants.
      next_descendant_node =
          BrowserAccessibilityManager::NextNonDescendantInTreeOrder(
              next_descendant_node);
    }
  }
  return descendants;
}

gfx::NativeViewAccessible BrowserAccessibilityWin::GetNativeViewAccessible() {
  return GetCOM();
}

BrowserAccessibilityComWin* BrowserAccessibilityWin::GetCOM() const {
  DCHECK(browser_accessibility_com_);
  return browser_accessibility_com_;
}

BrowserAccessibilityWin* ToBrowserAccessibilityWin(BrowserAccessibility* obj) {
  return static_cast<BrowserAccessibilityWin*>(obj);
}

const BrowserAccessibilityWin* ToBrowserAccessibilityWin(
    const BrowserAccessibility* obj) {
  return static_cast<const BrowserAccessibilityWin*>(obj);
}

ui::TextAttributeList BrowserAccessibilityWin::ComputeTextAttributes() const {
  return GetCOM()->AXPlatformNodeWin::ComputeTextAttributes();
}

bool BrowserAccessibilityWin::ShouldHideChildrenForUIA() const {
  return GetCOM()->AXPlatformNodeWin::ShouldHideChildrenForUIA();
}

}  // namespace content
