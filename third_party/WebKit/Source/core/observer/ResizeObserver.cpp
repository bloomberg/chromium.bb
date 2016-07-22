// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/observer/ResizeObserver.h"

#include "core/dom/Element.h"
#include "core/observer/ResizeObservation.h"
#include "core/observer/ResizeObserverCallback.h"
#include "core/observer/ResizeObserverController.h"

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
}

void ResizeObserver::unobserve(Element* target)
{
}

void ResizeObserver::disconnect()
{
}

DEFINE_TRACE(ResizeObserver)
{
    visitor->trace(m_callback);
    visitor->trace(m_observations);
    visitor->trace(m_controller);
}

} // namespace blink
