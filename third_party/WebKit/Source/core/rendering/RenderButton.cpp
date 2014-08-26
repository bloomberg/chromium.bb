/**
 * Copyright (C) 2005 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "core/rendering/RenderButton.h"

namespace blink {

using namespace HTMLNames;

RenderButton::RenderButton(Element* element)
    : RenderFlexibleBox(element)
    , m_inner(0)
{
}

RenderButton::~RenderButton()
{
}

void RenderButton::addChild(RenderObject* newChild, RenderObject* beforeChild)
{
    if (!m_inner) {
        // Create an anonymous block.
        ASSERT(!firstChild());
        m_inner = createAnonymousBlock(style()->display());
        RenderFlexibleBox::addChild(m_inner);
    }

    m_inner->addChild(newChild, beforeChild);
}

void RenderButton::removeChild(RenderObject* oldChild)
{
    // m_inner should be the only child, but checking for direct children who
    // are not m_inner prevents security problems when that assumption is
    // violated.
    if (oldChild == m_inner || !m_inner || oldChild->parent() == this) {
        ASSERT(oldChild == m_inner || !m_inner);
        RenderFlexibleBox::removeChild(oldChild);
        m_inner = 0;
    } else
        m_inner->removeChild(oldChild);
}

void RenderButton::updateAnonymousChildStyle(const RenderObject* child, RenderStyle* childStyle) const
{
    ASSERT(!m_inner || child == m_inner);

    childStyle->setFlexGrow(1.0f);
    // Use margin:auto instead of align-items:center to get safe centering, i.e.
    // when the content overflows, treat it the same as align-items: flex-start.
    childStyle->setMarginTop(Length());
    childStyle->setMarginBottom(Length());
    childStyle->setFlexDirection(style()->flexDirection());
    childStyle->setJustifyContent(style()->justifyContent());
    childStyle->setFlexWrap(style()->flexWrap());
    childStyle->setAlignItems(style()->alignItems());
    childStyle->setAlignContent(style()->alignContent());
}

bool RenderButton::canHaveGeneratedChildren() const
{
    // Input elements can't have generated children, but button elements can. We'll
    // write the code assuming any other button types that might emerge in the future
    // can also have children.
    return !isHTMLInputElement(*node());
}

LayoutRect RenderButton::controlClipRect(const LayoutPoint& additionalOffset) const
{
    // Clip to the padding box to at least give content the extra padding space.
    return LayoutRect(additionalOffset.x() + borderLeft(), additionalOffset.y() + borderTop(), width() - borderLeft() - borderRight(), height() - borderTop() - borderBottom());
}

int RenderButton::baselinePosition(FontBaseline baseline, bool firstLine, LineDirectionMode direction, LinePositionMode linePositionMode) const
{
    ASSERT(linePositionMode == PositionOnContainingLine);
    // We want to call the RenderBlock version of firstLineBoxBaseline to
    // avoid RenderFlexibleBox synthesizing a baseline that we don't want.
    // We use this check as a proxy for "are there any line boxes in this button"
    if (!hasLineIfEmpty() && RenderBlock::firstLineBoxBaseline() == -1) {
        // To ensure that we have a consistent baseline when we have no children,
        // even when we have the anonymous RenderBlock child, we calculate the
        // baseline for the empty case manually here.
        if (direction == HorizontalLine)
            return marginTop() + borderTop() + paddingTop() + contentHeight();

        return marginRight() + borderRight() + paddingRight() + contentWidth();
    }
    return RenderFlexibleBox::baselinePosition(baseline, firstLine, direction, linePositionMode);
}

} // namespace blink
