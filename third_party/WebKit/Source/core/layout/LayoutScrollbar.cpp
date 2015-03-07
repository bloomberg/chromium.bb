/*
 * Copyright (C) 2008, 2009 Apple Inc. All Rights Reserved.
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
#include "core/layout/LayoutScrollbar.h"

#include "core/css/PseudoStyleRequest.h"
#include "core/frame/FrameView.h"
#include "core/frame/LocalFrame.h"
#include "core/layout/LayoutPart.h"
#include "core/layout/LayoutScrollbarPart.h"
#include "core/layout/LayoutScrollbarTheme.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

PassRefPtrWillBeRawPtr<Scrollbar> LayoutScrollbar::createCustomScrollbar(ScrollableArea* scrollableArea, ScrollbarOrientation orientation, Node* ownerNode, LocalFrame* owningFrame)
{
    return adoptRefWillBeNoop(new LayoutScrollbar(scrollableArea, orientation, ownerNode, owningFrame));
}

LayoutScrollbar::LayoutScrollbar(ScrollableArea* scrollableArea, ScrollbarOrientation orientation, Node* ownerNode, LocalFrame* owningFrame)
    : Scrollbar(scrollableArea, orientation, RegularScrollbar, LayoutScrollbarTheme::renderScrollbarTheme())
    , m_owner(ownerNode)
    , m_owningFrame(owningFrame)
{
    ASSERT(ownerNode || owningFrame);

    // FIXME: We need to do this because LayoutScrollbar::styleChanged is called as soon as the scrollbar is created.

    // Update the scrollbar size.
    IntRect rect(0, 0, 0, 0);
    updateScrollbarPart(ScrollbarBGPart);
    if (LayoutScrollbarPart* part = m_parts.get(ScrollbarBGPart)) {
        part->layout();
        rect.setSize(flooredIntSize(part->size()));
    } else if (this->orientation() == HorizontalScrollbar) {
        rect.setWidth(this->width());
    } else {
        rect.setHeight(this->height());
    }

    setFrameRect(rect);

}

LayoutScrollbar::~LayoutScrollbar()
{
    if (m_parts.isEmpty())
        return;

    // When a scrollbar is detached from its parent (causing all parts removal) and
    // ready to be destroyed, its destruction can be delayed because of RefPtr
    // maintained in other classes such as EventHandler (m_lastScrollbarUnderMouse).
    // Meanwhile, we can have a call to updateScrollbarPart which recreates the
    // scrollbar part. So, we need to destroy these parts since we don't want them
    // to call on a destroyed scrollbar. See webkit bug 68009.
    updateScrollbarParts(true);
}

DEFINE_TRACE(LayoutScrollbar)
{
#if ENABLE(OILPAN)
    visitor->trace(m_owner);
    visitor->trace(m_owningFrame);
#endif
    Scrollbar::trace(visitor);
}

LayoutBox* LayoutScrollbar::owningRenderer() const
{
    if (m_owningFrame)
        return m_owningFrame->ownerLayoutObject();
    return m_owner && m_owner->layoutObject() ? m_owner->layoutObject()->enclosingBox() : 0;
}

void LayoutScrollbar::setParent(Widget* parent)
{
    Scrollbar::setParent(parent);
    if (!parent) {
        // Destroy all of the scrollbar's LayoutBoxes.
        updateScrollbarParts(true);
    }
}

void LayoutScrollbar::setEnabled(bool e)
{
    bool wasEnabled = enabled();
    Scrollbar::setEnabled(e);
    if (wasEnabled != e)
        updateScrollbarParts();
}

void LayoutScrollbar::styleChanged()
{
    updateScrollbarParts();
}

void LayoutScrollbar::setHoveredPart(ScrollbarPart part)
{
    if (part == m_hoveredPart)
        return;

    ScrollbarPart oldPart = m_hoveredPart;
    m_hoveredPart = part;

    updateScrollbarPart(oldPart);
    updateScrollbarPart(m_hoveredPart);

    updateScrollbarPart(ScrollbarBGPart);
    updateScrollbarPart(TrackBGPart);
}

void LayoutScrollbar::setPressedPart(ScrollbarPart part)
{
    ScrollbarPart oldPart = m_pressedPart;
    Scrollbar::setPressedPart(part);

    updateScrollbarPart(oldPart);
    updateScrollbarPart(part);

    updateScrollbarPart(ScrollbarBGPart);
    updateScrollbarPart(TrackBGPart);
}

PassRefPtr<LayoutStyle> LayoutScrollbar::getScrollbarPseudoStyle(ScrollbarPart partType, PseudoId pseudoId)
{
    if (!owningRenderer())
        return nullptr;

    RefPtr<LayoutStyle> result = owningRenderer()->getUncachedPseudoStyle(PseudoStyleRequest(pseudoId, this, partType), owningRenderer()->style());
    // Scrollbars for root frames should always have background color
    // unless explicitly specified as transparent. So we force it.
    // This is because WebKit assumes scrollbar to be always painted and missing background
    // causes visual artifact like non-paint invalidated dirty region.
    if (result && m_owningFrame && m_owningFrame->view() && !m_owningFrame->view()->isTransparent() && !result->hasBackground())
        result->setBackgroundColor(StyleColor(Color::white));

    return result;
}

void LayoutScrollbar::updateScrollbarParts(bool destroy)
{
    updateScrollbarPart(ScrollbarBGPart, destroy);
    updateScrollbarPart(BackButtonStartPart, destroy);
    updateScrollbarPart(ForwardButtonStartPart, destroy);
    updateScrollbarPart(BackTrackPart, destroy);
    updateScrollbarPart(ThumbPart, destroy);
    updateScrollbarPart(ForwardTrackPart, destroy);
    updateScrollbarPart(BackButtonEndPart, destroy);
    updateScrollbarPart(ForwardButtonEndPart, destroy);
    updateScrollbarPart(TrackBGPart, destroy);

    if (destroy)
        return;

    // See if the scrollbar's thickness changed.  If so, we need to mark our owning object as needing a layout.
    bool isHorizontal = orientation() == HorizontalScrollbar;
    int oldThickness = isHorizontal ? height() : width();
    int newThickness = 0;
    LayoutScrollbarPart* part = m_parts.get(ScrollbarBGPart);
    if (part) {
        part->layout();
        newThickness = isHorizontal ? part->size().height() : part->size().width();
    }

    if (newThickness != oldThickness) {
        setFrameRect(IntRect(location(), IntSize(isHorizontal ? width() : newThickness, isHorizontal ? newThickness : height())));
        if (LayoutBox* box = owningRenderer()) {
            if (box->isLayoutBlock())
                toLayoutBlock(box)->notifyScrollbarThicknessChanged();
            box->setChildNeedsLayout();
        }
    }
}

static PseudoId pseudoForScrollbarPart(ScrollbarPart part)
{
    switch (part) {
    case BackButtonStartPart:
    case ForwardButtonStartPart:
    case BackButtonEndPart:
    case ForwardButtonEndPart:
        return SCROLLBAR_BUTTON;
    case BackTrackPart:
    case ForwardTrackPart:
        return SCROLLBAR_TRACK_PIECE;
    case ThumbPart:
        return SCROLLBAR_THUMB;
    case TrackBGPart:
        return SCROLLBAR_TRACK;
    case ScrollbarBGPart:
        return SCROLLBAR;
    case NoPart:
    case AllParts:
        break;
    }
    ASSERT_NOT_REACHED();
    return SCROLLBAR;
}

void LayoutScrollbar::updateScrollbarPart(ScrollbarPart partType, bool destroy)
{
    if (partType == NoPart)
        return;

    RefPtr<LayoutStyle> partStyle = !destroy ? getScrollbarPseudoStyle(partType,  pseudoForScrollbarPart(partType)) : PassRefPtr<LayoutStyle>(nullptr);

    bool needRenderer = !destroy && partStyle && partStyle->display() != NONE;

    if (needRenderer && partStyle->display() != BLOCK) {
        // See if we are a button that should not be visible according to OS settings.
        ScrollbarButtonsPlacement buttonsPlacement = theme()->buttonsPlacement();
        switch (partType) {
        case BackButtonStartPart:
            needRenderer = (buttonsPlacement == ScrollbarButtonsSingle || buttonsPlacement == ScrollbarButtonsDoubleStart
                || buttonsPlacement == ScrollbarButtonsDoubleBoth);
            break;
        case ForwardButtonStartPart:
            needRenderer = (buttonsPlacement == ScrollbarButtonsDoubleStart || buttonsPlacement == ScrollbarButtonsDoubleBoth);
            break;
        case BackButtonEndPart:
            needRenderer = (buttonsPlacement == ScrollbarButtonsDoubleEnd || buttonsPlacement == ScrollbarButtonsDoubleBoth);
            break;
        case ForwardButtonEndPart:
            needRenderer = (buttonsPlacement == ScrollbarButtonsSingle || buttonsPlacement == ScrollbarButtonsDoubleEnd
                || buttonsPlacement == ScrollbarButtonsDoubleBoth);
            break;
        default:
            break;
        }
    }

    LayoutScrollbarPart* partRenderer = m_parts.get(partType);
    if (!partRenderer && needRenderer) {
        partRenderer = LayoutScrollbarPart::createAnonymous(&owningRenderer()->document(), this, partType);
        m_parts.set(partType, partRenderer);
    } else if (partRenderer && !needRenderer) {
        m_parts.remove(partType);
        partRenderer->destroy();
        partRenderer = 0;
    }

    if (partRenderer)
        partRenderer->setStyle(partStyle.release());
}

IntRect LayoutScrollbar::buttonRect(ScrollbarPart partType)
{
    LayoutScrollbarPart* partRenderer = m_parts.get(partType);
    if (!partRenderer)
        return IntRect();

    partRenderer->layout();

    bool isHorizontal = orientation() == HorizontalScrollbar;
    if (partType == BackButtonStartPart)
        return IntRect(location(), IntSize(isHorizontal ? partRenderer->pixelSnappedWidth() : width(), isHorizontal ? height() : partRenderer->pixelSnappedHeight()));
    if (partType == ForwardButtonEndPart) {
        return IntRect(isHorizontal ? x() + width() - partRenderer->pixelSnappedWidth() : x(),
            isHorizontal ? y() : y() + height() - partRenderer->pixelSnappedHeight(),
            isHorizontal ? partRenderer->pixelSnappedWidth() : width(),
            isHorizontal ? height() : partRenderer->pixelSnappedHeight());
    }

    if (partType == ForwardButtonStartPart) {
        IntRect previousButton = buttonRect(BackButtonStartPart);
        return IntRect(isHorizontal ? x() + previousButton.width() : x(),
            isHorizontal ? y() : y() + previousButton.height(),
            isHorizontal ? partRenderer->pixelSnappedWidth() : width(),
            isHorizontal ? height() : partRenderer->pixelSnappedHeight());
    }

    IntRect followingButton = buttonRect(ForwardButtonEndPart);
    return IntRect(isHorizontal ? x() + width() - followingButton.width() - partRenderer->pixelSnappedWidth() : x(),
        isHorizontal ? y() : y() + height() - followingButton.height() - partRenderer->pixelSnappedHeight(),
        isHorizontal ? partRenderer->pixelSnappedWidth() : width(),
        isHorizontal ? height() : partRenderer->pixelSnappedHeight());
}

IntRect LayoutScrollbar::trackRect(int startLength, int endLength)
{
    LayoutScrollbarPart* part = m_parts.get(TrackBGPart);
    if (part)
        part->layout();

    if (orientation() == HorizontalScrollbar) {
        int marginLeft = part ? static_cast<int>(part->marginLeft()) : 0;
        int marginRight = part ? static_cast<int>(part->marginRight()) : 0;
        startLength += marginLeft;
        endLength += marginRight;
        int totalLength = startLength + endLength;
        return IntRect(x() + startLength, y(), width() - totalLength, height());
    }

    int marginTop = part ? static_cast<int>(part->marginTop()) : 0;
    int marginBottom = part ? static_cast<int>(part->marginBottom()) : 0;
    startLength += marginTop;
    endLength += marginBottom;
    int totalLength = startLength + endLength;

    return IntRect(x(), y() + startLength, width(), height() - totalLength);
}

IntRect LayoutScrollbar::trackPieceRectWithMargins(ScrollbarPart partType, const IntRect& oldRect)
{
    LayoutScrollbarPart* partRenderer = m_parts.get(partType);
    if (!partRenderer)
        return oldRect;

    partRenderer->layout();

    IntRect rect = oldRect;
    if (orientation() == HorizontalScrollbar) {
        rect.setX(rect.x() + partRenderer->marginLeft());
        rect.setWidth(rect.width() - partRenderer->marginWidth());
    } else {
        rect.setY(rect.y() + partRenderer->marginTop());
        rect.setHeight(rect.height() - partRenderer->marginHeight());
    }
    return rect;
}

int LayoutScrollbar::minimumThumbLength()
{
    LayoutScrollbarPart* partRenderer = m_parts.get(ThumbPart);
    if (!partRenderer)
        return 0;
    partRenderer->layout();
    return orientation() == HorizontalScrollbar ? partRenderer->size().width() : partRenderer->size().height();
}

}
