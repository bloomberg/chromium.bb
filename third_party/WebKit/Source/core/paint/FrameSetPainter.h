// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef FrameSetPainter_h
#define FrameSetPainter_h

namespace blink {

class IntRect;
struct PaintInfo;
class LayoutPoint;
class RenderFrameSet;

class FrameSetPainter {
public:
    FrameSetPainter(RenderFrameSet& renderFrameSet) : m_renderFrameSet(renderFrameSet) { }

    void paint(const PaintInfo&, const LayoutPoint& paintOffset);

private:
    void paintRowBorder(const PaintInfo&, const IntRect&);
    void paintColumnBorder(const PaintInfo&, const IntRect&);

    RenderFrameSet& m_renderFrameSet;
};

} // namespace blink

#endif // FrameSetPainter_h
