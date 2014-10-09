// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGContainerPainter_h
#define SVGContainerPainter_h

namespace blink {

struct PaintInfo;
class RenderSVGContainer;

class SVGContainerPainter {
public:
    SVGContainerPainter(RenderSVGContainer& renderSVGContainer) : m_renderSVGContainer(renderSVGContainer) { }

    void paint(PaintInfo&);

private:
    RenderSVGContainer& m_renderSVGContainer;
};

} // namespace blink

#endif // SVGContainerPainter_h
