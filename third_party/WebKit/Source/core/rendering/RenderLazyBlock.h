/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef RenderLazyBlock_h
#define RenderLazyBlock_h

#include "core/rendering/RenderBlock.h"

namespace WebCore {

class RenderLazyBlock FINAL : public RenderBlock {
public:
    RenderLazyBlock(Element*);
    virtual ~RenderLazyBlock();

    bool isAttached() const { return m_attached; }
    bool isNested() const;

    void markForNestedLayout()
    {
        setNeedsLayout();
        m_isNestedLayout = true;
    }

    RenderLazyBlock* next() const { return m_next; }
    RenderLazyBlock* previous() const { return m_previous; }

    RenderBox* firstVisibleChildBox() const { return m_firstVisibleChildBox; }
    RenderBox* lastVisibleChildBox() const { return m_lastVisibleChildBox; }

private:
    virtual void paintChildren(PaintInfo&, const LayoutPoint&) OVERRIDE;

    virtual bool isRenderLazyBlock() const OVERRIDE { return true; }
    virtual void willBeDestroyed() OVERRIDE;
    virtual void willBeRemovedFromTree() OVERRIDE;
    virtual void layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight) OVERRIDE;

    void layoutChildren(bool relayoutChildren);

    void attachLazyBlock();
    void detachLazyBlock();

    RenderLazyBlock* m_next;
    RenderLazyBlock* m_previous;
    RenderBox* m_firstVisibleChildBox;
    RenderBox* m_lastVisibleChildBox;
    bool m_attached;
    bool m_isNestedLayout;
    LayoutRect m_intersectRect;
    IntRect m_expandedViewportRect;
};

inline RenderLazyBlock* toRenderLazyBlock(RenderObject* object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!object || object->isRenderLazyBlock());
    return static_cast<RenderLazyBlock*>(object);
}

inline const RenderLazyBlock* toRenderLazyBlock(const RenderObject* object)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!object || object->isRenderLazyBlock());
    return static_cast<const RenderLazyBlock*>(object);
}

} // namespace WebCore

#endif // RenderLazyBlock_h
