// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BackgroundImageGeometry_h
#define BackgroundImageGeometry_h

#include "core/paint/PaintPhase.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/geometry/LayoutSize.h"
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
        const GlobalPaintFlags, const FillLayer&, const LayoutRect& paintRect);

    LayoutRect destRect() const { return m_destRect; }
    LayoutSize tileSize() const { return m_tileSize; }
    LayoutSize imageContainerSize() const { return m_imageContainerSize; }
    LayoutPoint phase() const { return m_phase; }
    // Space-size represents extra width and height that may be added to
    // the image if used as a pattern with background-repeat: space.
    LayoutSize spaceSize() const { return m_repeatSpacing; }
    // Has background-attachment: fixed. Implies that we can't always cheaply compute destRect.
    bool hasNonLocalGeometry() const { return m_hasNonLocalGeometry; }

private:
    void setDestRect(const LayoutRect& destRect) { m_destRect = destRect; }
    void setPhase(const LayoutPoint& phase) { m_phase = phase; }
    void setImageContainerSize(const LayoutSize& imageContainerSize) { m_imageContainerSize = imageContainerSize; }
    void setTileSize(const LayoutSize& tileSize) { m_tileSize = tileSize; }
    void setSpaceSize(const LayoutSize& repeatSpacing) { m_repeatSpacing = repeatSpacing; }
    void setPhaseX(LayoutUnit x) { m_phase.setX(x); }
    void setPhaseY(LayoutUnit y) { m_phase.setY(y); }
    void setNoRepeatX(LayoutUnit xOffset);
    void setNoRepeatY(LayoutUnit yOffset);

    void pixelSnapGeometry();

    void useFixedAttachment(const LayoutPoint& attachmentPoint);
    void clip(const LayoutRect&);
    void setHasNonLocalGeometry() { m_hasNonLocalGeometry = true; }

    LayoutRect m_destRect;
    LayoutPoint m_phase;
    LayoutSize m_imageContainerSize;
    LayoutSize m_tileSize;
    LayoutSize m_repeatSpacing;
    bool m_hasNonLocalGeometry;
};

} // namespace blink

#endif // BackgroundImageGeometry_h
