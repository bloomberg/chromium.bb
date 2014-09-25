// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGImagePainter_h
#define SVGImagePainter_h

namespace blink {

struct PaintInfo;
class RenderSVGImage;

class SVGImagePainter {
public:
    SVGImagePainter(RenderSVGImage& renderSVGImage) : m_renderSVGImage(renderSVGImage) { }

    void paint(PaintInfo&);

    // Assumes the PaintInfo context has had all local transforms applied.
    static void paintForeground(RenderSVGImage&, PaintInfo&);

private:
    RenderSVGImage& m_renderSVGImage;
};

} // namespace blink

#endif // SVGImagePainter_h
