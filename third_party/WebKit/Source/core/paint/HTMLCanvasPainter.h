// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLCanvasPainter_h
#define HTMLCanvasPainter_h

namespace blink {

struct PaintInfo;
class LayoutPoint;
class RenderHTMLCanvas;

class HTMLCanvasPainter {
public:
    HTMLCanvasPainter(RenderHTMLCanvas& renderHTMLCanvas) : m_renderHTMLCanvas(renderHTMLCanvas) { }
    void paintReplaced(PaintInfo&, const LayoutPoint&);

private:
    RenderHTMLCanvas& m_renderHTMLCanvas;
};

} // namespace blink

#endif // HTMLCanvasPainter_h
