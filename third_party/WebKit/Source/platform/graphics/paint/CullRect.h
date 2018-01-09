// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CullRect_h
#define CullRect_h

#include "platform/geometry/IntRect.h"
#include "platform/transforms/AffineTransform.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/WTFString.h"

#include <limits>

namespace blink {

class FloatRect;
class LayoutRect;
class LayoutUnit;

class PLATFORM_EXPORT CullRect {
  DISALLOW_NEW();

 public:
  CullRect() = default;
  explicit CullRect(const IntRect& rect) : rect_(rect) {}
  CullRect(const CullRect&, const IntPoint& offset);
  CullRect(const CullRect&, const IntSize& offset);

  bool IntersectsCullRect(const AffineTransform&,
                          const FloatRect& bounding_box) const;
  bool IntersectsCullRect(const IntRect&) const;
  bool IntersectsCullRect(const LayoutRect&) const;
  bool IntersectsHorizontalRange(LayoutUnit lo, LayoutUnit hi) const;
  bool IntersectsVerticalRange(LayoutUnit lo, LayoutUnit hi) const;

  void UpdateCullRect(const AffineTransform& local_to_parent_transform);

  // |overflow_clip_rect| should be in the same coordinate space as |rect_|.
  void UpdateForScrollingContents(
      const IntRect& overflow_clip_rect,
      const AffineTransform& local_to_parent_transform);

  String ToString() const { return rect_.ToString(); }

 private:
  IntRect rect_;

  friend bool operator==(const CullRect&, const CullRect&);

  friend class CullRectTest;

  // TODO(chrishtr): temporary while we implement CullRect everywhere.
  friend class FramePainter;
  friend class GridPainter;
  friend class SVGInlineTextBoxPainter;
  friend class SVGPaintContext;
  friend class SVGRootInlineBoxPainter;
  friend class SVGShapePainter;
  friend class TableRowPainter;
  friend class TableSectionPainter;
  friend class ThemePainterMac;
  friend class WebPluginContainerImpl;
};

inline bool operator==(const CullRect& a, const CullRect& b) {
  return a.rect_ == b.rect_;
}
inline bool operator!=(const CullRect& a, const CullRect& b) {
  return !(a == b);
}

}  // namespace blink

#endif  // CullRect_h
