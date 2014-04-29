// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleDifference_h
#define StyleDifference_h

namespace WebCore {

// This class represents the difference between two computed styles (RenderStyle).
// The difference can be of 3 types:
// - Layout difference
// - Repaint difference
// - Recompositing difference
class StyleDifference {
public:
    StyleDifference()
        : m_needsRecompositeLayer(false)
        , m_repaintType(NoRepaint)
        , m_layoutType(NoLayout) { }

    // The two styles are identical.
    bool hasNoChange() const { return !m_needsRecompositeLayer && !m_repaintType && !m_layoutType; }

    // The layer needs its position and transform updated. Implied by other repaint and layout flags.
    bool needsRecompositeLayer() const { return m_needsRecompositeLayer || needsRepaint() || needsLayout(); }
    void setNeedsRecompositeLayer() { m_needsRecompositeLayer = true; }

    bool needsRepaint() const { return m_repaintType != NoRepaint; }
    void clearNeedsRepaint() { m_repaintType = NoRepaint; }

    // The object just needs to be repainted.
    bool needsRepaintObjectOnly() const { return m_repaintType == RepaintObjectOnly; }
    void setNeedsRepaintObject()
    {
        if (!needsRepaintLayer())
            m_repaintType = RepaintObjectOnly;
    }

    // The layer and its descendant layers need to be repainted.
    bool needsRepaintLayer() const { return m_repaintType == RepaintLayer; }
    void setNeedsRepaintLayer() { m_repaintType = RepaintLayer; }

    bool needsLayout() const { return m_layoutType != NoLayout; }
    void clearNeedsLayout() { m_layoutType = NoLayout; }

    // The offset of this positioned object has been updated.
    bool needsPositionedMovementLayout() const { return m_layoutType & PositionedMovement; }
    void setNeedsPositionedMovementLayout()
    {
        if (!needsFullLayout())
            m_layoutType |= PositionedMovement;
        // FIXME: This is temporary to keep the StyleDifferenceLegacy behavior.
        m_repaintType = NoRepaint;
    }

    bool needsFullLayout() const { return m_layoutType == FullLayout; }
    void setNeedsFullLayout()
    {
        m_layoutType = FullLayout;
        // FIXME: This is temporary to keep the StyleDifferenceLegacy behavior.
        m_repaintType = NoRepaint;
    }

private:
    unsigned m_needsRecompositeLayer : 1;

    enum RepaintType {
        NoRepaint = 0,
        RepaintObjectOnly,
        RepaintLayer
    };
    unsigned m_repaintType : 2;

    enum LayoutType {
        NoLayout = 0,
        PositionedMovement = 1 << 0,
        FullLayout = 1 << 1
    };
    unsigned m_layoutType : 2;
};

} // namespace WebCore

#endif // StyleDifference_h
