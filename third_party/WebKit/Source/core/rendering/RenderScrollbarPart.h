/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RenderScrollbarPart_h
#define RenderScrollbarPart_h

#include "core/rendering/RenderBlock.h"
#include "platform/scroll/ScrollTypes.h"

namespace blink {

class RenderScrollbar;

class RenderScrollbarPart final : public RenderBlock {
public:
    static RenderScrollbarPart* createAnonymous(Document*, RenderScrollbar* = 0, ScrollbarPart = NoPart);

    virtual ~RenderScrollbarPart();

    virtual const char* renderName() const override { return "RenderScrollbarPart"; }

    virtual LayerType layerTypeRequired() const override { return NoLayer; }

    virtual void layout() override;

    // Scrollbar parts needs to be rendered at device pixel boundaries.
    virtual LayoutUnit marginTop() const override { ASSERT(isIntegerValue(m_marginBox.top())); return m_marginBox.top(); }
    virtual LayoutUnit marginBottom() const override { ASSERT(isIntegerValue(m_marginBox.bottom())); return m_marginBox.bottom(); }
    virtual LayoutUnit marginLeft() const override { ASSERT(isIntegerValue(m_marginBox.left())); return m_marginBox.left(); }
    virtual LayoutUnit marginRight() const override { ASSERT(isIntegerValue(m_marginBox.right())); return m_marginBox.right(); }

    virtual bool isOfType(RenderObjectType type) const override { return type == RenderObjectRenderScrollbarPart || RenderBlock::isOfType(type); }
    RenderObject* rendererOwningScrollbar() const;

protected:
    virtual void styleWillChange(StyleDifference, const RenderStyle& newStyle) override;
    virtual void styleDidChange(StyleDifference, const RenderStyle* oldStyle) override;
    virtual void imageChanged(WrappedImagePtr, const IntRect* = 0) override;

private:
    RenderScrollbarPart(RenderScrollbar*, ScrollbarPart);

    virtual void computePreferredLogicalWidths() override;

    // Have all padding getters return 0. The important point here is to avoid resolving percents
    // against the containing block, since scroll bar corners don't always have one (so it would
    // crash). Scroll bar corners are not actually laid out, and they don't have child content, so
    // what we return here doesn't really matter.
    virtual LayoutUnit paddingTop() const override { return LayoutUnit(); }
    virtual LayoutUnit paddingBottom() const override { return LayoutUnit(); }
    virtual LayoutUnit paddingLeft() const override { return LayoutUnit(); }
    virtual LayoutUnit paddingRight() const override { return LayoutUnit(); }
    virtual LayoutUnit paddingBefore() const override { return LayoutUnit(); }
    virtual LayoutUnit paddingAfter() const override { return LayoutUnit(); }
    virtual LayoutUnit paddingStart() const override { return LayoutUnit(); }
    virtual LayoutUnit paddingEnd() const override { return LayoutUnit(); }

    void layoutHorizontalPart();
    void layoutVerticalPart();

    void computeScrollbarWidth();
    void computeScrollbarHeight();

    RenderScrollbar* m_scrollbar;
    ScrollbarPart m_part;
};

DEFINE_RENDER_OBJECT_TYPE_CASTS(RenderScrollbarPart, isRenderScrollbarPart());

} // namespace blink

#endif // RenderScrollbarPart_h
