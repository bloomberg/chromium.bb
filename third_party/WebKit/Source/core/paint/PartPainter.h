// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PartPainter_h
#define PartPainter_h

namespace blink {

struct PaintInfo;
class LayoutPoint;
class RenderPart;

class PartPainter {
public:
    PartPainter(RenderPart& renderPart) : m_renderPart(renderPart) { }

    void paint(PaintInfo&, const LayoutPoint&);
    void paintContents(PaintInfo&, const LayoutPoint&);

private:
    RenderPart& m_renderPart;
};

} // namespace blink

#endif // PartPainter_h
