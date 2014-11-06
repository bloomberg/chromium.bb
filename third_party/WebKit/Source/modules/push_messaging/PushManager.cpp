// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/push_messaging/PushManager.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "modules/push_messaging/PushController.h"
#include "modules/push_messaging/PushError.h"
#include "modules/push_messaging/PushPermissionClient.h"
#include "modules/push_messaging/PushPermissionRequestCallback.h"
#include "modules/push_messaging/PushPermissionStatusCallback.h"
#include "modules/push_messaging/PushRegistration.h"
#include "modules/serviceworkers/NavigatorServiceWorker.h"
#include "modules/serviceworkers/ServiceWorkerContainer.h"
#include "public/platform/WebPushClient.h"
#include "wtf/RefPtr.h"

namespace blink {

PushManager::PushManager()
{
}

// FIXME: This call should be available from workers which will not have a Document object available.
// See crbug.com/389194
ScriptPromise PushManager::registerPushMessaging(ScriptState* scriptState)
{
    ASSERT(scriptState->executionContext()->isDocument());

    Document* document = toDocument(scriptState->executionContext());
    if (!document->domWindow() || !document->page())
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(AbortError, "Document is detached from window."));

    WebServiceWorkerProvider* serviceWorkerProvider = NavigatorServiceWorker::serviceWorker(*document->domWindow()->navigator())->provider();
    if (!serviceWorkerProvider)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(AbortError, "No Service Worker installed for this document."));

    // FIXME: Once everything except permission request goes through platform,
    // delete WebPushClient and usage such as this one.
    // See crbug.com/389194
    WebPushClient* client = PushController::clientFrom(document->page());
    ASSERT(client);

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    PushPermissionClient* permissionClient = PushPermissionClient::from(scriptState->executionContext());
    if (permissionClient)
        permissionClient->requestPermission(scriptState->executionContext(), new PushPermissionRequestCallback(this, client, resolver, serviceWorkerProvider));
    else
        doRegister(client, resolver, serviceWorkerProvider);

    return promise;
}

// FIXME: This call should be available from workers which will not have a Document object available.
// See crbug.com/389194
ScriptPromise PushManager::hasPermission(ScriptState* scriptState)
{
    ASSERT(scriptState->executionContext()->isDocument());

    Document* document = toDocument(scriptState->executionContext());
    if (!document->domWindow() || !document->page())
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, "Document is detached from window."));
    blink::WebPushClient* client = PushController::clientFrom(document->page());
    ASSERT(client);

    // The currently implemented specification does not require a Service Worker to be present for the
    // hasPermission() call to work, but it will become a requirement soon.
    WebServiceWorkerProvider* serviceWorkerProvider = NavigatorServiceWorker::serviceWorker(*document->domWindow()->navigator())->provider();
    if (!serviceWorkerProvider)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(InvalidStateError, "No Service Worker installed for this document."));

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);

    client->getPermissionStatus(new PushPermissionStatusCallback(resolver), serviceWorkerProvider);
    return resolver->promise();
}

void PushManager::doRegister(WebPushClient* client, PassRefPtr<ScriptPromiseResolver> resolver, WebServiceWorkerProvider* serviceWorkerProvider)
{
    client->registerPushMessaging(new CallbackPromiseAdapter<PushRegistration, PushError>(resolver), serviceWorkerProvider);
}

} // namespace blink
