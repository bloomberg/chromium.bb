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
class LayoutStyle;

class EllipsisBoxPainter {
public:
    EllipsisBoxPainter(EllipsisBox& ellipsisBox) : m_ellipsisBox(ellipsisBox) { }

    void paint(const PaintInfo&, const LayoutPoint&, LayoutUnit lineTop, LayoutUnit lineBottom);

private:
    void paintEllipsis(const PaintInfo&, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom, const LayoutStyle&);
    void paintMarkupBox(const PaintInfo&, const LayoutPoint& paintOffset, LayoutUnit lineTop, LayoutUnit lineBottom, const LayoutStyle&);
    void paintSelection(GraphicsContext*, const FloatPoint&, const LayoutStyle&, const Font&);

    EllipsisBox& m_ellipsisBox;
};

} // namespace blink

#endif // EllipsisBoxPainter_h
