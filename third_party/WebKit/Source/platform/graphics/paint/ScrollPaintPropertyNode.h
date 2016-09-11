// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScrollPaintPropertyNode_h
#define ScrollPaintPropertyNode_h

#include "platform/PlatformExport.h"
#include "platform/geometry/FloatSize.h"
#include "platform/graphics/paint/TransformPaintPropertyNode.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/RefPtr.h"

#include <iosfwd>

namespace blink {

// A scroll node contains auxiliary scrolling information for compositor-thread
// scrolling and includes how far an area can be scrolled, which
// |TransformPaintPropertyNode| contains the scroll offset, etc.
class PLATFORM_EXPORT ScrollPaintPropertyNode : public RefCounted<ScrollPaintPropertyNode> {
public:
    static PassRefPtr<ScrollPaintPropertyNode> create(
        PassRefPtr<const ScrollPaintPropertyNode> parent,
        PassRefPtr<const TransformPaintPropertyNode> scrollOffsetTranslation,
        const IntSize& clip,
        const IntSize& bounds,
        bool userScrollableHorizontal,
        bool userScrollableVertical)
    {
        return adoptRef(new ScrollPaintPropertyNode(std::move(parent), std::move(scrollOffsetTranslation), clip, bounds, userScrollableHorizontal, userScrollableVertical));
    }

    void update(PassRefPtr<const ScrollPaintPropertyNode> parent, PassRefPtr<const TransformPaintPropertyNode> scrollOffsetTranslation, const IntSize& clip, const IntSize& bounds, bool userScrollableHorizontal, bool userScrollableVertical)
    {
        DCHECK(parent != this);
        m_parent = parent;
        DCHECK(scrollOffsetTranslation->matrix().isIdentityOr2DTranslation());
        m_scrollOffsetTranslation = scrollOffsetTranslation;
        m_clip = clip;
        m_bounds = bounds;
        m_userScrollableHorizontal = userScrollableHorizontal;
        m_userScrollableVertical = userScrollableVertical;
    }

    const ScrollPaintPropertyNode* parent() const { return m_parent.get(); }

    // Transform that the scroll is relative to.
    const TransformPaintPropertyNode* scrollOffsetTranslation() const { return m_scrollOffsetTranslation.get(); }

    // The clipped area that contains the scrolled content.
    const IntSize& clip() const { return m_clip; }

    // The bounds of the content that is scrolled within |clip|.
    const IntSize& bounds() const { return m_bounds; }

    bool userScrollableHorizontal() const { return m_userScrollableHorizontal; }
    bool userScrollableVertical() const { return m_userScrollableVertical; }

private:
    ScrollPaintPropertyNode(
        PassRefPtr<const ScrollPaintPropertyNode> parent,
        PassRefPtr<const TransformPaintPropertyNode> scrollOffsetTranslation,
        IntSize clip,
        IntSize bounds,
        bool userScrollableHorizontal,
        bool userScrollableVertical)
        : m_parent(parent)
        , m_scrollOffsetTranslation(scrollOffsetTranslation)
        , m_clip(clip)
        , m_bounds(bounds)
        , m_userScrollableHorizontal(userScrollableHorizontal)
        , m_userScrollableVertical(userScrollableVertical)
    {
        DCHECK(m_scrollOffsetTranslation->matrix().isIdentityOr2DTranslation());
    }

    RefPtr<const ScrollPaintPropertyNode> m_parent;
    RefPtr<const TransformPaintPropertyNode> m_scrollOffsetTranslation;
    IntSize m_clip;
    IntSize m_bounds;
    bool m_userScrollableHorizontal;
    bool m_userScrollableVertical;
    // TODO(pdr): Add main thread scrolling reasons.
    // TODO(pdr): Add an offset for the clip and bounds to the transform.
    // TODO(pdr): Add 2 bits for whether this is a viewport scroll node.
    // TODO(pdr): Add a bit for whether this is affected by page scale.
};

// Redeclared here to avoid ODR issues.
// See platform/testing/PaintPrinters.h.
void PrintTo(const ScrollPaintPropertyNode&, std::ostream*);

} // namespace blink

#endif // ScrollPaintPropertyNode_h
