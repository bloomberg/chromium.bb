// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SVGMarkerPainter_h
#define SVGMarkerPainter_h

namespace blink {

struct PaintInfo;
struct MarkerPosition;
class RenderSVGResourceMarker;

class SVGMarkerPainter {
public:
    SVGMarkerPainter(RenderSVGResourceMarker& renderSVGMarker) : m_renderSVGMarker(renderSVGMarker) { }

    void paint(PaintInfo&, const MarkerPosition&, float);

private:
    RenderSVGResourceMarker& m_renderSVGMarker;
};

} // namespace blink

#endif // SVGMarkerPainter_h
