// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/ksv/views/keyboard_shortcut_item_view.h"

#include <vector>

#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/ksv/keyboard_shortcut_viewer_metadata.h"
#include "ui/views/controls/styled_label.h"

namespace keyboard_shortcut_viewer {

namespace {

// The width of |shortcut_label_view_| as a ratio of its parent view's width.
// TODO(wutao): to be decided by UX specs.
constexpr float kShortcutViewWitdhRatio = 0.618f;

}  // namespace

KeyboardShortcutItemView::KeyboardShortcutItemView(
    const KeyboardShortcutItem& item) {
  description_label_view_ = new views::StyledLabel(
      l10n_util::GetStringUTF16(item.description_message_id), nullptr);
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
  DCHECK_EQ(replacement_strings.size(), offsets.size());
  for (size_t i = 0; i < offsets.size(); ++i) {
    views::StyledLabel::RangeStyleInfo style_info;
    // TODO(wutao): add rounded bubble views to highlight shortcut keys when
    // views::StyledLabel supports custom views.
    // TODO(wutao): add icons for keys.
    // TODO(wutao): finalize the sytles with UX specs.
    style_info.override_color = SK_ColorBLUE;
    style_info.disable_line_wrapping = true;
    shortcut_label_view_->AddStyleRange(
        gfx::Range(offsets[i], offsets[i] + replacement_strings[i].length()),
        style_info);
  }
  AddChildView(shortcut_label_view_);
}

int KeyboardShortcutItemView::GetHeightForWidth(int w) const {
  const int shortcut_view_height =
      shortcut_label_view_->GetHeightForWidth(w * kShortcutViewWitdhRatio);
  const int description_view_height =
      description_label_view_->GetHeightForWidth(w *
                                                 (1 - kShortcutViewWitdhRatio));
  return std::max(shortcut_view_height, description_view_height);
}

void KeyboardShortcutItemView::Layout() {
  gfx::Rect rect(GetContentsBounds());
  if (rect.IsEmpty())
    return;

  // TODO(wutao): addjust two views' bounds based on UX specs.
  const int shortcut_view_width = rect.width() * kShortcutViewWitdhRatio;
  const int description_view_width = rect.width() - shortcut_view_width;
  const int height = GetHeightForWidth(rect.width());
  description_label_view_->SetBoundsRect(
      gfx::Rect(description_view_width, height));
  shortcut_label_view_->SetBoundsRect(
      gfx::Rect(description_view_width, 0, shortcut_view_width, height));
}

}  // namespace keyboard_shortcut_viewer
