// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef StyleDifference_h
#define StyleDifference_h

#include "wtf/Assertions.h"

namespace WebCore {

class StyleDifference {
public:
    StyleDifference()
        : m_repaintType(NoRepaint)
        , m_layoutType(NoLayout)
        , m_transformChanged(false)
        , m_opacityChanged(false)
        , m_zIndexChanged(false)
        , m_filterChanged(false)
        , m_textOrColorChanged(false)
    { }

    bool hasDifference() const { return m_repaintType || m_layoutType || m_transformChanged || m_opacityChanged || m_zIndexChanged || m_filterChanged || m_textOrColorChanged; }

    bool needsRepaint() const { return m_repaintType != NoRepaint; }
    void clearNeedsRepaint() { m_repaintType = NoRepaint; }

    // The object just needs to be repainted.
    bool needsRepaintObject() const { return m_repaintType == RepaintObject; }
    void setNeedsRepaintObject()
    {
        ASSERT(!needsRepaintLayer());
        m_repaintType = RepaintObject;
    }

    // The layer and its descendant layers need to be repainted.
    bool needsRepaintLayer() const { return m_repaintType == RepaintLayer; }
    void setNeedsRepaintLayer() { m_repaintType = RepaintLayer; }

    bool needsLayout() const { return m_layoutType != NoLayout; }
    void clearNeedsLayout() { m_layoutType = NoLayout; }

    // The offset of this positioned object has been updated.
    bool needsPositionedMovementLayout() const { return m_layoutType == PositionedMovement; }
    void setNeedsPositionedMovementLayout()
    {
        ASSERT(!needsFullLayout());
        m_layoutType = PositionedMovement;
    }

    bool needsFullLayout() const { return m_layoutType == FullLayout; }
    void setNeedsFullLayout() { m_layoutType = FullLayout; }

    bool transformChanged() const { return m_transformChanged; }
    void setTransformChanged() { m_transformChanged = true; }

    bool opacityChanged() const { return m_opacityChanged; }
    void setOpacityChanged() { m_opacityChanged = true; }

    bool zIndexChanged() const { return m_zIndexChanged; }
    void setZIndexChanged() { m_zIndexChanged = true; }

    bool filterChanged() const { return m_filterChanged; }
    void setFilterChanged() { m_filterChanged = true; }

    bool textOrColorChanged() const { return m_textOrColorChanged; }
    void setTextOrColorChanged() { m_textOrColorChanged = true; }

private:
    enum RepaintType {
        NoRepaint = 0,
        RepaintObject,
        RepaintLayer
    };
    unsigned m_repaintType : 2;

    enum LayoutType {
        NoLayout = 0,
        PositionedMovement,
        FullLayout
    };
    unsigned m_layoutType : 2;

    unsigned m_transformChanged : 1;
    unsigned m_opacityChanged : 1;
    unsigned m_zIndexChanged : 1;
    unsigned m_filterChanged : 1;
    // The object needs to be repainted if it contains text or properties dependent on color (e.g., border or outline).
    unsigned m_textOrColorChanged : 1;
};

} // namespace WebCore

#endif // StyleDifference_h
