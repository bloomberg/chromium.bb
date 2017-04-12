// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGImagePainter_h
#define SVGImagePainter_h

#include "platform/geometry/FloatSize.h"
#include "platform/wtf/Allocator.h"

namespace blink {

struct PaintInfo;
class LayoutSVGImage;

class SVGImagePainter {
  STACK_ALLOCATED();

 public:
  SVGImagePainter(const LayoutSVGImage& layout_svg_image)
      : layout_svg_image_(layout_svg_image) {}

  void Paint(const PaintInfo&);

 private:
  // Assumes the PaintInfo context has had all local transforms applied.
  void PaintForeground(const PaintInfo&);
  FloatSize ComputeImageViewportSize() const;

  const LayoutSVGImage& layout_svg_image_;
};

}  // namespace blink

#endif  // SVGImagePainter_h
