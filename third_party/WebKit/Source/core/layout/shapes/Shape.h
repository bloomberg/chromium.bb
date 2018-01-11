/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Shape_h
#define Shape_h

#include "core/CoreExport.h"
#include "core/style/BasicShapes.h"
#include "core/style/StyleImage.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/Path.h"
#include "platform/text/WritingMode.h"
#include <memory>

namespace blink {

class FloatRoundedRect;

struct LineSegment {
  STACK_ALLOCATED();
  LineSegment() : logical_left(0), logical_right(0), is_valid(false) {}

  LineSegment(float logical_left, float logical_right)
      : logical_left(logical_left),
        logical_right(logical_right),
        is_valid(true) {}

  float logical_left;
  float logical_right;
  bool is_valid;
};

// A representation of a BasicShape that enables layout code to determine how to
// break a line up into segments that will fit within or around a shape. The
// line is defined by a pair of logical Y coordinates and the computed segments
// are returned as pairs of logical X coordinates. The BasicShape itself is
// defined in physical coordinates.

class CORE_EXPORT Shape {
  USING_FAST_MALLOC(Shape);

 public:
  struct DisplayPaths {
    STACK_ALLOCATED();
    Path shape;
    Path margin_shape;
  };
  static std::unique_ptr<Shape> CreateShape(const BasicShape*,
                                            const LayoutSize& logical_box_size,
                                            WritingMode,
                                            float margin);
  static std::unique_ptr<Shape> CreateRasterShape(Image*,
                                                  float threshold,
                                                  const LayoutRect& image_rect,
                                                  const LayoutRect& margin_rect,
                                                  WritingMode,
                                                  float margin);
  static std::unique_ptr<Shape> CreateLayoutBoxShape(const FloatRoundedRect&,
                                                     WritingMode,
                                                     float margin);

  virtual ~Shape() = default;

  virtual LayoutRect ShapeMarginLogicalBoundingBox() const = 0;
  virtual bool IsEmpty() const = 0;
  virtual LineSegment GetExcludedInterval(LayoutUnit logical_top,
                                          LayoutUnit logical_height) const = 0;

  bool LineOverlapsShapeMarginBounds(LayoutUnit line_top,
                                     LayoutUnit line_height) const {
    return LineOverlapsBoundingBox(line_top, line_height,
                                   ShapeMarginLogicalBoundingBox());
  }
  virtual void BuildDisplayPaths(DisplayPaths&) const = 0;

 protected:
  float ShapeMargin() const { return margin_; }

 private:
  static std::unique_ptr<Shape> CreateEmptyRasterShape(WritingMode,
                                                       float margin);

  bool LineOverlapsBoundingBox(LayoutUnit line_top,
                               LayoutUnit line_height,
                               const LayoutRect& rect) const {
    if (rect.IsEmpty())
      return false;
    return (line_top < rect.MaxY() && line_top + line_height > rect.Y()) ||
           (!line_height && line_top == rect.Y());
  }

  WritingMode writing_mode_;
  float margin_;
};

}  // namespace blink

#endif  // Shape_h
