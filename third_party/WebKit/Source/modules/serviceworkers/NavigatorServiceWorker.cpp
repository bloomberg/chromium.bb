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
#include "modules/serviceworkers/NavigatorServiceWorker.h"

#include "RuntimeEnabledFeatures.h"
#include "V8ServiceWorker.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "core/dom/Document.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/Frame.h"
#include "core/page/Navigator.h"
#include "core/workers/SharedWorker.h"
#include "modules/serviceworkers/CallbackPromiseAdapter.h"
#include "modules/serviceworkers/ServiceWorker.h"
#include "public/platform/WebServiceWorkerRegistry.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

namespace WebCore {

NavigatorServiceWorker::NavigatorServiceWorker(Navigator* navigator)
    : m_navigator(navigator)
{
}

NavigatorServiceWorker::~NavigatorServiceWorker()
{
}

const char* NavigatorServiceWorker::supplementName()
{
    return "NavigatorServiceWorker";
}

NavigatorServiceWorker* NavigatorServiceWorker::from(Navigator* navigator)
{
    NavigatorServiceWorker* supplement = toNavigatorServiceWorker(navigator);
    if (!supplement) {
        supplement = new NavigatorServiceWorker(navigator);
        provideTo(navigator, supplementName(), adoptPtr(supplement));
    }
    return supplement;
}

ScriptPromise NavigatorServiceWorker::registerServiceWorker(ScriptExecutionContext* context, Navigator* navigator, const String& pattern, const String& url, ExceptionState& es)
{
    return from(navigator)->registerServiceWorker(context, pattern, url, es);
}


ScriptPromise NavigatorServiceWorker::registerServiceWorker(ScriptExecutionContext* scriptExecutionContext, const String& pattern, const String& scriptSrc, ExceptionState& es)
{
    ASSERT(RuntimeEnabledFeatures::serviceWorkerEnabled());
    FrameLoaderClient* client = m_navigator->frame()->loader()->client();
    // WTF? Surely there's a better way to resolve a url?
    KURL scriptUrl = m_navigator->frame()->document()->completeURL(scriptSrc);
    WebKit::WebServiceWorkerRegistry* peer = client->serviceWorkerRegistry();
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptExecutionContext);

    if (peer)
        peer->registerServiceWorker(pattern, scriptUrl, new CallbackPromiseAdapter(resolver));
    else
        resolver->reject(PassRefPtr<ServiceWorker>(0));
    // call here?
    return resolver->promise();
}

ScriptPromise NavigatorServiceWorker::unregisterServiceWorker(ScriptExecutionContext* context, Navigator* navigator, const String& pattern, ExceptionState& es)
{
    return from(navigator)->unregisterServiceWorker(context, pattern, es);
}

ScriptPromise NavigatorServiceWorker::unregisterServiceWorker(ScriptExecutionContext* scriptExecutionContext, const String& pattern, ExceptionState& es)
{
    ASSERT(RuntimeEnabledFeatures::serviceWorkerEnabled());
    FrameLoaderClient* client = m_navigator->frame()->loader()->client();
    WebKit::WebServiceWorkerRegistry* peer = client->serviceWorkerRegistry();
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptExecutionContext);
    if (peer)
        peer->unregisterServiceWorker(pattern, new CallbackPromiseAdapter(resolver));
    else
        resolver->reject(PassRefPtr<ServiceWorker>(0));
    return resolver->promise();
}

} // namespace WebCore
