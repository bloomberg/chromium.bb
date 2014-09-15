// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/presentation/NavigatorPresentation.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Navigator.h"
#include "modules/presentation/Presentation.h"
#include "platform/heap/Handle.h"

namespace blink {

NavigatorPresentation::NavigatorPresentation(LocalFrame* frame)
    : DOMWindowProperty(frame)
{
}

NavigatorPresentation::~NavigatorPresentation()
{
}

// static
const char* NavigatorPresentation::supplementName()
{
    return "NavigatorPresentation";
}

// static
NavigatorPresentation& NavigatorPresentation::from(Navigator& navigator)
{
    NavigatorPresentation* supplement = static_cast<NavigatorPresentation*>(WillBeHeapSupplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        supplement = new NavigatorPresentation(navigator.frame());
        provideTo(navigator, supplementName(), adoptPtrWillBeNoop(supplement));
    }
    return *supplement;
}

// static
Presentation* NavigatorPresentation::presentation(Navigator& navigator)
{
    return NavigatorPresentation::from(navigator).presentation();
}

Presentation* NavigatorPresentation::presentation()
{
    if (!m_presentation) {
        if (!frame())
            return 0;
        m_presentation = Presentation::create(frame()->document());
    }
    return m_presentation.get();
}

void NavigatorPresentation::trace(Visitor* visitor)
{
    visitor->trace(m_presentation);
    WillBeHeapSupplement<Navigator>::trace(visitor);
    DOMWindowProperty::trace(visitor);
}

} // namespace blink
