// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FloatClipRect_h
#define FloatClipRect_h

#include "platform/geometry/FloatRect.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class PLATFORM_EXPORT FloatClipRect {
  USING_FAST_MALLOC(FloatClipRect);

 public:
  FloatClipRect()
      : rect_(FloatRect(LayoutRect::InfiniteIntRect())),
        has_radius_(false),
        is_infinite_(true) {}

  explicit FloatClipRect(const FloatRect& rect)
      : rect_(rect), has_radius_(false), is_infinite_(false) {}

  const FloatRect& Rect() const { return rect_; }

  FloatRect& Rect() { return rect_; }

  void Intersect(const FloatRect& other) {
    if (is_infinite_) {
      rect_ = other;
      is_infinite_ = false;
    } else {
      rect_.Intersect(other);
    }
  }

  bool HasRadius() const { return has_radius_; }
  void SetHasRadius() {
    has_radius_ = true;
    is_infinite_ = false;
  }

  void SetRect(const FloatRect& rect) {
    rect_ = rect;
    is_infinite_ = false;
  }

  void MoveBy(const FloatPoint& offset) {
    if (is_infinite_)
      return;
    rect_.MoveBy(offset);
  }

  bool IsInfinite() const { return is_infinite_; }

 private:
  FloatRect rect_;
  bool has_radius_ : 1;
  bool is_infinite_ : 1;
};

inline bool operator==(const FloatClipRect& a, const FloatClipRect& b) {
  if (a.IsInfinite() && b.IsInfinite())
    return true;
  if (!a.IsInfinite() && !b.IsInfinite())
    return a.Rect() == b.Rect() && a.HasRadius() == b.HasRadius();
  return false;
}

}  // namespace blink

#endif  // FloatClipRect_h
