// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlockPainter_h
#define BlockPainter_h

#include "core/rendering/RenderBlock.h"

namespace blink {

struct PaintInfo;
class LayoutPoint;

class BlockPainter {
public:
    BlockPainter(RenderBlock& block) : m_renderBlock(block) { }

    void paint(PaintInfo&, const LayoutPoint& paintOffset);
    void paintObject(PaintInfo&, const LayoutPoint&);
    void paintChildren(PaintInfo&, const LayoutPoint&);
    void paintChildAsInlineBlock(RenderBox*, PaintInfo&, const LayoutPoint&);

    // inline-block elements paint all phases atomically. This function ensures that. Certain other elements
    // (grid items, flex items) require this behavior as well, and this function exists as a helper for them.
    // It is expected that the caller will call this function independent of the value of paintInfo.phase.
    static void paintAsInlineBlock(RenderObject*, PaintInfo&, const LayoutPoint&);

private:
    LayoutRect overflowRectForPaintRejection() const;
    bool hasCaret() const;
    void paintCarets(PaintInfo&, const LayoutPoint&);
    void paintContents(PaintInfo&, const LayoutPoint&);
    void paintColumnContents(PaintInfo&, const LayoutPoint&, bool paintFloats = false);
    void paintColumnRules(PaintInfo&, const LayoutPoint&);
    void paintChild(RenderBox*, PaintInfo&, const LayoutPoint&);
    void paintSelection(PaintInfo&, const LayoutPoint&);
    void paintContinuationOutlines(PaintInfo&, const LayoutPoint&);

    RenderBlock& m_renderBlock;
};

} // namespace blink

#endif // BlockPainter_h
