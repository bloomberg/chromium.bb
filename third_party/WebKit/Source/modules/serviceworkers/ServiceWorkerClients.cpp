// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/ServiceWorkerClients.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/serviceworkers/ServiceWorkerClient.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "public/platform/WebServiceWorkerClientsInfo.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace blink {

namespace {

    class ClientArray {
    public:
        typedef blink::WebServiceWorkerClientsInfo WebType;
        static HeapVector<Member<ServiceWorkerClient> > take(ScriptPromiseResolver*, WebType* webClientsRaw)
        {
            OwnPtr<WebType> webClients = adoptPtr(webClientsRaw);
            HeapVector<Member<ServiceWorkerClient> > clients;
            for (size_t i = 0; i < webClients->clientIDs.size(); ++i) {
                clients.append(ServiceWorkerClient::create(webClients->clientIDs[i]));
            }
            return clients;
        }
        static void dispose(WebType* webClientsRaw)
        {
            delete webClientsRaw;
        }

    private:
        WTF_MAKE_NONCOPYABLE(ClientArray);
        ClientArray() WTF_DELETED_FUNCTION;
    };

} // namespace

ServiceWorkerClients* ServiceWorkerClients::create()
{
    return new ServiceWorkerClients();
}

ServiceWorkerClients::ServiceWorkerClients()
{
}

ScriptPromise ServiceWorkerClients::getAll(ScriptState* scriptState, const ServiceWorkerClientQueryParams& options)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ScriptPromise promise = resolver->promise();

    if (options.hasIncludeUncontrolled() && options.includeUncontrolled()) {
        // FIXME: Currently we don't support includeUncontrolled=true.
        resolver->reject(DOMException::create(NotSupportedError, "includeUncontrolled parameter of getAll is not supported."));
        return promise;
    }

    ServiceWorkerGlobalScopeClient::from(scriptState->executionContext())->getClients(new CallbackPromiseAdapter<ClientArray, ServiceWorkerError>(resolver));
    return promise;
}

} // namespace blink
