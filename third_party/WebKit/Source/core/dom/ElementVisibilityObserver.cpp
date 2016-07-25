// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/dom/ElementVisibilityObserver.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Element.h"
#include "core/dom/IntersectionObserver.h"
#include "core/dom/IntersectionObserverEntry.h"
#include "core/dom/IntersectionObserverInit.h"
#include "wtf/Functional.h"

namespace blink {

ElementVisibilityObserver* ElementVisibilityObserver::create(Element* element, Client* client)
{
    ElementVisibilityObserver* observer = new ElementVisibilityObserver(client);
    observer->start(element);
    return observer;
}

ElementVisibilityObserver::ElementVisibilityObserver(Client* client)
    : m_client(client)
{
    DCHECK(m_client);
}

ElementVisibilityObserver::~ElementVisibilityObserver() = default;

void ElementVisibilityObserver::start(Element* element)
{
    IntersectionObserverInit options;
    DoubleOrDoubleArray threshold;
    threshold.setDouble(std::numeric_limits<float>::min());
    options.setThreshold(threshold);

    DCHECK(!m_intersectionObserver);
    m_intersectionObserver = IntersectionObserver::create(options, *this, ASSERT_NO_EXCEPTION);
    DCHECK(m_intersectionObserver);
    m_intersectionObserver->observe(element);
}

void ElementVisibilityObserver::stop()
{
    DCHECK(m_intersectionObserver);

    m_intersectionObserver->disconnect();
    m_intersectionObserver = nullptr;
    // Client will no longer be called upon, release.
    m_client = nullptr;
}

void ElementVisibilityObserver::handleEvent(const HeapVector<Member<IntersectionObserverEntry>>& entries, IntersectionObserver&)
{
    if (!m_client)
        return;
    bool isVisible = entries.last()->intersectionRatio() > 0.f;
    m_client->onVisibilityChanged(isVisible);
}

ExecutionContext* ElementVisibilityObserver::getExecutionContext() const
{
    if (!m_client)
        return nullptr;
    return m_client->getElementVisibilityExecutionContext();
}

DEFINE_TRACE(ElementVisibilityObserver)
{
    visitor->trace(m_client);
    visitor->trace(m_intersectionObserver);
    IntersectionObserverCallback::trace(visitor);
}

} // namespace blink
