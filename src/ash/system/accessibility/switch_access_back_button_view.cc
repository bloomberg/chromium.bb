// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/accessibility/switch_access_back_button_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/accessibility/floating_menu_button.h"
#include "ash/system/tray/tray_constants.h"
#include "cc/paint/paint_flags.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/accessibility/mojom/ax_node_data.mojom-shared.h"
#include "ui/events/event.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_palette.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {
constexpr char kUniqueId[] = "switch_access_back_button";
}  // namespace

SwitchAccessBackButtonView::SwitchAccessBackButtonView(int diameter)
    : diameter_(diameter),
      back_button_(
          new FloatingMenuButton(this,
                                 kSwitchAccessBackIcon,
                                 IDS_ASH_SWITCH_ACCESS_BACK_BUTTON_DESCRIPTION,
                                 /*flip_for_rtl=*/false,
                                 diameter,
                                 /*draw_highlight=*/true,
                                 /*is_a11y_togglable=*/false)) {
  std::unique_ptr<views::BoxLayout> layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, gfx::Insets());
  SetLayoutManager(std::move(layout));
  AddChildView(back_button_);
}

void SwitchAccessBackButtonView::ButtonPressed(views::Button* sender,
                                               const ui::Event& event) {
  NotifyAccessibilityEvent(ax::mojom::Event::kClicked,
                           /*send_native_event=*/false);
}

void SwitchAccessBackButtonView::GetAccessibleNodeData(
    ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kButton;
  node_data->html_attributes.push_back(std::make_pair("id", kUniqueId));
}

const char* SwitchAccessBackButtonView::GetClassName() const {
  return "SwitchAccessBackButtonView";
}

void SwitchAccessBackButtonView::OnPaint(gfx::Canvas* canvas) {
  gfx::Rect rect(GetContentsBounds());
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  flags.setColor(gfx::kGoogleGrey800);
  flags.setStyle(cc::PaintFlags::kFill_Style);
  canvas->DrawCircle(gfx::PointF(rect.CenterPoint()), diameter_ / 2.f, flags);
}

}  // namespace ash
