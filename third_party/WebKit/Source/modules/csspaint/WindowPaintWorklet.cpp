// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/csspaint/WindowPaintWorklet.h"

#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "modules/csspaint/PaintWorklet.h"

namespace blink {

WindowPaintWorklet::WindowPaintWorklet(LocalDOMWindow& window)
    : DOMWindowProperty(window.frame())
{
}

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(WindowPaintWorklet);

const char* WindowPaintWorklet::supplementName()
{
    return "WindowPaintWorklet";
}

// static
WindowPaintWorklet& WindowPaintWorklet::from(LocalDOMWindow& window)
{
    WindowPaintWorklet* supplement = static_cast<WindowPaintWorklet*>(WillBeHeapSupplement<LocalDOMWindow>::from(window, supplementName()));
    if (!supplement) {
        supplement = new WindowPaintWorklet(window);
        provideTo(window, supplementName(), adoptPtrWillBeNoop(supplement));
    }
    return *supplement;
}

// static
Worklet* WindowPaintWorklet::paintWorklet(ExecutionContext* executionContext, DOMWindow& window)
{
    return from(toLocalDOMWindow(window)).paintWorklet(executionContext);
}

PaintWorklet* WindowPaintWorklet::paintWorklet(ExecutionContext* executionContext) const
{
    if (!m_paintWorklet && frame())
        m_paintWorklet = PaintWorklet::create(frame(), executionContext);
    return m_paintWorklet.get();
}

DEFINE_TRACE(WindowPaintWorklet)
{
    visitor->trace(m_paintWorklet);
    WillBeHeapSupplement<LocalDOMWindow>::trace(visitor);
    DOMWindowProperty::trace(visitor);
}

} // namespace blink
