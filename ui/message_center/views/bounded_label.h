// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_MESSAGE_CENTER_BOUNDED_LABEL_H_
#define UI_MESSAGE_CENTER_BOUNDED_LABEL_H_

#include "testing/gtest/include/gtest/gtest_prod.h"
#include "ui/message_center/message_center_export.h"
#include "ui/views/controls/label.h"

namespace message_center {

// BoundedLabels display text up to a maximum number of lines, with ellipsis at
// the end of the last line for any omitted text. The font, text, multiline
// behavior, character break behavior, elide behavior, and focus border of
// instances of this views::Label subclass cannot be changed, so the following
// views::Label methods should never be called with instances of this class:
//
//   SetFont()
//   SetText()
//   SetMultiLine()
//   SetAllowCharacterBreak()
//   SetElideBehavior()
//   SetHasFocusBorder()
//
class MESSAGE_CENTER_EXPORT BoundedLabel : public views::Label {
 public:
  BoundedLabel(string16 text, size_t max_lines);
  BoundedLabel(string16 text, gfx::Font font, size_t max_lines);
  virtual ~BoundedLabel();

  void SetMaxLines(size_t lines);
  size_t GetMaxLines();
  size_t GetPreferredLines();

  virtual int GetHeightForWidth(int width) OVERRIDE;

 protected:
  // Overridden from views::Label.
  virtual gfx::Size GetTextSize() const OVERRIDE;
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;
  virtual void PaintText(gfx::Canvas* canvas,
                         const string16& text,
                         const gfx::Rect& text_bounds,
                         int flags) OVERRIDE;

 private:
  friend class BoundedLabelTest;

  void Init(size_t max_lines);
  int GetTextFlags() const;
  int GetTextHeightForWidth(int width) const;
  std::vector<string16> SplitLines(int width, size_t max_lines) const;

  size_t max_lines_;
  size_t preferred_lines_;
  bool is_preferred_lines_valid_;
  mutable gfx::Size text_size_;
  mutable bool is_text_size_valid_;

  DISALLOW_COPY_AND_ASSIGN(BoundedLabel);
};

}  // namespace message_center

#endif // UI_MESSAGE_CENTER_BOUNDED_LABEL_H_
