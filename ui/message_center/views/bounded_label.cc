// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/message_center/views/bounded_label.h"

#include <limits>

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/canvas.h"

namespace message_center {

BoundedLabel::BoundedLabel(string16 text, size_t max_lines)
    : views::Label(text, gfx::Font()) {
  Init(max_lines);
}

BoundedLabel::BoundedLabel(string16 text, gfx::Font font, size_t max_lines)
    : views::Label(text, font) {
  Init(max_lines);
}

BoundedLabel::~BoundedLabel() {
}

void BoundedLabel::SetMaxLines(size_t lines) {
  is_preferred_lines_valid_ = false;
  is_text_size_valid_ = false;
  max_lines_ = lines;
}

size_t BoundedLabel::GetMaxLines() {
  return max_lines_;
}

size_t BoundedLabel::GetPreferredLines() {
  if (!is_preferred_lines_valid_) {
    int wrap_width = width() - GetInsets().width();
    int unlimited_lines = std::numeric_limits<size_t>::max();
    preferred_lines_ = SplitLines(wrap_width, unlimited_lines).size();
    is_preferred_lines_valid_ = true;
  }
  return preferred_lines_;
}

int BoundedLabel::GetHeightForWidth(int width) {
  gfx::Insets insets = GetInsets();
  return GetTextHeightForWidth(width - insets.width()) + insets.height();
}

gfx::Size BoundedLabel::GetTextSize() const {
  if (!is_text_size_valid_) {
    text_size_.set_width(width() - GetInsets().width());
    text_size_.set_height(GetTextHeightForWidth(text_size_.width()));
    is_text_size_valid_ = true;
  }
  return text_size_;
}

void BoundedLabel::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  is_preferred_lines_valid_ = false;
  is_text_size_valid_ = false;
}

void BoundedLabel::PaintText(gfx::Canvas* canvas,
                             const string16& paint_text,
                             const gfx::Rect& text_bounds,
                             int flags) {
  gfx::Insets insets = GetInsets();
  gfx::Rect bounds(gfx::Point(insets.left(), insets.top()), GetTextSize());
  string16 text = JoinString(SplitLines(bounds.width(), max_lines_), '\n');
  views::Label::PaintText(canvas, text, bounds, GetTextFlags());
}

void BoundedLabel::Init(size_t max_lines) {
  SetMultiLine(true);
  SetAllowCharacterBreak(true);
  SetElideBehavior(views::Label::ELIDE_AT_END);
  max_lines_ = max_lines;
  is_preferred_lines_valid_ = false;
  is_text_size_valid_ = false;
}

int BoundedLabel::GetTextFlags() const {
  int flags = gfx::Canvas::MULTI_LINE | gfx::Canvas::CHARACTER_BREAK;

  // We can't use subpixel rendering if the background is non-opaque.
  if (SkColorGetA(background_color()) != 0xFF)
    flags |= gfx::Canvas::NO_SUBPIXEL_RENDERING;

  if (directionality_mode() == AUTO_DETECT_DIRECTIONALITY) {
    base::i18n::TextDirection direction =
        base::i18n::GetFirstStrongCharacterDirection(text());
    if (direction == base::i18n::RIGHT_TO_LEFT)
      flags |= gfx::Canvas::FORCE_RTL_DIRECTIONALITY;
    else
      flags |= gfx::Canvas::FORCE_LTR_DIRECTIONALITY;
  }

  switch (horizontal_alignment()) {
    case gfx::ALIGN_LEFT:
      flags |= gfx::Canvas::TEXT_ALIGN_LEFT;
      break;
    case gfx::ALIGN_CENTER:
      flags |= gfx::Canvas::TEXT_ALIGN_CENTER;
      break;
    case gfx::ALIGN_RIGHT:
      flags |= gfx::Canvas::TEXT_ALIGN_RIGHT;
      break;
  }

  return flags;
}

int BoundedLabel::GetTextHeightForWidth(int width) const {
  int height = font().GetHeight();
  width = std::max(width, 0);
  string16 text = JoinString(SplitLines(width, max_lines_), '\n');
  gfx::Canvas::SizeStringInt(text, font(), &width, &height, GetTextFlags());
  return height;
}

std::vector<string16> BoundedLabel::SplitLines(int width,
                                               size_t max_lines) const {
  // Adjust max_lines so (max_lines + 1) * line_height <= INT_MAX, then use the
  // adjusted max_lines to get a max_height of (max_lines + 1) * line_height.
  size_t max_height = std::numeric_limits<int>::max();
  size_t line_height = std::max(font().GetHeight(), 2);  // At least 2 pixels!
  max_lines = std::min(max_lines, max_height / line_height - 1);
  max_height = (max_lines + 1) * line_height;

  // Split. Do not use ui::WRAP_LONG_WORDS instead of ui::IGNORE_LONG_WORDS
  // below as this may cause ui::ElideRectangleText() to go into an infinite
  // loop for small width values.
  std::vector<string16> lines;
  ui::ElideRectangleText(text(), font(), width, max_height,
                         ui::IGNORE_LONG_WORDS, &lines);

  // Elide if necessary.
  if (lines.size() > max_lines) {
    // Add an ellipsis to the last line. If this ellipsis makes the last line
    // too wide, that line will be further elided by the ui::ElideText below,
    // so for example "ABC" could become "ABC..." here and "AB..." below.
    string16 last = lines[max_lines - 1] + UTF8ToUTF16(ui::kEllipsis);
    lines.resize(max_lines - 1);
    lines.push_back((font().GetStringWidth(last) > width) ?
                    ui::ElideText(last, font(), width, ui::ELIDE_AT_END) :
                    last);
  }

  return lines;
}

}  // namespace message_center
