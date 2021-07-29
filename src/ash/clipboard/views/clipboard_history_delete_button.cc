// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/clipboard/views/clipboard_history_delete_button.h"

#include "ash/clipboard/views/clipboard_history_item_view.h"
#include "ash/clipboard/views/clipboard_history_view_constants.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/scoped_light_mode_as_default.h"
#include "base/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/strings/grit/ui_strings.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/controls/highlight_path_generator.h"

namespace ash {
ClipboardHistoryDeleteButton::ClipboardHistoryDeleteButton(
    ClipboardHistoryItemView* listener)
    : views::ImageButton(base::BindRepeating(
          [](ClipboardHistoryItemView* item_view, const ui::Event& event) {
            item_view->HandleDeleteButtonPressEvent(event);
          },
          base::Unretained(listener))),
      listener_(listener) {
  SetID(ClipboardHistoryUtil::kDeleteButtonViewID);
  SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_CLIPBOARD_HISTORY_DELETE_BUTTON));
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  SetPreferredSize(gfx::Size(ClipboardHistoryViews::kDeleteButtonSizeDip,
                             ClipboardHistoryViews::kDeleteButtonSizeDip));
  SetVisible(false);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  ink_drop_container_ =
      AddChildView(std::make_unique<views::InkDropContainerView>());

  // Typically we should not create a layer for a view used in the clipboard
  // history menu. Because if a layer extends outside of the menu's bounds, it
  // does not get cut (in addition, due to the lack of ownership, it is hard to
  // change this behavior). However, it is safe to paint to layer here since the
  // default visibility is false,
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);

  // The ink drop ripple should be circular.
  views::InstallFixedSizeCircleHighlightPathGenerator(
      this, ClipboardHistoryViews::kDeleteButtonSizeDip / 2);
  views::InkDrop::UseInkDropForFloodFillRipple(views::InkDrop::Get(this),
                                               /*highlight_on_hover=*/false,
                                               /*highlight_on_focus=*/true);
}

ClipboardHistoryDeleteButton::~ClipboardHistoryDeleteButton() {
  // TODO(pbos): Revisit explicit removal of InkDrop for classes that override
  // Add/RemoveLayerBeneathView(). This is done so that the InkDrop doesn't
  // access the non-override versions in ~View.
  views::InkDrop::Remove(this);
}

const char* ClipboardHistoryDeleteButton::GetClassName() const {
  return "DeleteButton";
}

void ClipboardHistoryDeleteButton::AddLayerBeneathView(ui::Layer* layer) {
  ink_drop_container_->AddLayerBeneathView(layer);
}

void ClipboardHistoryDeleteButton::OnClickCanceled(const ui::Event& event) {
  DCHECK(event.IsMouseEvent());

  listener_->OnMouseClickOnDescendantCanceled();
  views::Button::OnClickCanceled(event);
}

void ClipboardHistoryDeleteButton::OnThemeChanged() {
  // Use the light mode as default because the light mode is the default mode of
  // the native theme which decides the context menu's background color.
  // TODO(andrewxu): remove this line after https://crbug.com/1143009 is fixed.
  ScopedLightModeAsDefault scoped_light_mode_as_default;

  views::ImageButton::OnThemeChanged();
  AshColorProvider::Get()->DecorateCloseButton(
      this, ClipboardHistoryViews::kDeleteButtonSizeDip, kCloseButtonIcon);

  const AshColorProvider::RippleAttributes ripple_attributes =
      AshColorProvider::Get()->GetRippleAttributes();
  views::InkDrop::Get(this)->SetBaseColor(ripple_attributes.base_color);
  views::InkDrop::Get(this)->SetVisibleOpacity(
      ripple_attributes.inkdrop_opacity);
  views::InkDrop::Get(this)->SetHighlightOpacity(
      ripple_attributes.highlight_opacity);
}

void ClipboardHistoryDeleteButton::RemoveLayerBeneathView(ui::Layer* layer) {
  ink_drop_container_->RemoveLayerBeneathView(layer);
}

}  // namespace ash
