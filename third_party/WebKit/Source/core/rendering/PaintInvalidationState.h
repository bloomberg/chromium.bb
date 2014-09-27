// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PaintInvalidationState_h
#define PaintInvalidationState_h

#include "platform/geometry/LayoutRect.h"
#include "platform/transforms/AffineTransform.h"
#include "wtf/Noncopyable.h"

namespace blink {

class RenderLayerModelObject;
class RenderObject;
class RenderSVGModelObject;
class RenderView;

class PaintInvalidationState {
    WTF_MAKE_NONCOPYABLE(PaintInvalidationState);
public:
    PaintInvalidationState(const PaintInvalidationState& next, RenderLayerModelObject& renderer, const RenderLayerModelObject& paintInvalidationContainer);
    PaintInvalidationState(const PaintInvalidationState& next, const RenderSVGModelObject& renderer);

    explicit PaintInvalidationState(const RenderView&);

    const LayoutRect& clipRect() const { return m_clipRect; }
    const LayoutSize& paintOffset() const { return m_paintOffset; }
    const AffineTransform& svgTransform() const { ASSERT(m_svgTransform); return *m_svgTransform; }

    bool cachedOffsetsEnabled() const { return m_cachedOffsetsEnabled; }
    bool isClipped() const { return m_clipped; }

    bool forceCheckForPaintInvalidation() const { return m_forceCheckForPaintInvalidation; }
    void setForceCheckForPaintInvalidation() { m_forceCheckForPaintInvalidation = true; }

    const RenderLayerModelObject& paintInvalidationContainer() const { return m_paintInvalidationContainer; }

    bool canMapToContainer(const RenderLayerModelObject* container) const
    {
        return m_cachedOffsetsEnabled && container == &m_paintInvalidationContainer;
    }
private:
    void applyClipIfNeeded(const RenderObject&);
    void addClipRectRelativeToPaintOffset(const LayoutSize& clipSize);

    friend class ForceHorriblySlowRectMapping;

    bool m_clipped;
    mutable bool m_cachedOffsetsEnabled;
    bool m_forceCheckForPaintInvalidation;

    LayoutRect m_clipRect;

    // x/y offset from paint invalidation container. Includes relative positioning and scroll offsets.
    LayoutSize m_paintOffset;

    const RenderLayerModelObject& m_paintInvalidationContainer;

    // Transform from the initial viewport coordinate system of an outermost
    // SVG root to the userspace _before_ the relevant element. Combining this
    // with |m_paintOffset| yields the "final" offset.
    OwnPtr<AffineTransform> m_svgTransform;
};

} // namespace blink

#endif // PaintInvalidationState_h
