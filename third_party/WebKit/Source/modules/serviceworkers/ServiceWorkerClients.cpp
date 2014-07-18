// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/serviceworkers/ServiceWorkerClients.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "modules/serviceworkers/Client.h"
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
        static WillBeHeapVector<RefPtrWillBeMember<Client> > from(ScriptPromiseResolver*, WebType* webClientsRaw)
        {
            OwnPtr<WebType> webClients = adoptPtr(webClientsRaw);
            WillBeHeapVector<RefPtrWillBeMember<Client> > clients;
            for (size_t i = 0; i < webClients->clientIDs.size(); ++i) {
                clients.append(Client::create(webClients->clientIDs[i]));
            }
            return clients;
        }

    private:
        WTF_MAKE_NONCOPYABLE(ClientArray);
        ClientArray() WTF_DELETED_FUNCTION;
    };

} // namespace

PassRefPtrWillBeRawPtr<ServiceWorkerClients> ServiceWorkerClients::create()
{
    return adoptRefWillBeNoop(new ServiceWorkerClients());
}

ServiceWorkerClients::ServiceWorkerClients()
{
    ScriptWrappable::init(this);
}

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(ServiceWorkerClients);

ScriptPromise ServiceWorkerClients::getServiced(ScriptState* scriptState)
{
    RefPtr<ScriptPromiseResolver> resolver = ScriptPromiseResolver::create(scriptState);
    ServiceWorkerGlobalScopeClient::from(scriptState->executionContext())->getClients(new CallbackPromiseAdapter<ClientArray, ServiceWorkerError>(resolver));
    return resolver->promise();
}

} // namespace blink
