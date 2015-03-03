// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGMaskPainter_h
#define SVGMaskPainter_h

#include "platform/geometry/FloatRect.h"
#include "platform/graphics/paint/DisplayItem.h"

namespace blink {

class GraphicsContext;
class LayoutObject;
class LayoutSVGResourceMasker;

class SVGMaskPainter {
public:
    SVGMaskPainter(LayoutSVGResourceMasker& mask) : m_mask(mask) { }

    bool prepareEffect(LayoutObject*, GraphicsContext*);
    void finishEffect(LayoutObject*, GraphicsContext*);

private:
    void drawMaskForRenderer(GraphicsContext*, DisplayItemClient, const FloatRect& targetBoundingBox);

    LayoutSVGResourceMasker& m_mask;
};

} // namespace blink

#endif // SVGShapePainter_h
