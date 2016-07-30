// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/observer/ResizeObservation.h"

#include "core/layout/LayoutBox.h"
#include "core/observer/ResizeObserver.h"
#include "core/svg/SVGElement.h"
#include "core/svg/SVGGraphicsElement.h"

namespace blink {

ResizeObservation::ResizeObservation(Element* target, ResizeObserver* observer)
    : m_target(target)
    , m_observer(observer)
    , m_observationSize(0, 0)
    , m_elementSizeChanged(true)
{
    DCHECK(m_target);
    m_observer->elementSizeChanged();
}

void ResizeObservation::setObservationSize(const LayoutSize& size)
{
    m_observationSize = size;
    m_elementSizeChanged = false;
}

bool ResizeObservation::observationSizeOutOfSync() const
{
    return m_elementSizeChanged && m_observationSize != ResizeObservation::getTargetSize(m_target);
}

size_t ResizeObservation::targetDepth()
{
    unsigned depth = 0;
    for (Element* parent = m_target; parent; parent = parent->parentElement())
        ++depth;
    return depth;
}

LayoutSize ResizeObservation::getTargetSize(Element* target) // static
{
    if (target) {
        if (target->isSVGElement() && toSVGElement(target)->isSVGGraphicsElement()) {
            SVGGraphicsElement& svg = toSVGGraphicsElement(*target);
            return LayoutSize(svg.getBBox().size());
        }
        LayoutBox* layout = target->layoutBox();
        if (layout)
            return layout->contentSize();
    }
    return LayoutSize();
}

void ResizeObservation::elementSizeChanged()
{
    m_elementSizeChanged = true;
    m_observer->elementSizeChanged();
}

DEFINE_TRACE(ResizeObservation)
{
    visitor->trace(m_target);
    visitor->trace(m_observer);
}

} // namespace blink
