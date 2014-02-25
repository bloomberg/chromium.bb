/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include "config.h"
#include "modules/serviceworkers/ServiceWorkerContainer.h"

#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/CallbackPromiseAdapter.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "core/dom/ExecutionContext.h"
#include "modules/serviceworkers/RegistrationOptionList.h"
#include "modules/serviceworkers/ServiceWorker.h"
#include "modules/serviceworkers/ServiceWorkerContainerClient.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "public/platform/WebServiceWorkerProvider.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

using blink::WebServiceWorkerProvider;

namespace WebCore {

PassRefPtr<ServiceWorkerContainer> ServiceWorkerContainer::create()
{
    return adoptRef(new ServiceWorkerContainer());
}

ServiceWorkerContainer::~ServiceWorkerContainer()
{
}

void ServiceWorkerContainer::detachClient()
{
    if (m_provider) {
        m_provider->setClient(0);
        m_provider = 0;
    }
}

ScriptPromise ServiceWorkerContainer::registerServiceWorker(ExecutionContext* executionContext, const String& url, const Dictionary& dictionary)
{
    RegistrationOptionList options(dictionary);
    ASSERT(RuntimeEnabledFeatures::serviceWorkerEnabled());
    ScriptPromise promise = ScriptPromise::createPending(executionContext);
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(promise, executionContext);

    RefPtr<SecurityOrigin> documentOrigin = executionContext->securityOrigin();
    KURL patternURL = executionContext->completeURL(options.scope);
    if (!documentOrigin->canRequest(patternURL)) {
        resolver->reject(DOMError::create(SecurityError, "Can only register for patterns in the document's origin."));
        return promise;
    }

    KURL scriptURL = executionContext->completeURL(url);
    if (!documentOrigin->canRequest(scriptURL)) {
        resolver->reject(DOMError::create(SecurityError, "Script must be in document's origin."));
        return promise;
    }

    ensureProvider(executionContext)->registerServiceWorker(patternURL, scriptURL, new CallbackPromiseAdapter<ServiceWorker, ServiceWorkerError>(resolver, executionContext));
    return promise;
}

ScriptPromise ServiceWorkerContainer::unregisterServiceWorker(ExecutionContext* executionContext, const String& pattern)
{
    ASSERT(RuntimeEnabledFeatures::serviceWorkerEnabled());
    ScriptPromise promise = ScriptPromise::createPending(executionContext);
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(promise, executionContext);

    RefPtr<SecurityOrigin> documentOrigin = executionContext->securityOrigin();
    KURL patternURL = executionContext->completeURL(pattern);
    if (!pattern.isEmpty() && !documentOrigin->canRequest(patternURL)) {
        resolver->reject(DOMError::create(SecurityError, "Can only unregister for patterns in the document's origin."));

        return promise;
    }

    ensureProvider(executionContext)->unregisterServiceWorker(patternURL, new CallbackPromiseAdapter<ServiceWorker, ServiceWorkerError>(resolver, executionContext));
    return promise;
}

ServiceWorkerContainer::ServiceWorkerContainer()
    : m_provider(0)
{
    ScriptWrappable::init(this);
}

WebServiceWorkerProvider* ServiceWorkerContainer::ensureProvider(ExecutionContext* executionContext)
{
    if (!m_provider) {
        m_provider = ServiceWorkerContainerClient::from(executionContext)->provider();
        m_provider->setClient(this);
    }
    return m_provider;
}

} // namespace WebCore
