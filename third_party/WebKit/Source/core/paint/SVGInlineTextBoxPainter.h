// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGInlineTextBoxPainter_h
#define SVGInlineTextBoxPainter_h

#include "core/style/ComputedStyleConstants.h"
#include "core/layout/svg/LayoutSVGResourcePaintServer.h"
#include "wtf/Allocator.h"

namespace blink {

class FloatPoint;
class Font;
class GraphicsContext;
struct PaintInfo;
class LayoutPoint;
class ComputedStyle;
class SVGInlineTextBox;
struct SVGTextFragment;
class TextRun;
class DocumentMarker;

class SVGInlineTextBoxPainter {
    STACK_ALLOCATED();
public:
    SVGInlineTextBoxPainter(const SVGInlineTextBox& svgInlineTextBox) : m_svgInlineTextBox(svgInlineTextBox) { }
    void paint(const PaintInfo&, const LayoutPoint&);
    void paintSelectionBackground(const PaintInfo&);
    virtual void paintTextMatchMarker(GraphicsContext*, const LayoutPoint&, DocumentMarker*, const ComputedStyle&, const Font&);

private:
    bool shouldPaintSelection(const PaintInfo&) const;
    void paintTextFragments(const PaintInfo&, LayoutObject&);
    void paintDecoration(const PaintInfo&, TextDecoration, const SVGTextFragment&);
    void paintTextWithShadows(const PaintInfo&, const ComputedStyle&, TextRun&, const SVGTextFragment&, int startPosition, int endPosition, LayoutSVGResourceMode);
    void paintText(const PaintInfo&, const ComputedStyle&, const ComputedStyle& selectionStyle, const SVGTextFragment&, LayoutSVGResourceMode, bool shouldPaintSelection);

    const SVGInlineTextBox& m_svgInlineTextBox;
};

} // namespace blink

#endif // SVGInlineTextBoxPainter_h
