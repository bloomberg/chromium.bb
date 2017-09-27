// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextPainterBase_h
#define TextPainterBase_h

#include "core/CoreExport.h"
#include "core/paint/DecorationInfo.h"
#include "core/paint/TextPaintStyle.h"
#include "core/style/AppliedTextDecoration.h"
#include "platform/fonts/Font.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/Color.h"
#include "platform/transforms/AffineTransform.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class ComputedStyle;
class Document;
class GraphicsContext;
class GraphicsContextStateSaver;
class TextDecorationOffsetBase;
struct PaintInfo;

// Base class for text painting. Has no dependencies on the layout tree and thus
// provides functionality and definitions that can be shared between both legacy
// layout and LayoutNG.
class CORE_EXPORT TextPainterBase {
  STACK_ALLOCATED();

 public:
  TextPainterBase(GraphicsContext&,
                  const Font&,
                  const LayoutPoint& text_origin,
                  const LayoutRect& text_bounds,
                  bool horizontal);
  ~TextPainterBase();

  virtual void ClipDecorationsStripe(float upper,
                                     float stripe_width,
                                     float dilation) = 0;

  void SetEmphasisMark(const AtomicString&, TextEmphasisPosition);
  void SetEllipsisOffset(int offset) { ellipsis_offset_ = offset; }

  static void UpdateGraphicsContext(GraphicsContext&,
                                    const TextPaintStyle&,
                                    bool horizontal,
                                    GraphicsContextStateSaver&);

  void PaintDecorationsExceptLineThrough(const TextDecorationOffsetBase&,
                                         const DecorationInfo&,
                                         const PaintInfo&,
                                         const Vector<AppliedTextDecoration>&,
                                         const TextPaintStyle& text_style,
                                         bool* has_line_through_decoration);
  void PaintDecorationsOnlyLineThrough(const DecorationInfo&,
                                       const PaintInfo&,
                                       const Vector<AppliedTextDecoration>&,
                                       const TextPaintStyle&);
  void PaintDecorationUnderOrOverLine(GraphicsContext&,
                                      const DecorationInfo&,
                                      const AppliedTextDecoration&,
                                      int line_offset,
                                      float decoration_offset);

  void ComputeDecorationInfo(DecorationInfo&,
                             const LayoutPoint& box_origin,
                             LayoutPoint local_origin,
                             LayoutUnit width,
                             FontBaseline,
                             const ComputedStyle&,
                             const ComputedStyle* decorating_box_style);

  static Color TextColorForWhiteBackground(Color);
  static TextPaintStyle TextPaintingStyle(const Document&,
                                          const ComputedStyle&,
                                          const PaintInfo&);
  static TextPaintStyle SelectionPaintingStyle(
      const Document&,
      const ComputedStyle&,
      Node*,
      bool have_selection,
      const PaintInfo&,
      const TextPaintStyle& text_style);

  enum RotationDirection { kCounterclockwise, kClockwise };
  static AffineTransform Rotation(const LayoutRect& box_rect,
                                  RotationDirection);

 protected:
  void UpdateGraphicsContext(const TextPaintStyle& style,
                             GraphicsContextStateSaver& saver) {
    UpdateGraphicsContext(graphics_context_, style, horizontal_, saver);
  }
  void DecorationsStripeIntercepts(
      float upper,
      float stripe_width,
      float dilation,
      const Vector<Font::TextIntercept>& text_intercepts);

  enum PaintInternalStep { kPaintText, kPaintEmphasisMark };

  GraphicsContext& graphics_context_;
  const Font& font_;
  LayoutPoint text_origin_;
  LayoutRect text_bounds_;
  bool horizontal_;
  bool has_combined_text_;
  AtomicString emphasis_mark_;
  int emphasis_mark_offset_;
  int ellipsis_offset_;
};

inline AffineTransform TextPainterBase::Rotation(
    const LayoutRect& box_rect,
    RotationDirection rotation_direction) {
  // Why this matrix is correct: consider the case of a clockwise rotation.

  // Let the corner points that define |boxRect| be ABCD, where A is top-left
  // and B is bottom-left.

  // 1. We want B to end up at the same pixel position after rotation as A is
  //    before rotation.
  // 2. Before rotation, B is at (x(), maxY())
  // 3. Rotating clockwise by 90 degrees places B at the coordinates
  //    (-maxY(), x()).
  // 4. Point A before rotation is at (x(), y())
  // 5. Therefore the translation from (3) to (4) is (x(), y()) - (-maxY(), x())
  //    = (x() + maxY(), y() - x())

  // A similar argument derives the counter-clockwise case.
  return rotation_direction == kClockwise
             ? AffineTransform(0, 1, -1, 0, box_rect.X() + box_rect.MaxY(),
                               box_rect.Y() - box_rect.X())
             : AffineTransform(0, -1, 1, 0, box_rect.X() - box_rect.Y(),
                               box_rect.X() + box_rect.MaxY());
}

}  // namespace blink

#endif  // TextPainterBase_h
