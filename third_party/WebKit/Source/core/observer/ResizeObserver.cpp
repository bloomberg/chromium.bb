// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/observer/ResizeObserver.h"

#include "core/dom/Element.h"
#include "core/frame/FrameView.h"
#include "core/observer/ResizeObservation.h"
#include "core/observer/ResizeObserverCallback.h"
#include "core/observer/ResizeObserverController.h"
#include "core/observer/ResizeObserverEntry.h"

namespace blink {

ResizeObserver* ResizeObserver::create(Document& document, ResizeObserverCallback* callback)
{
    return new ResizeObserver(callback, document);
}

ResizeObserver::ResizeObserver(ResizeObserverCallback* callback, Document& document)
    : m_callback(callback)
{
    m_controller = &document.ensureResizeObserverController();
    m_controller->addObserver(*this);
}

void ResizeObserver::observe(Element* target)
{
    auto& observerMap = target->ensureResizeObserverData();
    if (observerMap.contains(this))
        return; // Already registered.

    auto observation = new ResizeObservation(target, this);
    m_observations.add(observation);
    observerMap.set(this, observation);

    if (FrameView* frameView = target->document().view())
        frameView->scheduleAnimation();
}

void ResizeObserver::unobserve(Element* target)
{
    auto observerMap = target ? target->resizeObserverData() : nullptr;
    if (!observerMap)
        return;
    auto observation = observerMap->find(this);
    if (observation != observerMap->end()) {
        m_observations.remove((*observation).value);
        observerMap->remove(observation);
    }
}

void ResizeObserver::disconnect()
{
    ObservationList observations;
    m_observations.swap(observations);

    for (auto observation : observations) {
        Element* target = (*observation).target();
        if (target)
            target->ensureResizeObserverData().remove(this);
    }
}

DEFINE_TRACE(ResizeObserver)
{
    visitor->trace(m_callback);
    visitor->trace(m_observations);
    visitor->trace(m_controller);
}

} // namespace blink
