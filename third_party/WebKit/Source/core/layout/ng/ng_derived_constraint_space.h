// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NGConstraintSpace_h
#define NGConstraintSpace_h

#include "core/CoreExport.h"
#include "core/layout/ng/ng_constraint_space.h"
#include "core/layout/ng/ng_units.h"
#include "platform/LayoutUnit.h"
#include "wtf/DoublyLinkedList.h"

namespace blink {

class CORE_EXPORT NGDerivedConstraintSpace final : public NGConstraintSpace {
 public:
  ~NGDerivedConstraintSpace();

  LayoutUnit offset() const { return offset_; }
  LayoutUnit size() const { return size_; }

 private:
  NGDerivedConstraintSpace(const NGConstraintSpace* original,
                           NGLogicalOffset offset,
                           NGLogicalSize size,
                           NGWritingMode writingMode,
                           NGDirection direction)
      : original_(original),
        offset_(offset),
        size_(size),
        writingMode_(writingMode),
        direction_(direction) {}

  const NGConstraintSpace* original_;
  NGLogicalOffset offset_;
  NGLogicalSize size_;
  NGWritingMode writingMode_;
  NGDirection direction_;
};

}  // namespace blink

#endif  // NGConstraintSpace_h
