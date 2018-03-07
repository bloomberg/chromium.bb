// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InlineTextBoxPainter_h
#define InlineTextBoxPainter_h

#include "core/editing/markers/DocumentMarker.h"
#include "core/style/ComputedStyleConstants.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/wtf/Allocator.h"

namespace blink {

struct PaintInfo;

class Color;
class ComputedStyle;
class Font;
class GraphicsContext;
class InlineTextBox;
class LayoutObject;
class LayoutPoint;
class LayoutTextCombine;
class StyleableMarker;
class TextMatchMarker;

enum class DocumentMarkerPaintPhase { kForeground, kBackground };

class InlineTextBoxPainter {
  STACK_ALLOCATED();

 public:
  InlineTextBoxPainter(const InlineTextBox& inline_text_box)
      : inline_text_box_(inline_text_box) {}

  void Paint(const PaintInfo&, const LayoutPoint&);

  // We don't paint composition or spelling markers that overlap a suggestion
  // marker (to match the native Android behavior). This method lets us throw
  // out the overlapping composition and spelling markers in O(N log N) time
  // where N is the total number of DocumentMarkers in this node.
  DocumentMarkerVector ComputeMarkersToPaint() const;

  void PaintDocumentMarkers(const DocumentMarkerVector& markers_to_paint,
                            const PaintInfo&,
                            const LayoutPoint& box_origin,
                            const ComputedStyle&,
                            const Font&,
                            DocumentMarkerPaintPhase);
  void PaintDocumentMarker(GraphicsContext&,
                           const LayoutPoint& box_origin,
                           const DocumentMarker&,
                           const ComputedStyle&,
                           const Font&,
                           bool grammar);
  void PaintTextMatchMarkerForeground(const PaintInfo&,
                                      const LayoutPoint& box_origin,
                                      const TextMatchMarker&,
                                      const ComputedStyle&,
                                      const Font&);
  void PaintTextMatchMarkerBackground(const PaintInfo&,
                                      const LayoutPoint& box_origin,
                                      const TextMatchMarker&,
                                      const ComputedStyle&,
                                      const Font&);

  static bool PaintsMarkerHighlights(const LayoutObject&);

 private:
  enum class PaintOptions { kNormal, kCombinedText };

  void PaintSingleMarkerBackgroundRun(GraphicsContext&,
                                      const LayoutPoint& box_origin,
                                      const ComputedStyle&,
                                      const Font&,
                                      Color background_color,
                                      int start_pos,
                                      int end_pos);
  template <PaintOptions>
  void PaintSelection(GraphicsContext&,
                      const LayoutRect& box_rect,
                      const ComputedStyle&,
                      const Font&,
                      Color text_color,
                      LayoutTextCombine* = nullptr);

  void PaintStyleableMarkerUnderline(GraphicsContext&,
                                     const LayoutPoint& box_origin,
                                     const StyleableMarker&,
                                     const ComputedStyle&,
                                     const Font&);
  struct PaintOffsets {
    unsigned start;
    unsigned end;
  };
  PaintOffsets ApplyTruncationToPaintOffsets(const PaintOffsets&);
  // For markers that shouldn't draw over a truncation ellipsis (i.e., not
  // text match markers, which do draw over said ellipsis)
  PaintOffsets MarkerPaintStartAndEnd(const DocumentMarker&);

  bool ShouldPaintTextBox(const PaintInfo&);
  void ExpandToIncludeNewlineForSelection(LayoutRect&);
  LayoutObject& InlineLayoutObject() const;

  const InlineTextBox& inline_text_box_;
};

}  // namespace blink

#endif  // InlineTextBoxPainter_h
