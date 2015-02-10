// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGInlineTextBoxPainter_h
#define SVGInlineTextBoxPainter_h

#include "core/layout/style/LayoutStyleConstants.h"
#include "core/layout/svg/LayoutSVGResourcePaintServer.h"

namespace blink {

class FloatPoint;
class Font;
class GraphicsContext;
struct PaintInfo;
class LayoutPoint;
class LayoutStyle;
class SVGInlineTextBox;
struct SVGTextFragment;
class TextRun;
class DocumentMarker;

class SVGInlineTextBoxPainter {
public:
    SVGInlineTextBoxPainter(SVGInlineTextBox& svgInlineTextBox) : m_svgInlineTextBox(svgInlineTextBox) { }
    void paint(const PaintInfo&, const LayoutPoint&);
    void paintSelectionBackground(const PaintInfo&);
    virtual void paintTextMatchMarker(GraphicsContext*, const FloatPoint&, DocumentMarker*, const LayoutStyle&, const Font&);

private:
    bool shouldPaintSelection() const;
    void paintTextFragments(const PaintInfo&, LayoutObject&);
    void paintDecoration(const PaintInfo&, TextDecoration, const SVGTextFragment&);
    void paintTextWithShadows(const PaintInfo&, const LayoutStyle&, TextRun&, const SVGTextFragment&, int startPosition, int endPosition, LayoutSVGResourceMode);
    void paintText(const PaintInfo&, const LayoutStyle&, const LayoutStyle& selectionStyle, const SVGTextFragment&, LayoutSVGResourceMode, bool shouldPaintSelection);

    SVGInlineTextBox& m_svgInlineTextBox;
};

} // namespace blink

#endif // SVGInlineTextBoxPainter_h
