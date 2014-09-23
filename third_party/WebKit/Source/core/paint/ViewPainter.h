// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ViewPainter_h
#define ViewPainter_h

namespace blink {

class LayoutPoint;
struct PaintInfo;
class RenderBox;
class RenderView;

class ViewPainter {
public:
    ViewPainter(RenderView& renderView) : m_renderView(renderView) { }

    void paint(PaintInfo&, const LayoutPoint& paintOffset);
    void paintBoxDecorationBackground(PaintInfo&);

private:
    bool rootFillsViewportBackground(RenderBox* rootBox) const;

    RenderView& m_renderView;
};

} // namespace blink

#endif // ViewPainter_h
