// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COCOA_TOOLTIP_MANAGER_MAC_H_
#define UI_VIEWS_COCOA_TOOLTIP_MANAGER_MAC_H_

#include "ui/views/widget/tooltip_manager.h"

namespace remote_cocoa {
namespace mojom {
class NativeWidgetNSWindow;
}  // namespace mojom
}  // namespace remote_cocoa

namespace views {

// Manages native Cocoa tooltips for the given NativeWidgetNSWindowHostImpl.
class TooltipManagerMac : public TooltipManager {
 public:
  explicit TooltipManagerMac(remote_cocoa::mojom::NativeWidgetNSWindow* bridge);

  TooltipManagerMac(const TooltipManagerMac&) = delete;
  TooltipManagerMac& operator=(const TooltipManagerMac&) = delete;

  ~TooltipManagerMac() override;

  // TooltipManager:
  int GetMaxWidth(const gfx::Point& location) const override;
  const gfx::FontList& GetFontList() const override;
  void UpdateTooltip() override;
  void TooltipTextChanged(View* view) override;

 private:
  remote_cocoa::mojom::NativeWidgetNSWindow*
      bridge_;  // Weak. Owned by the owner of this.
};

}  // namespace views

#endif  // UI_VIEWS_COCOA_TOOLTIP_MANAGER_MAC_H_
