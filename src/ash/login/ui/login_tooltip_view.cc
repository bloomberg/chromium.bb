// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_tooltip_view.h"

#include "ash/login/ui/views_utils.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Horizontal and vertical padding of login tooltip view.
constexpr int kHorizontalPaddingLoginTooltipViewDp = 8;
constexpr int kVerticalPaddingLoginTooltipViewDp = 8;

}  // namespace

LoginTooltipView::LoginTooltipView(const base::string16& message,
                                   views::View* anchor_view)
    : LoginBaseBubbleView(anchor_view) {
  SetText(message);
}

LoginTooltipView::~LoginTooltipView() = default;

void LoginTooltipView::SetText(const base::string16& message) {
  views::Label* text =
      login_views_utils::CreateBubbleLabel(message, SK_ColorWHITE);
  text->SetMultiLine(true);
  RemoveAllChildViews(true /*delete_children*/);
  AddChildView(text);
}

void LoginTooltipView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kTooltip;
}

gfx::Size LoginTooltipView::CalculatePreferredSize() const {
  gfx::Size size;

  if (GetAnchorView())
    size.set_width(GetAnchorView()->width());

  size.set_height(GetHeightForWidth(size.width()));
  return size;
}

gfx::Point LoginTooltipView::CalculatePosition() {
  return CalculatePositionUsingDefaultStrategy(
      PositioningStrategy::kShowOnLeftSideOrRightSide,
      kHorizontalPaddingLoginTooltipViewDp, kVerticalPaddingLoginTooltipViewDp);
}

}  // namespace ash
