// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollbarPainter_h
#define ScrollbarPainter_h

#include "platform/scroll/Scrollbar.h"

namespace blink {

class GraphicsContext;
struct PaintInfo;
class IntRect;
class LayoutPoint;
class LayoutRect;
class RenderScrollbar;
class RenderScrollbarPart;

class ScrollbarPainter {
public:
    ScrollbarPainter(RenderScrollbar& renderScrollbar) : m_renderScrollbar(renderScrollbar) { }

    void paintPart(GraphicsContext*, ScrollbarPart, const IntRect&);
    static void paintIntoRect(RenderScrollbarPart*, GraphicsContext*, const LayoutPoint& paintOffset, const LayoutRect&);

private:
    RenderScrollbar& m_renderScrollbar;
};

} // namespace blink

#endif // ScrollbarPainter_h
