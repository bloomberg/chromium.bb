// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGTextPainter_h
#define SVGTextPainter_h

namespace blink {

struct PaintInfo;
class RenderSVGText;

class SVGTextPainter {
public:
    SVGTextPainter(RenderSVGText& renderSVGText) : m_renderSVGText(renderSVGText) { }
    void paint(PaintInfo&);

private:
    RenderSVGText& m_renderSVGText;
};

} // namespace blink

#endif // SVGTextPainter_h
