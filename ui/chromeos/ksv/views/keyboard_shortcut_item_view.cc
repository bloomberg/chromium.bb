// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/ksv/views/keyboard_shortcut_item_view.h"

#include <memory>
#include <vector>

#include "base/i18n/rtl.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/ksv/keyboard_shortcut_item.h"
#include "ui/chromeos/ksv/keyboard_shortcut_viewer_metadata.h"
#include "ui/chromeos/ksv/views/bubble_view.h"
#include "ui/views/border.h"
#include "ui/views/controls/styled_label.h"

namespace keyboard_shortcut_viewer {

namespace {

// The width of |shortcut_label_view_| as a ratio of its parent view's width.
// TODO(wutao): to be decided by UX specs.
constexpr float kShortcutViewWitdhRatio = 0.618f;

}  // namespace

KeyboardShortcutItemView::KeyboardShortcutItemView(
    const KeyboardShortcutItem& item,
    ShortcutCategory category)
    : shortcut_item_(&item), category_(category) {
  description_label_view_ = new views::StyledLabel(
      l10n_util::GetStringUTF16(item.description_message_id), nullptr);
  // StyledLabel will flip the alignment if UI layout is right-to-left.
  // Flip the alignment here in order to make |description_label_view_| always
  // align to left.
  description_label_view_->SetHorizontalAlignment(
      base::i18n::IsRTL() ? gfx::ALIGN_RIGHT : gfx::ALIGN_LEFT);
  AddChildView(description_label_view_);

  std::vector<size_t> offsets;
  std::vector<base::string16> replacement_strings;
  for (ui::KeyboardCode key_code : item.shortcut_key_codes)
    replacement_strings.emplace_back(GetStringForKeyboardCode(key_code));

  base::string16 shortcut_string;
  if (replacement_strings.empty()) {
    shortcut_string = l10n_util::GetStringUTF16(item.shortcut_message_id);
  } else {
    shortcut_string = l10n_util::GetStringFUTF16(item.shortcut_message_id,
                                                 replacement_strings, &offsets);
  }
  shortcut_label_view_ = new views::StyledLabel(shortcut_string, nullptr);
  // StyledLabel will flip the alignment if UI layout is right-to-left.
  // Flip the alignment here in order to make |shortcut_label_view_| always
  // align to right.
  shortcut_label_view_->SetHorizontalAlignment(
      base::i18n::IsRTL() ? gfx::ALIGN_LEFT : gfx::ALIGN_RIGHT);
  DCHECK_EQ(replacement_strings.size(), offsets.size());
  for (size_t i = 0; i < offsets.size(); ++i) {
    views::StyledLabel::RangeStyleInfo style_info;
    // TODO(wutao): add icons for keys.
    // TODO(wutao): finalize the sytles with UX specs.
    style_info.override_color = SK_ColorBLUE;
    style_info.disable_line_wrapping = true;
    auto bubble_view = std::make_unique<BubbleView>();
    bubble_view->set_owned_by_client();
    bubble_view->SetText(replacement_strings[i]);
    style_info.custom_view = bubble_view.get();
    shortcut_label_view_->AddCustomView(std::move(bubble_view));
    shortcut_label_view_->AddStyleRange(
        gfx::Range(offsets[i], offsets[i] + replacement_strings[i].length()),
        style_info);
  }
  AddChildView(shortcut_label_view_);

  constexpr int kVerticalPadding = 10;
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(kVerticalPadding, 0, kVerticalPadding, 0)));
}

int KeyboardShortcutItemView::GetHeightForWidth(int w) const {
  const int shortcut_view_height =
      shortcut_label_view_->GetHeightForWidth(w * kShortcutViewWitdhRatio);
  const int description_view_height =
      description_label_view_->GetHeightForWidth(w *
                                                 (1 - kShortcutViewWitdhRatio));
  return std::max(shortcut_view_height, description_view_height) +
         GetInsets().height();
}

void KeyboardShortcutItemView::Layout() {
  gfx::Rect content_bounds(GetContentsBounds());
  if (content_bounds.IsEmpty())
    return;

  // TODO(wutao): addjust two views' bounds based on UX specs.
  const int shortcut_view_width =
      content_bounds.width() * kShortcutViewWitdhRatio;
  const int description_view_width =
      content_bounds.width() - shortcut_view_width;
  const int height = GetHeightForWidth(content_bounds.width());
  const int left = content_bounds.x();
  const int top = content_bounds.y();
  description_label_view_->SetBoundsRect(
      gfx::Rect(left, top, description_view_width, height));
  shortcut_label_view_->SetBoundsRect(gfx::Rect(
      left + description_view_width, top, shortcut_view_width, height));
}

}  // namespace keyboard_shortcut_viewer
