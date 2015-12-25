// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintInvalidationState_h
#define PaintInvalidationState_h

#include "platform/geometry/LayoutRect.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

namespace blink {

class LayoutBoxModelObject;
class LayoutObject;
class LayoutSVGModelObject;
class LayoutView;

class PaintInvalidationState {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    WTF_MAKE_NONCOPYABLE(PaintInvalidationState);
public:
    PaintInvalidationState(PaintInvalidationState& next, LayoutBoxModelObject& layoutObject, const LayoutBoxModelObject& paintInvalidationContainer);
    PaintInvalidationState(PaintInvalidationState& next, const LayoutSVGModelObject& layoutObject);

    PaintInvalidationState(const LayoutView& layoutView, Vector<LayoutObject*>& pendingDelayedPaintInvalidations)
        : PaintInvalidationState(layoutView, pendingDelayedPaintInvalidations, nullptr) { }
    PaintInvalidationState(const LayoutView& layoutView, PaintInvalidationState& ownerPaintInvalidationState)
        : PaintInvalidationState(layoutView, ownerPaintInvalidationState.m_pendingDelayedPaintInvalidations, &ownerPaintInvalidationState) { }

    const LayoutRect& clipRect() const { return m_clipRect; }
    const LayoutSize& paintOffset() const { return m_paintOffset; }
    const AffineTransform& svgTransform() const { return m_svgTransform; }

    bool cachedOffsetsEnabled() const { return m_cachedOffsetsEnabled; }
    bool isClipped() const { return m_clipped; }

    bool forcedSubtreeInvalidationWithinContainer() const { return m_forcedSubtreeInvalidationWithinContainer; }
    void setForceSubtreeInvalidationWithinContainer() { m_forcedSubtreeInvalidationWithinContainer = true; }

    bool forcedSubtreeInvalidationRectUpdateWithinContainer() const { return m_forcedSubtreeInvalidationRectUpdateWithinContainer; }
    void setForceSubtreeInvalidationRectUpdateWithinContainer() { m_forcedSubtreeInvalidationRectUpdateWithinContainer = true; }

    const LayoutBoxModelObject& paintInvalidationContainer() const { return m_paintInvalidationContainer; }

    bool canMapToContainer(const LayoutBoxModelObject* container) const
    {
        return m_cachedOffsetsEnabled && container == &m_paintInvalidationContainer;
    }

    // Records |obj| as needing paint invalidation on the next frame. See the definition of PaintInvalidationDelayedFull for more details.
    void pushDelayedPaintInvalidationTarget(LayoutObject& obj) { m_pendingDelayedPaintInvalidations.append(&obj); }
    Vector<LayoutObject*>& pendingDelayedPaintInvalidationTargets() { return m_pendingDelayedPaintInvalidations; }

    // Disable view clipping and scroll offset adjustment for paint invalidation of FrameView scrollbars.
    // TODO(wangxianzhu): Remove this when root-layer-scrolls launches.
    bool viewClippingAndScrollOffsetDisabled() const { return m_viewClippingAndScrollOffsetDisabled; }
    void setViewClippingAndScrollOffsetDisabled(bool b) { m_viewClippingAndScrollOffsetDisabled = b; }

private:
    PaintInvalidationState(const LayoutView&, Vector<LayoutObject*>& pendingDelayedPaintInvalidations, PaintInvalidationState* ownerPaintInvalidationState);

    void applyClipIfNeeded(const LayoutObject&);
    void addClipRectRelativeToPaintOffset(const LayoutSize& clipSize);

    friend class ForceHorriblySlowRectMapping;

    bool m_clipped;
    mutable bool m_cachedOffsetsEnabled;
    bool m_forcedSubtreeInvalidationWithinContainer;
    bool m_forcedSubtreeInvalidationRectUpdateWithinContainer;
    bool m_viewClippingAndScrollOffsetDisabled;

    LayoutRect m_clipRect;

    // x/y offset from paint invalidation container. Includes relative positioning and scroll offsets.
    LayoutSize m_paintOffset;

    const LayoutBoxModelObject& m_paintInvalidationContainer;

    // Transform from the initial viewport coordinate system of an outermost
    // SVG root to the userspace _before_ the relevant element. Combining this
    // with |m_paintOffset| yields the "final" offset.
    AffineTransform m_svgTransform;

    Vector<LayoutObject*>& m_pendingDelayedPaintInvalidations;
};

} // namespace blink

#endif // PaintInvalidationState_h
