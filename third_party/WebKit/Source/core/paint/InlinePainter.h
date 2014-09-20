// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InlinePainter_h
#define InlinePainter_h

#include "core/rendering/style/RenderStyleConstants.h"

namespace blink {

class Color;
class GraphicsContext;
class LayoutPoint;
class LayoutRect;
struct PaintInfo;
class RenderInline;

class InlinePainter {
public:
    InlinePainter(RenderInline& renderInline) : m_renderInline(renderInline) { }

    void paint(PaintInfo&, const LayoutPoint& paintOffset);
    void paintOutline(PaintInfo&, const LayoutPoint& paintOffset);

private:
    void paintOutlineForLine(GraphicsContext*, const LayoutPoint&, const LayoutRect& prevLine, const LayoutRect& thisLine,
        const LayoutRect& nextLine, const Color);
    RenderInline& m_renderInline;
};

} // namespace blink

#endif // InlinePainter_h
