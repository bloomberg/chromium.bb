/*
 * Copyright (c) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "platform/geometry/LayoutRectOutsets.h"

#include <algorithm>
#include "platform/wtf/Assertions.h"

namespace blink {

LayoutUnit LayoutRectOutsets::LogicalTop(WritingMode writing_mode) const {
  return IsHorizontalWritingMode(writing_mode) ? top_ : left_;
}

LayoutUnit LayoutRectOutsets::LogicalBottom(WritingMode writing_mode) const {
  return IsHorizontalWritingMode(writing_mode) ? bottom_ : right_;
}

LayoutUnit LayoutRectOutsets::LogicalLeft(WritingMode writing_mode) const {
  return IsHorizontalWritingMode(writing_mode) ? left_ : top_;
}

LayoutUnit LayoutRectOutsets::LogicalRight(WritingMode writing_mode) const {
  return IsHorizontalWritingMode(writing_mode) ? right_ : bottom_;
}

LayoutRectOutsets LayoutRectOutsets::LogicalOutsets(
    WritingMode writing_mode) const {
  if (!IsHorizontalWritingMode(writing_mode))
    return LayoutRectOutsets(left_, bottom_, right_, top_);
  return *this;
}

LayoutRectOutsets LayoutRectOutsets::LogicalOutsetsWithFlippedLines(
    WritingMode writing_mode) const {
  LayoutRectOutsets outsets = LogicalOutsets(writing_mode);
  if (IsFlippedLinesWritingMode(writing_mode))
    std::swap(outsets.top_, outsets.bottom_);
  return outsets;
}

LayoutUnit LayoutRectOutsets::Before(WritingMode writing_mode) const {
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      return top_;
    case WritingMode::kVerticalLr:
      return left_;
    case WritingMode::kVerticalRl:
      return right_;
  }
  NOTREACHED();
  return top_;
}

LayoutUnit LayoutRectOutsets::After(WritingMode writing_mode) const {
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      return bottom_;
    case WritingMode::kVerticalLr:
      return right_;
    case WritingMode::kVerticalRl:
      return left_;
  }
  NOTREACHED();
  return bottom_;
}

LayoutUnit LayoutRectOutsets::Start(WritingMode writing_mode,
                                    TextDirection direction) const {
  if (IsHorizontalWritingMode(writing_mode))
    return IsLtr(direction) ? left_ : right_;
  return IsLtr(direction) ? top_ : bottom_;
}

LayoutUnit LayoutRectOutsets::end(WritingMode writing_mode,
                                  TextDirection direction) const {
  if (IsHorizontalWritingMode(writing_mode))
    return IsLtr(direction) ? right_ : left_;
  return IsLtr(direction) ? bottom_ : top_;
}

LayoutUnit LayoutRectOutsets::Over(WritingMode writing_mode) const {
  return IsHorizontalWritingMode(writing_mode) ? top_ : right_;
}

LayoutUnit LayoutRectOutsets::Under(WritingMode writing_mode) const {
  return IsHorizontalWritingMode(writing_mode) ? bottom_ : left_;
}

void LayoutRectOutsets::SetBefore(WritingMode writing_mode, LayoutUnit value) {
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      top_ = value;
      break;
    case WritingMode::kVerticalLr:
      left_ = value;
      break;
    case WritingMode::kVerticalRl:
      right_ = value;
      break;
    default:
      NOTREACHED();
      top_ = value;
  }
}

void LayoutRectOutsets::SetAfter(WritingMode writing_mode, LayoutUnit value) {
  switch (writing_mode) {
    case WritingMode::kHorizontalTb:
      bottom_ = value;
      break;
    case WritingMode::kVerticalLr:
      right_ = value;
      break;
    case WritingMode::kVerticalRl:
      left_ = value;
      break;
    default:
      NOTREACHED();
      bottom_ = value;
  }
}

void LayoutRectOutsets::SetStart(WritingMode writing_mode,
                                 TextDirection direction,
                                 LayoutUnit value) {
  if (IsHorizontalWritingMode(writing_mode)) {
    if (IsLtr(direction))
      left_ = value;
    else
      right_ = value;
  } else {
    if (IsLtr(direction))
      top_ = value;
    else
      bottom_ = value;
  }
}

void LayoutRectOutsets::SetEnd(WritingMode writing_mode,
                               TextDirection direction,
                               LayoutUnit value) {
  if (IsHorizontalWritingMode(writing_mode)) {
    if (IsLtr(direction))
      right_ = value;
    else
      left_ = value;
  } else {
    if (IsLtr(direction))
      bottom_ = value;
    else
      top_ = value;
  }
}

}  // namespace blink
