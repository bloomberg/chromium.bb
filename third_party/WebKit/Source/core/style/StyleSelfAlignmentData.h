// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleSelfAlignmentData_h
#define StyleSelfAlignmentData_h

#include "core/style/ComputedStyleConstants.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class StyleSelfAlignmentData {
  DISALLOW_NEW();

 public:
  // Style data for Self-Aligment and Default-Alignment properties: align-{self,
  // items}, justify-{self, items}.
  // [ <self-position> && <overflow-position>? ] | [ legacy && [ left | right |
  // center ] ]
  StyleSelfAlignmentData(ItemPosition position,
                         OverflowAlignment overflow,
                         ItemPositionType position_type = kNonLegacyPosition)
      : position_(position),
        position_type_(position_type),
        overflow_(overflow) {}

  void SetPosition(ItemPosition position) { position_ = position; }
  void SetPositionType(ItemPositionType position_type) {
    position_type_ = position_type;
  }
  void SetOverflow(OverflowAlignment overflow) { overflow_ = overflow; }

  ItemPosition GetPosition() const {
    return static_cast<ItemPosition>(position_);
  }
  ItemPositionType PositionType() const {
    return static_cast<ItemPositionType>(position_type_);
  }
  OverflowAlignment Overflow() const {
    return static_cast<OverflowAlignment>(overflow_);
  }

  bool operator==(const StyleSelfAlignmentData& o) const {
    return position_ == o.position_ && position_type_ == o.position_type_ &&
           overflow_ == o.overflow_;
  }

  bool operator!=(const StyleSelfAlignmentData& o) const {
    return !(*this == o);
  }

 private:
  unsigned position_ : 4;       // ItemPosition
  unsigned position_type_ : 1;  // Whether or not alignment uses the 'legacy'
                                // keyword.
  unsigned overflow_ : 2;       // OverflowAlignment
};

}  // namespace blink

#endif  // StyleSelfAlignmentData_h
