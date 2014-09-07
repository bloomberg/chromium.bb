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
#include "modules/push_messaging/PushRegistration.h"
#include "modules/serviceworkers/NavigatorServiceWorker.h"
#include "modules/serviceworkers/ServiceWorkerContainer.h"
#include "public/platform/WebPushClient.h"
#include "wtf/RefPtr.h"

namespace blink {

PushManager::PushManager()
{
}

ScriptPromise PushManager::registerPushMessaging(ScriptState* scriptState, const String& senderId)
{
    ASSERT(scriptState->executionContext()->isDocument());

    Document* document = toDocument(scriptState->executionContext());
    if (!document->domWindow() || !document->page())
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(AbortError, "Document is detached from window."));

    WebServiceWorkerProvider* serviceWorkerProvider = NavigatorServiceWorker::serviceWorker(document->domWindow()->navigator())->provider();
    if (!serviceWorkerProvider)
        return ScriptPromise::rejectWithDOMException(scriptState, DOMException::create(AbortError, "No Service Worker installed for this document."));

    WebPushClient* client = PushController::clientFrom(document->page());
    ASSERT(client);

    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    client->registerPushMessaging(senderId, new CallbackPromiseAdapter<PushRegistration, PushError>(resolver), serviceWorkerProvider);
    return promise;
}

} // namespace blink
