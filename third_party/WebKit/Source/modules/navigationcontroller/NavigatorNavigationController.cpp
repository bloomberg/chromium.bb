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
#include "modules/navigationcontroller/NavigatorNavigationController.h"

#include "RuntimeEnabledFeatures.h"
#include "V8NavigationController.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "core/dom/Document.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoaderClient.h"
#include "core/page/Frame.h"
#include "core/page/Navigator.h"
#include "core/workers/SharedWorker.h"
#include "modules/navigationcontroller/CallbackPromiseAdapter.h"
#include "modules/navigationcontroller/NavigationController.h"
#include "public/platform/WebNavigationControllerRegistry.h"
#include "public/platform/WebString.h"
#include "public/platform/WebURL.h"

namespace WebCore {

NavigatorNavigationController::NavigatorNavigationController(Navigator* navigator)
    : m_navigator(navigator)
{
}

NavigatorNavigationController::~NavigatorNavigationController()
{
}

const char* NavigatorNavigationController::supplementName()
{
    return "NavigatorNavigationController";
}

NavigatorNavigationController* NavigatorNavigationController::from(Navigator* navigator)
{
    NavigatorNavigationController* supplement = static_cast<NavigatorNavigationController*>(Supplement<Navigator>::from(navigator, supplementName()));
    if (!supplement) {
        supplement = new NavigatorNavigationController(navigator);
        provideTo(navigator, supplementName(), adoptPtr(supplement));
    }
    return supplement;
}

ScriptPromise NavigatorNavigationController::registerController(ScriptExecutionContext* context, Navigator* navigator, const String& pattern, const String& url, ExceptionState& es)
{
    return from(navigator)->registerController(context, pattern, url, es);
}


ScriptPromise NavigatorNavigationController::registerController(ScriptExecutionContext* scriptExecutionContext, const String& pattern, const String& scriptSrc, ExceptionState& es)
{
    ASSERT(RuntimeEnabledFeatures::navigationControllerEnabled());
    FrameLoaderClient* client = m_navigator->frame()->loader()->client();
    // WTF? Surely there's a better way to resolve a url?
    KURL scriptUrl = m_navigator->frame()->document()->completeURL(scriptSrc);
    WebKit::WebNavigationControllerRegistry* peer = client->navigationControllerRegistry();
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptExecutionContext);

    if (peer)
        peer->registerController(pattern, scriptUrl, new CallbackPromiseAdapter(resolver));
    else
        resolver->reject(PassRefPtr<NavigationController>(0));
    // call here?
    return resolver->promise();
}

ScriptPromise NavigatorNavigationController::unregisterController(ScriptExecutionContext* context, Navigator* navigator, const String& pattern, ExceptionState& es)
{
    return from(navigator)->unregisterController(context, pattern, es);
}

ScriptPromise NavigatorNavigationController::unregisterController(ScriptExecutionContext* scriptExecutionContext, const String& pattern, ExceptionState& es)
{
    ASSERT(RuntimeEnabledFeatures::navigationControllerEnabled());
    FrameLoaderClient* client = m_navigator->frame()->loader()->client();
    WebKit::WebNavigationControllerRegistry* peer = client->navigationControllerRegistry();
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptExecutionContext);
    if (peer)
        peer->unregisterController(pattern, new CallbackPromiseAdapter(resolver));
    else
        resolver->reject(PassRefPtr<NavigationController>(0));
    return resolver->promise();
}

} // namespace WebCore
