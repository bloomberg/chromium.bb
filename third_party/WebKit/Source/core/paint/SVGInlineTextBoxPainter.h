// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGInlineTextBoxPainter_h
#define SVGInlineTextBoxPainter_h

#include "core/rendering/style/RenderStyleConstants.h"
#include "core/rendering/svg/RenderSVGResource.h"

namespace blink {

class FloatPoint;
class Font;
class GraphicsContext;
struct PaintInfo;
class LayoutPoint;
class RenderStyle;
class RenderObject;
class SVGInlineTextBox;
struct SVGTextFragment;
class TextRun;
class DocumentMarker;

class SVGInlineTextBoxPainter {
public:
    SVGInlineTextBoxPainter(SVGInlineTextBox& svgInlineTextBox) : m_svgInlineTextBox(svgInlineTextBox) { }
    void paint(PaintInfo&, const LayoutPoint&);
    void paintSelectionBackground(PaintInfo&);
    virtual void paintTextMatchMarker(GraphicsContext*, const FloatPoint&, DocumentMarker*, RenderStyle*, const Font&);

private:
    void paintDecoration(GraphicsContext*, TextDecoration, const SVGTextFragment&);
    void paintTextWithShadows(GraphicsContext*, RenderStyle*, TextRun&, const SVGTextFragment&, int startPosition, int endPosition, RenderSVGResourceMode);
    void paintText(GraphicsContext*, RenderStyle*, RenderStyle* selectionStyle, const SVGTextFragment&, RenderSVGResourceMode, bool hasSelection, bool paintSelectedTextOnly);

    SVGInlineTextBox& m_svgInlineTextBox;
};

} // namespace blink

#endif // SVGInlineTextBoxPainter_h
