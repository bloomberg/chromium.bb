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

#ifndef LayoutRectOutsets_h
#define LayoutRectOutsets_h

#include "platform/LayoutUnit.h"
#include "platform/PlatformExport.h"
#include "platform/geometry/FloatRectOutsets.h"
#include "platform/geometry/IntRectOutsets.h"
#include "platform/text/WritingMode.h"
#include "platform/wtf/Allocator.h"

namespace blink {

// Specifies LayoutUnit lengths to be used to expand a rectangle.
// For example, |top()| returns the distance the top edge should be moved
// upward.
//
// Negative lengths can be used to express insets.
class PLATFORM_EXPORT LayoutRectOutsets {
  DISALLOW_NEW();

 public:
  LayoutRectOutsets() {}
  LayoutRectOutsets(LayoutUnit top,
                    LayoutUnit right,
                    LayoutUnit bottom,
                    LayoutUnit left)
      : top_(top), right_(right), bottom_(bottom), left_(left) {}
  LayoutRectOutsets(int top, int right, int bottom, int left)
      : top_(LayoutUnit(top)),
        right_(LayoutUnit(right)),
        bottom_(LayoutUnit(bottom)),
        left_(LayoutUnit(left)) {}

  LayoutRectOutsets(const IntRectOutsets& outsets)
      : top_(LayoutUnit(outsets.Top())),
        right_(LayoutUnit(outsets.Right())),
        bottom_(LayoutUnit(outsets.Bottom())),
        left_(LayoutUnit(outsets.Left())) {}

  LayoutRectOutsets(const FloatRectOutsets& outsets)
      : top_(LayoutUnit(outsets.Top())),
        right_(LayoutUnit(outsets.Right())),
        bottom_(LayoutUnit(outsets.Bottom())),
        left_(LayoutUnit(outsets.Left())) {}

  LayoutUnit Top() const { return top_; }
  LayoutUnit Right() const { return right_; }
  LayoutUnit Bottom() const { return bottom_; }
  LayoutUnit Left() const { return left_; }

  void SetTop(LayoutUnit value) { top_ = value; }
  void SetRight(LayoutUnit value) { right_ = value; }
  void SetBottom(LayoutUnit value) { bottom_ = value; }
  void SetLeft(LayoutUnit value) { left_ = value; }

  // Produces a new LayoutRectOutsets in line orientation
  // (https://www.w3.org/TR/css-writing-modes-3/#line-orientation), whose
  // - |top| is the logical 'over',
  // - |right| is the logical 'line right',
  // - |bottom| is the logical 'under',
  // - |left| is the logical 'line left'.
  LayoutRectOutsets LineOrientationOutsets(WritingMode) const;

  // The same as |logicalOutsets|, but also adjusting for flipped lines.
  LayoutRectOutsets LineOrientationOutsetsWithFlippedLines(WritingMode) const;

  bool operator==(const LayoutRectOutsets other) const {
    return Top() == other.Top() && Right() == other.Right() &&
           Bottom() == other.Bottom() && Left() == other.Left();
  }

 private:
  LayoutUnit top_;
  LayoutUnit right_;
  LayoutUnit bottom_;
  LayoutUnit left_;
};

inline LayoutRectOutsets& operator+=(LayoutRectOutsets& a,
                                     const LayoutRectOutsets& b) {
  a.SetTop(a.Top() + b.Top());
  a.SetRight(a.Right() + b.Right());
  a.SetBottom(a.Bottom() + b.Bottom());
  a.SetLeft(a.Left() + b.Left());
  return a;
}

inline LayoutRectOutsets operator+(const LayoutRectOutsets& a,
                                   const LayoutRectOutsets& b) {
  return LayoutRectOutsets(a.Top() + b.Top(), a.Right() + b.Right(),
                           a.Bottom() + b.Bottom(), a.Left() + b.Left());
}

inline LayoutRectOutsets operator-(const LayoutRectOutsets& a) {
  return LayoutRectOutsets(-a.Top(), -a.Right(), -a.Bottom(), -a.Left());
}

}  // namespace blink

#endif  // LayoutRectOutsets_h
