// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/presentation/NavigatorPresentation.h"

#include "core/frame/Navigator.h"
#include "modules/presentation/Presentation.h"

namespace blink {

NavigatorPresentation::NavigatorPresentation()
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
        supplement = new NavigatorPresentation();
        provideTo(navigator, supplementName(), adoptPtrWillBeNoop(supplement));
    }
    return *supplement;
}

// static
Presentation* NavigatorPresentation::presentation(Navigator& navigator)
{
    NavigatorPresentation& self = NavigatorPresentation::from(navigator);
    if (!self.m_presentation) {
        if (!navigator.frame())
            return nullptr;
        self.m_presentation = Presentation::create(navigator.frame());
    }
    return self.m_presentation.get();
}

DEFINE_TRACE(NavigatorPresentation)
{
    visitor->trace(m_presentation);
    WillBeHeapSupplement<Navigator>::trace(visitor);
}

} // namespace blink
