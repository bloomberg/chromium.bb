// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundImageGeometry_h
#define BackgroundImageGeometry_h

#include "core/paint/PaintPhase.h"
#include "platform/geometry/IntPoint.h"
#include "platform/geometry/IntRect.h"
#include "platform/geometry/IntSize.h"
#include "wtf/Allocator.h"

namespace blink {

class FillLayer;
class LayoutBoxModelObject;
class LayoutObject;
class LayoutRect;

class BackgroundImageGeometry {
    STACK_ALLOCATED();
public:
    BackgroundImageGeometry()
        : m_hasNonLocalGeometry(false)
    { }

    void calculate(const LayoutBoxModelObject&, const LayoutBoxModelObject* paintContainer,
        const GlobalPaintFlags, const FillLayer&, const LayoutRect& paintRect,
        const LayoutObject* backgroundObject = nullptr);

    IntRect destRect() const { return m_destRect; }
    IntSize tileSize() const { return m_tileSize; }
    IntPoint phase() const { return m_phase; }
    // Space-size represents extra width and height that may be added to
    // the image if used as a pattern with background-repeat: space.
    IntSize spaceSize() const { return m_repeatSpacing; }
    // Has background-attachment: fixed. Implies that we can't always cheaply compute destRect.
    bool hasNonLocalGeometry() const { return m_hasNonLocalGeometry; }

private:
    void setDestRect(const IntRect& destRect) { m_destRect = destRect; }
    void setPhase(const IntPoint& phase) { m_phase = phase; }
    void setTileSize(const IntSize& tileSize) { m_tileSize = tileSize; }
    void setSpaceSize(const IntSize& repeatSpacing) { m_repeatSpacing = repeatSpacing; }
    void setPhaseX(int x) { m_phase.setX(x); }
    void setPhaseY(int y) { m_phase.setY(y); }
    void setNoRepeatX(int xOffset);
    void setNoRepeatY(int yOffset);

    void useFixedAttachment(const IntPoint& attachmentPoint);
    void clip(const IntRect&);
    void setHasNonLocalGeometry() { m_hasNonLocalGeometry = true; }

    IntRect m_destRect;
    IntPoint m_phase;
    IntSize m_tileSize;
    IntSize m_repeatSpacing;
    bool m_hasNonLocalGeometry;
};

} // namespace blink

#endif // BackgroundImageGeometry_h
