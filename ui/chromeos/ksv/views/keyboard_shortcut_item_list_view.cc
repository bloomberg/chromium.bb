// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/ksv/views/keyboard_shortcut_item_list_view.h"

#include "ui/base/default_style.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/chromeos/ksv/views/keyboard_shortcut_item_view.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace keyboard_shortcut_viewer {

KeyboardShortcutItemListView::KeyboardShortcutItemListView() {
  constexpr int kHorizontalPadding = 32;
  auto layout = std::make_unique<views::BoxLayout>(views::BoxLayout::kVertical);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_CENTER);
  SetLayoutManager(std::move(layout));
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(0, kHorizontalPadding, 0, kHorizontalPadding)));
}

void KeyboardShortcutItemListView::AddCategoryLabel(
    const base::string16& text) {
  constexpr int kLabelTopPadding = 44;
  constexpr int kLabelBottomPadding = 20;
  constexpr SkColor kLabelColor = SkColorSetARGBMacro(0xFF, 0x42, 0x85, 0xF4);

  views::Label* category_label = new views::Label(text);
  category_label->SetHorizontalAlignment(gfx::ALIGN_TO_HEAD);
  category_label->SetBorder(views::CreateEmptyBorder(
      gfx::Insets(kLabelTopPadding, 0, kLabelBottomPadding, 0)));
  category_label->SetEnabledColor(kLabelColor);
  category_label->SetFontList(
      ui::ResourceBundle::GetSharedInstance().GetFontListWithDelta(
          ui::kLabelFontSizeDelta, gfx::Font::NORMAL, gfx::Font::Weight::BOLD));
  AddChildView(category_label);
}

}  // namespace keyboard_shortcut_viewer
