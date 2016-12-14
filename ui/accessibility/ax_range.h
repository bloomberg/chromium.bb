// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_ACCESSIBILITY_AX_RANGE_H_
#define UI_ACCESSIBILITY_AX_RANGE_H_

#include <memory>
#include <utility>

#include "base/strings/string16.h"

namespace ui {

// A range of ax positions.
//
// In order to avoid any confusion regarding whether a deep or a shallow copy is
// being performed, this class can be moved but not copied.
template <class AXPositionType>
class AXRange {
 public:
  AXRange()
      : anchor_(AXPositionType::CreateNullPosition()),
        focus_(AXPositionType::CreateNullPosition()) {}

  AXRange(std::unique_ptr<AXPositionType> anchor,
          std::unique_ptr<AXPositionType> focus) {
    if (anchor) {
      anchor_ = std::move(anchor);
    } else {
      anchor_ = AXPositionType::CreateNullPosition();
    }
    if (focus) {
      focus_ = std::move(focus);
    } else {
      focus = AXPositionType::CreateNullPosition();
    }
  }

  AXRange(const AXRange& other) = delete;

  AXRange(const AXRange&& other) : AXRange() {
    anchor_.swap(other.anchor_);
    focus_.swap(other.focus_);
  }

  AXRange& operator=(const AXRange& other) = delete;

  AXRange& operator=(const AXRange&& other) {
    if (this != other) {
      anchor_ = AXPositionType::CreateNullPosition();
      focus_ = AXPositionType::CreateNullPosition();
      anchor_.swap(other.anchor_);
      focus_.swap(other.focus_);
    }
    return *this;
  }

  virtual ~AXRange() {}

  bool IsNull() const {
    return !anchor_ || !focus_ || anchor_->IsNullPosition() ||
           focus_->IsNullPosition();
  }

  AXPositionType* anchor() const {
    DCHECK(anchor_);
    return anchor_.get();
  }

  AXPositionType* focus() const {
    DCHECK(focus_);
    return focus_.get();
  }

  base::string16 GetText() const {
    base::string16 text;
    if (IsNull())
      return text;

    std::unique_ptr<AXPositionType> first, second;
    if (*anchor_ <= *focus_) {
      first = anchor_->AsLeafTextPosition();
      second = focus_->AsLeafTextPosition();
    } else {
      first = focus_->AsLeafTextPosition();
      second = anchor_->AsLeafTextPosition();
    }

    do {
      text += first->GetInnerText();
      first = first->CreateNextTextAnchorPosition();
    } while (*first <= *second);

    return text;
  }

 private:
  std::unique_ptr<AXPositionType> anchor_;
  std::unique_ptr<AXPositionType> focus_;
};

}  // namespace ui

#endif  // UI_ACCESSIBILITY_AX_RANGE_H_
