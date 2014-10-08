// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EllipsisBoxPainter_h
#define EllipsisBoxPainter_h

namespace blink {

struct PaintInfo;

class EllipsisBox;
class FloatPoint;
class Font;
class GraphicsContext;
class LayoutPoint;
class LayoutUnit;
class RenderStyle;

class EllipsisBoxPainter {
public:
    EllipsisBoxPainter(EllipsisBox& ellipsisBox) : m_ellipsisBox(ellipsisBox) { }

    void paint(PaintInfo&, const LayoutPoint&, LayoutUnit lineTop, LayoutUnit lineBottom);

private:
    void paintMarkupBox(PaintInfo&, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom, RenderStyle*);
    void paintSelection(GraphicsContext*, const FloatPoint&, RenderStyle*, const Font&);

    EllipsisBox& m_ellipsisBox;
};

} // namespace blink

#endif // EllipsisBoxPainter_h
