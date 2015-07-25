// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/app_banner/BeforeInstallPromptEvent.h"

#include "bindings/core/v8/ScriptPromise.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "modules/app_banner/BeforeInstallPromptEventInit.h"
#include "public/platform/modules/app_banner/WebAppBannerClient.h"

namespace blink {

BeforeInstallPromptEvent::BeforeInstallPromptEvent()
{
}

BeforeInstallPromptEvent::BeforeInstallPromptEvent(const AtomicString& name, ExecutionContext* executionContext, const Vector<String>& platforms, int requestId, WebAppBannerClient* client)
    : Event(name, false, true)
    , m_platforms(platforms)
    , m_requestId(requestId)
    , m_client(client)
    , m_userChoice(new UserChoiceProperty(executionContext, this, UserChoiceProperty::UserChoice))
    , m_prompt(new PromptProperty(executionContext, this, PromptProperty::Prompt))
{
}

BeforeInstallPromptEvent::BeforeInstallPromptEvent(const AtomicString& name, const BeforeInstallPromptEventInit& init)
    : Event(name, false, true)
    , m_requestId(-1)
    , m_client(nullptr)
{
    if (init.hasPlatforms())
        m_platforms = init.platforms();
}

BeforeInstallPromptEvent::~BeforeInstallPromptEvent()
{
}

Vector<String> BeforeInstallPromptEvent::platforms() const
{
    return m_platforms;
}

ScriptPromise BeforeInstallPromptEvent::userChoice(ScriptState* scriptState)
{
    if (m_userChoice)
        return m_userChoice->promise(scriptState->world());
    return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, "The prompt() method cannot be called on this event."));
}

const AtomicString& BeforeInstallPromptEvent::interfaceName() const
{
    return EventNames::BeforeInstallPromptEvent;
}

ScriptPromise BeforeInstallPromptEvent::prompt(ScriptState* scriptState)
{
    if (!m_prompt)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, "The prompt() method cannot be called on this event."));

    if (m_prompt->state() != PromptProperty::Pending)
        return m_prompt->promise(scriptState->world());

    if (!m_client || !defaultPrevented()) {
        m_prompt->reject(DOMException::create(InvalidStateError, "The prompt() method may only be called once, following preventDefault()."));
    } else {
        ASSERT(m_requestId != -1);
        m_prompt->resolve(ToV8UndefinedGenerator());
        m_client->showAppBanner(m_requestId);
    }

    return m_prompt->promise(scriptState->world());
}

DEFINE_TRACE(BeforeInstallPromptEvent)
{
    visitor->trace(m_userChoice);
    visitor->trace(m_prompt);
    Event::trace(visitor);
}

} // namespace blink
