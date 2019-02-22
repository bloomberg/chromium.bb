// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_TOOLTOP_MANAGER_MAC_H_
#define UI_VIEWS_COCOA_TOOLTOP_MANAGER_MAC_H_

#include "base/macros.h"
#include "ui/views/widget/tooltip_manager.h"

namespace views_bridge_mac {
namespace mojom {
class BridgedNativeWidget;
}  // namespace mojom
}  // namespace views_bridge_mac

namespace views {

// Manages native Cocoa tooltips for the given BridgedNativeWidgetHostImpl.
class TooltipManagerMac : public TooltipManager {
 public:
  explicit TooltipManagerMac(
      views_bridge_mac::mojom::BridgedNativeWidget* bridge);
  ~TooltipManagerMac() override;

  // TooltipManager:
  int GetMaxWidth(const gfx::Point& location) const override;
  const gfx::FontList& GetFontList() const override;
  void UpdateTooltip() override;
  void TooltipTextChanged(View* view) override;

 private:
  views_bridge_mac::mojom::BridgedNativeWidget*
      bridge_;  // Weak. Owned by the owner of this.

  DISALLOW_COPY_AND_ASSIGN(TooltipManagerMac);
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_TOOLTOP_MANAGER_MAC_H_
