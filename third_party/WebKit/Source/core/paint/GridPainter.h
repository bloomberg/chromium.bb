// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GridPainter_h
#define GridPainter_h

namespace blink {

struct PaintInfo;
class LayoutPoint;
class RenderBox;
class RenderGrid;

class GridPainter {
public:
    GridPainter(RenderGrid& renderGrid) : m_renderGrid(renderGrid) { }

    void paintChildren(PaintInfo&, const LayoutPoint&);
    void paintChild(RenderBox*, PaintInfo&, const LayoutPoint&);

    void paint(PaintInfo&, const LayoutPoint& paintOffset);

private:
    RenderGrid& m_renderGrid;
};

} // namespace blink

#endif // GridPainter_h
