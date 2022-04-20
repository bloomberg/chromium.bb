// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/desk_action_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/close_button.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/background.h"

namespace ash {

namespace {

constexpr int kButtonMargin = 2;
constexpr int kButtonSpacing = 4;
constexpr int kCornerRadius = 11;

}  // namespace

DeskActionView::DeskActionView(
    const std::u16string& initial_combine_desks_target_name,
    base::RepeatingClosure combine_desks_callback,
    base::RepeatingClosure close_all_callback)
    : close_all_button_(AddChildView(
          std::make_unique<CloseButton>(std::move(close_all_callback),
                                        CloseButton::Type::kMediumFloating))),
      combine_desks_button_(AddChildView(
          std::make_unique<CloseButton>(std::move(combine_desks_callback),
                                        CloseButton::Type::kMediumFloating))) {
  SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  SetInsideBorderInsets(gfx::Insets(kButtonMargin));
  SetBetweenChildSpacing(kButtonSpacing);
  SetBackground(views::CreateRoundedRectBackground(
      AshColorProvider::Get()->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kTransparent80),
      kCornerRadius));

  close_all_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_ASH_DESKS_CLOSE_ALL_DESCRIPTION));
  combine_desks_button_->SetVectorIcon(kCombineDesksIcon);
  UpdateCombineDesksTooltip(initial_combine_desks_target_name);

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

void DeskActionView::UpdateCombineDesksTooltip(
    const std::u16string& new_combine_desks_target_name) {
  combine_desks_button_->SetTooltipText(l10n_util::GetStringFUTF16(
      IDS_ASH_DESKS_COMBINE_DESKS_DESCRIPTION, new_combine_desks_target_name));
}

BEGIN_METADATA(DeskActionView, views::BoxLayoutView)
END_METADATA

}  // namespace ash