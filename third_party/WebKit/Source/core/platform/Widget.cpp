/*
 * Copyright (C) 2004, 2005, 2006, 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "core/platform/Widget.h"

#include "core/platform/HostWindow.h"
#include "core/platform/ScrollView.h"
#include "core/platform/graphics/IntRect.h"

#include <wtf/Assertions.h>

namespace WebCore {

Widget::Widget()
    : m_parent(0)
    , m_selfVisible(false)
    , m_parentVisible(false)
{
}

Widget::~Widget()
{
    ASSERT(!parent());
}

void Widget::setCursor(const Cursor& cursor)
{
    if (ScrollView* view = root())
        view->hostWindow()->setCursor(cursor);
}

void Widget::removeFromParent()
{
    if (parent())
        parent()->removeChild(this);
}

void Widget::setParent(ScrollView* view)
{
    ASSERT(!view || !m_parent);
    if (!view || !view->isVisible())
        setParentVisible(false);
    m_parent = view;
    if (view && view->isVisible())
        setParentVisible(true);
}

ScrollView* Widget::root() const
{
    const Widget* top = this;
    while (top->parent())
        top = top->parent();
    if (top->isFrameView())
        return const_cast<ScrollView*>(static_cast<const ScrollView*>(top));
    return 0;
}

IntRect Widget::convertFromRootView(const IntRect& rootRect) const
{
    if (const ScrollView* parentScrollView = parent()) {
        IntRect parentRect = parentScrollView->convertFromRootView(rootRect);
        return convertFromContainingView(parentRect);
    }
    return rootRect;
}

IntRect Widget::convertToRootView(const IntRect& localRect) const
{
    if (const ScrollView* parentScrollView = parent()) {
        IntRect parentRect = convertToContainingView(localRect);
        return parentScrollView->convertToRootView(parentRect);
    }
    return localRect;
}

IntPoint Widget::convertFromRootView(const IntPoint& rootPoint) const
{
    if (const ScrollView* parentScrollView = parent()) {
        IntPoint parentPoint = parentScrollView->convertFromRootView(rootPoint);
        return convertFromContainingView(parentPoint);
    }
    return rootPoint;
}

IntPoint Widget::convertToRootView(const IntPoint& localPoint) const
{
    if (const ScrollView* parentScrollView = parent()) {
        IntPoint parentPoint = convertToContainingView(localPoint);
        return parentScrollView->convertToRootView(parentPoint);
    }
    return localPoint;
}

IntRect Widget::convertFromContainingWindow(const IntRect& windowRect) const
{
    if (const ScrollView* parentScrollView = parent()) {
        IntRect parentRect = parentScrollView->convertFromContainingWindow(windowRect);
        return convertFromContainingView(parentRect);
    }
    return windowRect;
}

IntRect Widget::convertToContainingWindow(const IntRect& localRect) const
{
    if (const ScrollView* parentScrollView = parent()) {
        IntRect parentRect = convertToContainingView(localRect);
        return parentScrollView->convertToContainingWindow(parentRect);
    }
    return localRect;
}

IntPoint Widget::convertFromContainingWindow(const IntPoint& windowPoint) const
{
    if (const ScrollView* parentScrollView = parent()) {
        IntPoint parentPoint = parentScrollView->convertFromContainingWindow(windowPoint);
        return convertFromContainingView(parentPoint);
    }
    return windowPoint;
}

IntPoint Widget::convertToContainingWindow(const IntPoint& localPoint) const
{
    if (const ScrollView* parentScrollView = parent()) {
        IntPoint parentPoint = convertToContainingView(localPoint);
        return parentScrollView->convertToContainingWindow(parentPoint);
    }
    return localPoint;
}

IntRect Widget::convertToContainingView(const IntRect& localRect) const
{
    if (const ScrollView* parentScrollView = parent()) {
        IntRect parentRect(localRect);
        parentRect.setLocation(parentScrollView->convertChildToSelf(this, localRect.location()));
        return parentRect;
    }
    return localRect;
}

IntRect Widget::convertFromContainingView(const IntRect& parentRect) const
{
    if (const ScrollView* parentScrollView = parent()) {
        IntRect localRect = parentRect;
        localRect.setLocation(parentScrollView->convertSelfToChild(this, localRect.location()));
        return localRect;
    }

    return parentRect;
}

IntPoint Widget::convertToContainingView(const IntPoint& localPoint) const
{
    if (const ScrollView* parentScrollView = parent())
        return parentScrollView->convertChildToSelf(this, localPoint);

    return localPoint;
}

IntPoint Widget::convertFromContainingView(const IntPoint& parentPoint) const
{
    if (const ScrollView* parentScrollView = parent())
        return parentScrollView->convertSelfToChild(this, parentPoint);

    return parentPoint;
}

} // namespace WebCore
