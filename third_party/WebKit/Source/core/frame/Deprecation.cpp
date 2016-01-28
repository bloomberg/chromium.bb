// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/Deprecation.h"

#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"

namespace blink {

Deprecation::Deprecation()
{
    m_cssPropertyDeprecationBits.ensureSize(lastUnresolvedCSSProperty + 1);
    m_cssPropertyDeprecationBits.clearAll();
}

Deprecation::~Deprecation()
{
    m_cssPropertyDeprecationBits.clearAll();
}

void Deprecation::suppress(CSSPropertyID unresolvedProperty)
{
    ASSERT(unresolvedProperty >= firstCSSProperty);
    ASSERT(unresolvedProperty <= lastUnresolvedCSSProperty);
    m_cssPropertyDeprecationBits.quickSet(unresolvedProperty);
}
bool Deprecation::isSuppressed(CSSPropertyID unresolvedProperty)
{
    ASSERT(unresolvedProperty >= firstCSSProperty);
    ASSERT(unresolvedProperty <= lastUnresolvedCSSProperty);
    return m_cssPropertyDeprecationBits.quickGet(unresolvedProperty);
}

void Deprecation::warnOnDeprecatedProperties(const LocalFrame* frame, CSSPropertyID unresolvedProperty)
{
    FrameHost* host = frame ? frame->host() : nullptr;
    if (!host || host->deprecation().isSuppressed(unresolvedProperty)) {
        return;
    }

    String message = deprecationMessage(unresolvedProperty);
    if (!message.isEmpty()) {
        host->deprecation().suppress(unresolvedProperty);
        frame->console().addMessage(ConsoleMessage::create(DeprecationMessageSource, WarningMessageLevel, message));
    }
}

String Deprecation::deprecationMessage(CSSPropertyID unresolvedProperty)
{
    switch (unresolvedProperty) {
    case CSSPropertyWebkitBackgroundComposite:
        return UseCounter::willBeRemoved("'-webkit-background-composite'", 51, "6607299456008192");
    default:
        return emptyString();
    }
}

} // namespace blink
