// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/app_banner/BeforeInstallPromptEvent.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/app_banner/AppBannerPromptResult.h"
#include "modules/app_banner/BeforeInstallPromptEventInit.h"
#include "public/platform/modules/app_banner/WebAppBannerClient.h"

namespace blink {

BeforeInstallPromptEvent::BeforeInstallPromptEvent()
{
}

BeforeInstallPromptEvent::BeforeInstallPromptEvent(const AtomicString& name, const Vector<String>& platforms, int requestId, WebAppBannerClient* client)
    : Event(name, false, true)
    , m_platforms(platforms)
    , m_requestId(requestId)
    , m_client(client)
{
}

BeforeInstallPromptEvent::BeforeInstallPromptEvent(const AtomicString& name, const BeforeInstallPromptEventInit& init)
    : Event(name, false, true)
    , m_platforms(init.platforms())
    , m_requestId(-1)
    , m_client(nullptr)
{
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
    if (m_userChoice.isEmpty() && m_client) {
        ASSERT(m_requestId != -1);
        RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
        m_userChoice = resolver->promise();
        m_client->registerBannerCallbacks(m_requestId, new CallbackPromiseAdapter<AppBannerPromptResult, void>(resolver));
    }

    return m_userChoice;
}

const AtomicString& BeforeInstallPromptEvent::interfaceName() const
{
    return EventNames::BeforeInstallPromptEvent;
}

} // namespace blink
