// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/dom/ElementFullscreen.h"

#include "core/dom/FullscreenElementStack.h"

namespace blink {

void ElementFullscreen::webkitRequestFullscreen(Element& element)
{
    FullscreenElementStack::from(element.document()).requestFullscreen(element, FullscreenElementStack::PrefixedRequest);
}

void ElementFullscreen::webkitRequestFullScreen(Element& element, unsigned short flags)
{
    FullscreenElementStack::RequestType requestType;
    if (flags & ALLOW_KEYBOARD_INPUT)
        requestType = FullscreenElementStack::PrefixedMozillaAllowKeyboardInputRequest;
    else
        requestType = FullscreenElementStack::PrefixedMozillaRequest;
    FullscreenElementStack::from(element.document()).requestFullscreen(element, requestType);
}

} // namespace blink
