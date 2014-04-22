// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/ServiceWorkerClients.h"

#include "bindings/v8/CallbackPromiseAdapter.h"
#include "bindings/v8/NewScriptState.h"
#include "bindings/v8/ScriptPromise.h"
#include "bindings/v8/ScriptPromiseResolver.h"
#include "bindings/v8/ScriptPromiseResolverWithContext.h"
#include "modules/serviceworkers/Client.h"
#include "modules/serviceworkers/ServiceWorkerError.h"
#include "modules/serviceworkers/ServiceWorkerGlobalScopeClient.h"
#include "public/platform/WebServiceWorkerClientsInfo.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"

namespace WebCore {

namespace {

    class ClientArray {
    public:
        typedef blink::WebServiceWorkerClientsInfo WebType;
        static Vector<RefPtr<Client> > from(NewScriptState*, WebType* webClients)
        {
            Vector<RefPtr<Client> > clients;
            for (size_t i = 0; i < webClients->clientIDs.size(); ++i) {
                clients.append(Client::create(webClients->clientIDs[i]));
            }
            return clients;
        }

    private:
        ClientArray();
    };

} // namespace

PassRefPtr<ServiceWorkerClients> ServiceWorkerClients::create()
{
    return adoptRef(new ServiceWorkerClients());
}

ServiceWorkerClients::ServiceWorkerClients()
{
    ScriptWrappable::init(this);
}

ServiceWorkerClients::~ServiceWorkerClients()
{
}

ScriptPromise ServiceWorkerClients::getServiced(ExecutionContext* context)
{
    RefPtr<ScriptPromiseResolverWithContext> resolver = ScriptPromiseResolverWithContext::create(NewScriptState::current(toIsolate(context)));
    ServiceWorkerGlobalScopeClient::from(context)->getClients(new CallbackPromiseAdapter<ClientArray, ServiceWorkerError>(resolver));
    return resolver->promise();
}

} // namespace WebCore
