// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlockPainter_h
#define BlockPainter_h

namespace blink {

struct PaintInfo;
class InlineBox;
class LayoutPoint;
class LayoutRect;
class RenderFlexibleBox;
class RenderBlock;
class RenderBox;
class LayoutObject;

class BlockPainter {
public:
    BlockPainter(RenderBlock& block) : m_renderBlock(block) { }

    void paint(const PaintInfo&, const LayoutPoint& paintOffset);
    void paintObject(const PaintInfo&, const LayoutPoint&);
    void paintChildren(const PaintInfo&, const LayoutPoint&);
    void paintChild(RenderBox&, const PaintInfo&, const LayoutPoint&);
    void paintChildAsInlineBlock(RenderBox&, const PaintInfo&, const LayoutPoint&);
    void paintOverflowControlsIfNeeded(const PaintInfo&, const LayoutPoint&);

    // inline-block elements paint all phases atomically. This function ensures that. Certain other elements
    // (grid items, flex items) require this behavior as well, and this function exists as a helper for them.
    // It is expected that the caller will call this function independent of the value of paintInfo.phase.
    static void paintAsInlineBlock(LayoutObject&, const PaintInfo&, const LayoutPoint&);
    static void paintChildrenOfFlexibleBox(RenderFlexibleBox&, const PaintInfo&, const LayoutPoint& paintOffset);
    static void paintInlineBox(InlineBox&, const PaintInfo&, const LayoutPoint& paintOffset);

private:
    LayoutRect overflowRectForPaintRejection() const;
    bool hasCaret() const;
    void paintCarets(const PaintInfo&, const LayoutPoint&);
    void paintContents(const PaintInfo&, const LayoutPoint&);
    void paintColumnContents(const PaintInfo&, const LayoutPoint&, bool paintFloats = false);
    void paintColumnRules(const PaintInfo&, const LayoutPoint&);
    void paintSelection(const PaintInfo&, const LayoutPoint&);
    void paintContinuationOutlines(const PaintInfo&, const LayoutPoint&);

    RenderBlock& m_renderBlock;
};

} // namespace blink

#endif // BlockPainter_h
