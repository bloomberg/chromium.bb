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
class PaintLayer;

// PaintInvalidationState is an optimization used during the paint
// invalidation phase.
//
// This class is extremely close to LayoutState so see the documentation
// of LayoutState for the class existence and performance benefits.
//
// The main difference with LayoutState is that it was customized for the
// needs of the paint invalidation systems (keeping visual rectangles
// instead of layout specific information).
class PaintInvalidationState {
    DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();
    WTF_MAKE_NONCOPYABLE(PaintInvalidationState);
public:
    PaintInvalidationState(const PaintInvalidationState& parentState, const LayoutObject&);

    // TODO(wangxianzhu): This is temporary for positioned object whose paintInvalidationContainer is different from
    // the one we find during tree walk. Remove this after we fix the issue with tree walk in DOM-order.
    PaintInvalidationState(const PaintInvalidationState& parentState, const LayoutBoxModelObject&, const LayoutBoxModelObject& paintInvalidationContainer);

    // For root LayoutView.
    PaintInvalidationState(const LayoutView& layoutView, Vector<LayoutObject*>& pendingDelayedPaintInvalidations)
        : PaintInvalidationState(layoutView, pendingDelayedPaintInvalidations, nullptr) { }
    // For LayoutView in a sub-frame.
    PaintInvalidationState(const LayoutView& layoutView, const PaintInvalidationState& ownerPaintInvalidationState)
        : PaintInvalidationState(layoutView, ownerPaintInvalidationState.m_pendingDelayedPaintInvalidations, &ownerPaintInvalidationState) { }

    // When a PaintInvalidationState is constructed, it just updates paintInvalidationContainer and
    // copy cached paintOffset and clip from the parent PaintInvalidationContainer.
    // This PaintInvalidationContainer can be used to invalidate the current object.
    //
    // After invalidation of the current object, before invalidation of the subtrees,
    // this method must be called to make this PaintInvalidationState suitable for
    // paint invalidation of children.
    void updatePaintOffsetAndClipForChildren();

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

    bool canMapToAncestor(const LayoutBoxModelObject* ancestor) const
    {
        return m_cachedOffsetsEnabled && ancestor == &m_paintInvalidationContainer;
    }
    void mapObjectRectToAncestor(const LayoutObject&, const LayoutBoxModelObject* ancestor, LayoutRect&) const;

    // Records |obj| as needing paint invalidation on the next frame. See the definition of PaintInvalidationDelayedFull for more details.
    void pushDelayedPaintInvalidationTarget(LayoutObject& obj) const { m_pendingDelayedPaintInvalidations.append(&obj); }
    Vector<LayoutObject*>& pendingDelayedPaintInvalidationTargets() const { return m_pendingDelayedPaintInvalidations; }

    // Disable view clipping and scroll offset adjustment for paint invalidation of FrameView scrollbars.
    // TODO(wangxianzhu): Remove this when root-layer-scrolls launches.
    bool viewClippingAndScrollOffsetDisabled() const { return m_viewClippingAndScrollOffsetDisabled; }
    void setViewClippingAndScrollOffsetDisabled(bool b) { m_viewClippingAndScrollOffsetDisabled = b; }

    PaintLayer& enclosingSelfPaintingLayer(const LayoutObject&) const;

private:
    PaintInvalidationState(const LayoutView&, Vector<LayoutObject*>& pendingDelayedPaintInvalidations, const PaintInvalidationState* ownerPaintInvalidationState);

    void applyClipIfNeeded();
    void addClipRectRelativeToPaintOffset(const LayoutSize& clipSize);

    friend class ForceHorriblySlowRectMapping;

    const LayoutObject& m_currentObject;

    bool m_clipped;
    mutable bool m_cachedOffsetsEnabled;
    bool m_forcedSubtreeInvalidationWithinContainer;
    bool m_forcedSubtreeInvalidationRectUpdateWithinContainer;
    bool m_viewClippingAndScrollOffsetDisabled;

    LayoutRect m_clipRect;

    // x/y offset from paint invalidation container. Includes relative positioning and scroll offsets.
    LayoutSize m_paintOffset;

    // The current paint invalidation container.
    //
    // It is the enclosing composited object.
    const LayoutBoxModelObject& m_paintInvalidationContainer;

    // Transform from the initial viewport coordinate system of an outermost
    // SVG root to the userspace _before_ the relevant element. Combining this
    // with |m_paintOffset| yields the "final" offset.
    AffineTransform m_svgTransform;

    Vector<LayoutObject*>& m_pendingDelayedPaintInvalidations;

    PaintLayer& m_enclosingSelfPaintingLayer;

#if ENABLE(ASSERT)
    bool m_didUpdatePaintOffsetAndClipForChildren;
#endif
};

// Suspends the PaintInvalidationState cached offset and clipRect optimization. Used under transforms
// that cannot be represented by PaintInvalidationState (common in SVG) and when paint invalidation
// containers don't follow the common tree-walk algorithm (e.g. when an absolute positioned descendant
// is nested under a relatively positioned inline-block child).
class ForceHorriblySlowRectMapping {
    STACK_ALLOCATED();
    WTF_MAKE_NONCOPYABLE(ForceHorriblySlowRectMapping);
public:
    ForceHorriblySlowRectMapping(const PaintInvalidationState* paintInvalidationState)
        : m_paintInvalidationState(paintInvalidationState)
        , m_didDisable(m_paintInvalidationState && m_paintInvalidationState->cachedOffsetsEnabled())
    {
        if (m_paintInvalidationState)
            m_paintInvalidationState->m_cachedOffsetsEnabled = false;
    }

    ~ForceHorriblySlowRectMapping()
    {
        if (m_didDisable)
            m_paintInvalidationState->m_cachedOffsetsEnabled = true;
    }
private:
    const PaintInvalidationState* m_paintInvalidationState;
    bool m_didDisable;
};

} // namespace blink

#endif // PaintInvalidationState_h
