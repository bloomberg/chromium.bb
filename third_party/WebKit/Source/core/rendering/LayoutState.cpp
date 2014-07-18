/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "config.h"
#include "core/rendering/LayoutState.h"

#include "core/rendering/RenderInline.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderView.h"
#include "platform/Partitions.h"

namespace blink {

LayoutState::LayoutState(LayoutUnit pageLogicalHeight, bool pageLogicalHeightChanged, RenderView& view)
    : m_isPaginated(pageLogicalHeight)
    , m_pageLogicalHeightChanged(pageLogicalHeightChanged)
    , m_columnInfo(0)
    , m_next(0)
    , m_pageLogicalHeight(pageLogicalHeight)
    , m_renderer(view)
{
    ASSERT(!view.layoutState());
    view.pushLayoutState(*this);
}

LayoutState::LayoutState(RenderBox& renderer, const LayoutSize& offset, LayoutUnit pageLogicalHeight, bool pageLogicalHeightChanged, ColumnInfo* columnInfo)
    : m_columnInfo(columnInfo)
    , m_next(renderer.view()->layoutState())
    , m_renderer(renderer)
{
    renderer.view()->pushLayoutState(*this);
    bool fixed = renderer.isOutOfFlowPositioned() && renderer.style()->position() == FixedPosition;
    if (fixed) {
        // FIXME: This doesn't work correctly with transforms.
        FloatPoint fixedOffset = renderer.view()->localToAbsolute(FloatPoint(), IsFixed);
        m_layoutOffset = LayoutSize(fixedOffset.x(), fixedOffset.y()) + offset;
    } else {
        m_layoutOffset = m_next->m_layoutOffset + offset;
    }

    if (renderer.isOutOfFlowPositioned() && !fixed) {
        if (RenderObject* container = renderer.container()) {
            if (container->style()->hasInFlowPosition() && container->isRenderInline())
                m_layoutOffset += toRenderInline(container)->offsetForInFlowPositionedInline(renderer);
        }
    }
    // If we establish a new page height, then cache the offset to the top of the first page.
    // We can compare this later on to figure out what part of the page we're actually on,
    if (pageLogicalHeight || m_columnInfo || renderer.isRenderFlowThread()) {
        m_pageLogicalHeight = pageLogicalHeight;
        bool isFlipped = renderer.style()->isFlippedBlocksWritingMode();
        m_pageOffset = LayoutSize(m_layoutOffset.width() + (!isFlipped ? renderer.borderLeft() + renderer.paddingLeft() : renderer.borderRight() + renderer.paddingRight()),
            m_layoutOffset.height() + (!isFlipped ? renderer.borderTop() + renderer.paddingTop() : renderer.borderBottom() + renderer.paddingBottom()));
        m_pageLogicalHeightChanged = pageLogicalHeightChanged;
        m_isPaginated = true;
    } else {
        // If we don't establish a new page height, then propagate the old page height and offset down.
        m_pageLogicalHeight = m_next->m_pageLogicalHeight;
        m_pageLogicalHeightChanged = m_next->m_pageLogicalHeightChanged;
        m_pageOffset = m_next->m_pageOffset;

        // Disable pagination for objects we don't support. For now this includes overflow:scroll/auto, inline blocks and
        // writing mode roots.
        if (renderer.isUnsplittableForPagination()) {
            m_pageLogicalHeight = 0;
            m_isPaginated = false;
        } else {
            m_isPaginated = m_pageLogicalHeight || m_next->m_columnInfo || renderer.flowThreadContainingBlock();
        }
    }

    if (!m_columnInfo)
        m_columnInfo = m_next->m_columnInfo;

    // FIXME: <http://bugs.webkit.org/show_bug.cgi?id=13443> Apply control clip if present.
}

LayoutState::LayoutState(RenderObject& root)
    : m_isPaginated(false)
    , m_pageLogicalHeightChanged(false)
    , m_columnInfo(0)
    , m_next(root.view()->layoutState())
    , m_pageLogicalHeight(0)
    , m_renderer(root)
{
    // FIXME: Why does RenderTableSection create this wonky LayoutState?
    ASSERT(!m_next || root.isTableSection());
    // We'll end up pushing in RenderView itself, so don't bother adding it.
    if (root.isRenderView())
        return;

    root.view()->pushLayoutState(*this);

    RenderObject* container = root.container();
    FloatPoint absContentPoint = container->localToAbsolute(FloatPoint(), UseTransforms);
    m_layoutOffset = LayoutSize(absContentPoint.x(), absContentPoint.y());
}

LayoutState::~LayoutState()
{
    if (m_renderer.view()->layoutState()) {
        ASSERT(m_renderer.view()->layoutState() == this);
        m_renderer.view()->popLayoutState();
    }
}

void LayoutState::clearPaginationInformation()
{
    m_pageLogicalHeight = m_next->m_pageLogicalHeight;
    m_pageOffset = m_next->m_pageOffset;
    m_columnInfo = m_next->m_columnInfo;
}

LayoutUnit LayoutState::pageLogicalOffset(const RenderBox& child, const LayoutUnit& childLogicalOffset) const
{
    if (child.isHorizontalWritingMode())
        return m_layoutOffset.height() + childLogicalOffset - m_pageOffset.height();
    return m_layoutOffset.width() + childLogicalOffset - m_pageOffset.width();
}

void LayoutState::addForcedColumnBreak(const RenderBox& child, const LayoutUnit& childLogicalOffset)
{
    if (!m_columnInfo || m_columnInfo->columnHeight())
        return;
    m_columnInfo->addForcedBreak(pageLogicalOffset(child, childLogicalOffset));
}

} // namespace blink
