// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InlineFlowBoxPainter_h
#define InlineFlowBoxPainter_h

#include "core/rendering/style/ShadowData.h"
#include "platform/graphics/GraphicsTypes.h"

namespace blink {

class Color;
class FillLayer;
class InlineFlowBox;
class LayoutPoint;
class LayoutRect;
class LayoutUnit;
struct PaintInfo;
class RenderStyle;

class InlineFlowBoxPainter {
public:
    InlineFlowBoxPainter(InlineFlowBox& inlineFlowBox) : m_inlineFlowBox(inlineFlowBox) { }
    void paint(PaintInfo&, const LayoutPoint&, const LayoutUnit lineTop, const LayoutUnit lineBottom);

private:
    void paintBoxDecorationBackground(PaintInfo&, const LayoutPoint&);
    void paintMask(PaintInfo&, const LayoutPoint&);
    void paintFillLayers(const PaintInfo&, const Color&, const FillLayer&, const LayoutRect&, CompositeOperator = CompositeSourceOver);
    void paintFillLayer(const PaintInfo&, const Color&, const FillLayer&, const LayoutRect&, CompositeOperator);
    void paintBoxShadow(const PaintInfo&, RenderStyle*, ShadowStyle, const LayoutRect&);
    LayoutRect roundedFrameRectClampedToLineTopAndBottomIfNeeded() const;

    InlineFlowBox& m_inlineFlowBox;
};

} // namespace blink

#endif // InlineFlowBoxPainter_h
