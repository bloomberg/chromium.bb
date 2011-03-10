// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_TEXTFIELD_TEXT_RANGE_H_
#define VIEWS_CONTROLS_TEXTFIELD_TEXT_RANGE_H_
#pragma once

#include <stddef.h>

#include <algorithm>

namespace views {

// TextRange specifies the range of text in the Textfield.  This is used to
// specify selected text and will be used to change the attributes of characters
// in the textfield. When this is used for selection, the end is caret position,
// and the start is where selection started.  The range preserves the direction,
// and selecting from the end to the begining is considered "reverse" order.
// (that is, start > end is reverse)
class TextRange {
 public:
  TextRange() : start_(0), end_(0) {}
  TextRange(size_t start, size_t end) : start_(start), end_(end) {}

  // Allow copy so that the omnibox can save the view state for each tabs.
  explicit TextRange(const TextRange& range)
      : start_(range.start_),
        end_(range.end_) {}

  // Returns the start position;
  size_t start() const { return start_; }

  // Returns the end position.
  size_t end() const { return end_; }

  // Returns true if the selected text is empty.
  bool is_empty() const { return start_ == end_; }

  // Returns true if the selection is made in reverse order.
  bool is_reverse() const { return start_ > end_; }

  // Returns the min of selected range.
  size_t GetMin() const { return std::min(start_, end_); }

  // Returns the max of selected range.
  size_t GetMax() const { return std::max(start_, end_); }

  // Returns true if the the selection range is same ignoring the direction.
  bool EqualsIgnoringDirection(const TextRange& range) const {
    return GetMin() == range.GetMin() && GetMax() == range.GetMax();
  }

  // Set the range with |start| and |end|.
  void SetRange(size_t start, size_t end) {
    start_ = start;
    end_ = end;
  }

 private:
  size_t start_;
  size_t end_;
};

}  // namespace views

#endif  // VIEWS_CONTROLS_TEXTFIELD_TEXT_RANGE_H_
