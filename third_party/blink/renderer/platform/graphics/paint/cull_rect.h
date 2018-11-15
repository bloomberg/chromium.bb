// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_CULL_RECT_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_CULL_RECT_H_

#include "third_party/blink/renderer/platform/geometry/int_rect.h"
#include "third_party/blink/renderer/platform/geometry/layout_rect.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"

#include <limits>

namespace blink {

class AffineTransform;
class FloatRect;
class LayoutRect;
class LayoutUnit;
class TransformPaintPropertyNode;

class PLATFORM_EXPORT CullRect {
  DISALLOW_NEW();

 public:
  CullRect() = default;
  explicit CullRect(const IntRect& rect) : rect_(rect) {}

  static CullRect Infinite() { return CullRect(LayoutRect::InfiniteIntRect()); }

  bool IsInfinite() const { return rect_ == LayoutRect::InfiniteIntRect(); }

  bool Intersects(const IntRect&) const;
  bool Intersects(const LayoutRect&) const;
  bool IntersectsTransformed(const AffineTransform&, const FloatRect&) const;
  bool IntersectsHorizontalRange(LayoutUnit lo, LayoutUnit hi) const;
  bool IntersectsVerticalRange(LayoutUnit lo, LayoutUnit hi) const;

  void MoveBy(const IntPoint& offset);
  void Move(const IntSize& offset);
  void ApplyTransform(const TransformPaintPropertyNode*);

  const IntRect& Rect() const { return rect_; }

  String ToString() const { return rect_.ToString(); }

 private:
  IntRect rect_;
};

inline bool operator==(const CullRect& a, const CullRect& b) {
  return a.Rect() == b.Rect();
}
inline bool operator!=(const CullRect& a, const CullRect& b) {
  return !(a == b);
}

inline std::ostream& operator<<(std::ostream& os, const CullRect& cull_rect) {
  return os << cull_rect.Rect();
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_CULL_RECT_H_
