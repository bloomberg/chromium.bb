// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/ServiceWorkerClients.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/ExceptionCode.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerLocation.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "modules/serviceworkers/ServiceWorkerWindowClient.h"
#include "public/platform/WebServiceWorkerClientQueryOptions.h"
#include "public/platform/WebServiceWorkerClientsInfo.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace blink {

namespace {

class ClientArray {
public:
    typedef blink::WebServiceWorkerClientsInfo WebType;
    static HeapVector<Member<ServiceWorkerClient>> take(ScriptPromiseResolver*, WebType* webClientsRaw)
    {
        OwnPtr<WebType> webClients = adoptPtr(webClientsRaw);
        HeapVector<Member<ServiceWorkerClient>> clients;
        for (size_t i = 0; i < webClients->clients.size(); ++i) {
            // FIXME: For now we only support getting "window" type clients.
            ASSERT(webClients->clients[i].clientType == WebServiceWorkerClientTypeWindow);
            clients.append(ServiceWorkerWindowClient::create(webClients->clients[i]));
        }
        return clients;
    }
    static void dispose(WebType* webClientsRaw)
    {
        delete webClientsRaw;
    }

private:
    WTF_MAKE_NONCOPYABLE(ClientArray);
    ClientArray() = delete;
};

WebServiceWorkerClientType getClientType(const String& type)
{
    if (type == "window")
        return WebServiceWorkerClientTypeWindow;
    if (type == "worker")
        return WebServiceWorkerClientTypeWorker;
    if (type == "sharedworker")
        return WebServiceWorkerClientTypeSharedWorker;
    if (type == "all")
        return WebServiceWorkerClientTypeAll;
    ASSERT_NOT_REACHED();
    return WebServiceWorkerClientTypeWindow;
}

} // namespace

ServiceWorkerClients* ServiceWorkerClients::create()
{
    return new ServiceWorkerClients();
}

ServiceWorkerClients::ServiceWorkerClients()
{
}

ScriptPromise ServiceWorkerClients::matchAll(ScriptState* scriptState, const ClientQueryOptions& options)
{
    ExecutionContext* executionContext = scriptState->executionContext();
    // FIXME: May be null due to worker termination: http://crbug.com/413518.
    if (!executionContext)
        return ScriptPromise();

    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (options.includeUncontrolled()) {
        // FIXME: Remove this when query options are supported in the embedder.
        resolver->reject(DOMException::create(NotSupportedError, "includeUncontrolled parameter of getAll is not supported."));
        return promise;
    }

    if (options.type() != "window") {
        // FIXME: Remove this when query options are supported in the embedder.
        resolver->reject(DOMException::create(NotSupportedError, "type parameter of getAll is not supported."));
        return promise;
    }

    WebServiceWorkerClientQueryOptions webOptions;
    webOptions.clientType = getClientType(options.type());
    webOptions.includeUncontrolled = options.includeUncontrolled();
    ServiceWorkerGlobalScopeClient::from(executionContext)->getClients(webOptions, new CallbackPromiseAdapter<ClientArray, ServiceWorkerError>(resolver));
    return promise;
}

ScriptPromise ServiceWorkerClients::claim(ScriptState* scriptState)
{
    ExecutionContext* executionContext = scriptState->executionContext();

    // FIXME: May be null due to worker termination: http://crbug.com/413518.
    if (!executionContext)
        return ScriptPromise();

    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    WebServiceWorkerClientsClaimCallbacks* callbacks = new CallbackPromiseAdapter<void, ServiceWorkerError>(resolver);
    ServiceWorkerGlobalScopeClient::from(executionContext)->claim(callbacks);
    return promise;
}

ScriptPromise ServiceWorkerClients::openWindow(ScriptState* scriptState, const String& url)
{
    RefPtrWillBeRawPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();
    ExecutionContext* context = scriptState->executionContext();

    KURL parsedUrl = KURL(toWorkerGlobalScope(context)->location()->url(), url);
    if (!parsedUrl.isValid()) {
        resolver->reject(DOMException::create(SyntaxError, "'" + url + "' is not a valid URL."));
        return promise;
    }

    if (!context->securityOrigin()->canRequest(parsedUrl)) {
        resolver->reject(DOMException::create(SecurityError, "'" + parsedUrl.elidedString() + "' is not same-origin with the Worker."));
        return promise;
    }

    if (!context->isWindowInteractionAllowed()) {
        resolver->reject(DOMException::create(InvalidAccessError, "Not allowed to open a window."));
        return promise;
    }
    context->consumeWindowInteraction();

    ServiceWorkerGlobalScopeClient::from(context)->openWindow(parsedUrl, new CallbackPromiseAdapter<ServiceWorkerWindowClient, ServiceWorkerError>(resolver));
    return promise;
}

} // namespace blink
