// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/worklet/DOMWindowWorklet.h"

#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "modules/worklet/Worklet.h"

namespace blink {

DOMWindowWorklet::DOMWindowWorklet(LocalDOMWindow& window)
    : DOMWindowProperty(window.frame())
{
}

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(DOMWindowWorklet);

const char* DOMWindowWorklet::supplementName()
{
    return "DOMWindowWorklet";
}

// static
DOMWindowWorklet& DOMWindowWorklet::from(LocalDOMWindow& window)
{
    DOMWindowWorklet* supplement = static_cast<DOMWindowWorklet*>(WillBeHeapSupplement<LocalDOMWindow>::from(window, supplementName()));
    if (!supplement) {
        supplement = new DOMWindowWorklet(window);
        provideTo(window, supplementName(), adoptPtrWillBeNoop(supplement));
    }
    return *supplement;
}

// static
Worklet* DOMWindowWorklet::renderWorklet(ExecutionContext* executionContext, DOMWindow& window)
{
    return from(toLocalDOMWindow(window)).renderWorklet(executionContext);
}

Worklet* DOMWindowWorklet::renderWorklet(ExecutionContext* executionContext) const
{
    if (!m_renderWorklet && frame())
        m_renderWorklet = Worklet::create(executionContext);
    return m_renderWorklet.get();
}

DEFINE_TRACE(DOMWindowWorklet)
{
    visitor->trace(m_renderWorklet);
    WillBeHeapSupplement<LocalDOMWindow>::trace(visitor);
    DOMWindowProperty::trace(visitor);
}

} // namespace blink
