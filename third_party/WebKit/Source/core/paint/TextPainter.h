// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TextPainter_h
#define TextPainter_h

#include "core/CoreExport.h"
#include "core/paint/TextPainterBase.h"

namespace blink {

class TextRun;
struct TextRunPaintInfo;
class LayoutTextCombine;
class LineLayoutItem;

// Text painter for legacy layout. Operates on TextRuns.
class CORE_EXPORT TextPainter : public TextPainterBase {
  STACK_ALLOCATED();

 public:
  TextPainter(GraphicsContext& context,
              const Font& font,
              const TextRun& run,
              const LayoutPoint& text_origin,
              const LayoutRect& text_bounds,
              bool horizontal)
      : TextPainterBase(context, font, text_origin, text_bounds, horizontal),
        run_(run),
        combined_text_(0) {}
  ~TextPainter() {}

  void SetCombinedText(LayoutTextCombine* combined_text) {
    combined_text_ = combined_text;
  }

  void ClipDecorationsStripe(float upper,
                             float stripe_width,
                             float dilation) override;
  void Paint(unsigned start_offset,
             unsigned end_offset,
             unsigned length,
             const Style&);

  static Style SelectionPaintingStyle(LineLayoutItem,
                                      bool have_selection,
                                      const PaintInfo&,
                                      const Style& text_style);

 private:
  template <PaintInternalStep step>
  void PaintInternalRun(TextRunPaintInfo&, unsigned from, unsigned to);

  template <PaintInternalStep step>
  void PaintInternal(unsigned start_offset,
                     unsigned end_offset,
                     unsigned truncation_point);

  void PaintEmphasisMarkForCombinedText();

  const TextRun& run_;
  LayoutTextCombine* combined_text_;
};

}  // namespace blink

#endif  // TextPainter_h
