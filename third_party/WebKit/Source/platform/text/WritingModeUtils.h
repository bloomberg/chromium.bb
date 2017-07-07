// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license thaT can be
// found in the LICENSE file.

#ifndef WritingModeUtils_h
#define WritingModeUtils_h

#include "platform/text/TextDirection.h"
#include "platform/text/WritingMode.h"
#include "platform/wtf/Allocator.h"

namespace blink {

template <typename T>
class PhysicalToLogical {
  STACK_ALLOCATED();

 public:
  PhysicalToLogical(WritingMode writing_mode,
                    TextDirection direction,
                    T top,
                    T right,
                    T bottom,
                    T left)
      : writing_mode_(writing_mode),
        direction_(direction),
        top_(top),
        right_(right),
        bottom_(bottom),
        left_(left) {}

  T InlineStart() const {
    if (IsHorizontalWritingMode(writing_mode_))
      return IsLtr(direction_) ? left_ : right_;
    return IsLtr(direction_) ? top_ : bottom_;
  }

  T InlineEnd() const {
    if (IsHorizontalWritingMode(writing_mode_))
      return IsLtr(direction_) ? right_ : left_;
    return IsLtr(direction_) ? bottom_ : top_;
  }

  T BlockStart() const {
    if (IsHorizontalWritingMode(writing_mode_))
      return top_;
    return IsFlippedBlocksWritingMode(writing_mode_) ? right_ : left_;
  }

  T BlockEnd() const {
    if (IsHorizontalWritingMode(writing_mode_))
      return bottom_;
    return IsFlippedBlocksWritingMode(writing_mode_) ? left_ : right_;
  }

  T Over() const {
    return IsHorizontalWritingMode(writing_mode_) ? top_ : right_;
  }

  T Under() const {
    return IsHorizontalWritingMode(writing_mode_) ? bottom_ : left_;
  }

  T LineLeft() const {
    return IsHorizontalWritingMode(writing_mode_) ? left_ : top_;
  }

  T LineRight() const {
    return IsHorizontalWritingMode(writing_mode_) ? right_ : bottom_;
  }

  // Legacy logical directions.
  T Start() const { return InlineStart(); }
  T End() const { return InlineEnd(); }
  T Before() const { return BlockStart(); }
  T After() const { return BlockEnd(); }

 private:
  WritingMode writing_mode_;
  TextDirection direction_;
  T top_;
  T right_;
  T bottom_;
  T left_;
};

template <typename T>
class LogicalToPhysical {
  STACK_ALLOCATED();

 public:
  LogicalToPhysical(WritingMode writing_mode,
                    TextDirection direction,
                    T inline_start,
                    T inline_end,
                    T block_start,
                    T block_end)
      : writing_mode_(writing_mode),
        direction_(direction),
        inline_start_(inline_start),
        inline_end_(inline_end),
        block_start_(block_start),
        block_end_(block_end) {}

  T Left() const {
    if (IsHorizontalWritingMode(writing_mode_))
      return IsLtr(direction_) ? inline_start_ : inline_end_;
    return IsFlippedBlocksWritingMode(writing_mode_) ? block_end_
                                                     : block_start_;
  }

  T Right() const {
    if (IsHorizontalWritingMode(writing_mode_))
      return IsLtr(direction_) ? inline_end_ : inline_start_;
    return IsFlippedBlocksWritingMode(writing_mode_) ? block_start_
                                                     : block_end_;
  }

  T Top() const {
    if (IsHorizontalWritingMode(writing_mode_))
      return block_start_;
    return IsLtr(direction_) ? inline_start_ : inline_end_;
  }

  T Bottom() const {
    if (IsHorizontalWritingMode(writing_mode_))
      return block_end_;
    return IsLtr(direction_) ? inline_end_ : inline_start_;
  }

 private:
  WritingMode writing_mode_;
  TextDirection direction_;
  T inline_start_;  // a.k.a. start
  T inline_end_;    // a.k.a. end
  T block_start_;   // a.k.a. before
  T block_end_;     // a.k.a. after
};

}  // namespace blink

#endif  // WritingModeUtils_h
