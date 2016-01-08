// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/IntersectionObserverEntry.h"

#include "core/dom/Element.h"

namespace blink {

IntersectionObserverEntry::IntersectionObserverEntry(double time, const IntRect& boundingClientRect, const IntRect& rootBounds, const IntRect& intersectionRect, Element* target)
    : m_time(time)
    , m_boundingClientRect(ClientRect::create(boundingClientRect))
    , m_rootBounds(ClientRect::create(rootBounds))
    , m_intersectionRect(ClientRect::create(intersectionRect))
    , m_target(target)

{
}

IntersectionObserverEntry::~IntersectionObserverEntry()
{
}

DEFINE_TRACE(IntersectionObserverEntry)
{
    visitor->trace(m_boundingClientRect);
    visitor->trace(m_rootBounds);
    visitor->trace(m_intersectionRect);
    visitor->trace(m_target);
}

} // namespace blink
