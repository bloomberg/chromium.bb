// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/app_banner/BeforeInstallPromptEvent.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/app_banner/BeforeInstallPromptEventInit.h"

namespace blink {

BeforeInstallPromptEvent::BeforeInstallPromptEvent()
{
}

BeforeInstallPromptEvent::BeforeInstallPromptEvent(const AtomicString& name, const String& platform)
    : Event(name, false, true)
    , m_platform(platform)
{
}

BeforeInstallPromptEvent::BeforeInstallPromptEvent(const AtomicString& name, const BeforeInstallPromptEventInit& init)
    : Event(name, false, true)
    , m_platform(init.platform())
{
}

BeforeInstallPromptEvent::~BeforeInstallPromptEvent()
{
}

String BeforeInstallPromptEvent::platform() const
{
    return m_platform;
}

ScriptPromise BeforeInstallPromptEvent::userChoice(ScriptState* scriptState) const
{
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(NotSupportedError, "Subscription failed - no active Service Worker"));
}

const AtomicString& BeforeInstallPromptEvent::interfaceName() const
{
    return EventNames::BeforeInstallPromptEvent;
}

} // namespace blink
