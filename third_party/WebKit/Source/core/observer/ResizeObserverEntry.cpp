// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/observer/ResizeObserverEntry.h"

#include "core/dom/ClientRect.h"
#include "core/dom/Element.h"
#include "core/layout/LayoutBox.h"
#include "core/observer/ResizeObservation.h"

namespace blink {

class ResizeObservation;

ResizeObserverEntry::ResizeObserverEntry(Element* target)
    : m_target(target)
{
    FloatSize size = FloatSize(ResizeObservation::getTargetSize(m_target));
    FloatPoint location;
    LayoutBox* layout = m_target ? m_target->layoutBox() : nullptr;
    if (layout) {
        location = FloatPoint(layout->paddingLeft(), layout->paddingTop());
    }
    m_contentRect = ClientRect::create(FloatRect(location, size));
}

LayoutSize ResizeObserverEntry::contentSize() const
{
    return LayoutSize(m_contentRect->width(), m_contentRect->height());
}

DEFINE_TRACE(ResizeObserverEntry)
{
    visitor->trace(m_target);
    visitor->trace(m_contentRect);
}

} // namespace blink
