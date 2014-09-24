// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ListMarkerPainter_h
#define ListMarkerPainter_h

namespace blink {

struct PaintInfo;
class Path;
class LayoutPoint;
class RenderListMarker;

class ListMarkerPainter {
public:
    ListMarkerPainter(RenderListMarker& renderListMarker) : m_renderListMarker(renderListMarker) { }

    void paint(PaintInfo&, const LayoutPoint& paintOffset);

private:
    Path getCanonicalPath() const;
    Path getPath(const LayoutPoint& origin) const;

    RenderListMarker& m_renderListMarker;
};

} // namespace blink

#endif // ListMarkerPainter_h
