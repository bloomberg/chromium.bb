// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerClients_h
#define ServiceWorkerClients_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/serviceworkers/ServiceWorkerClientQueryOptions.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebServiceWorkerClientsInfo.h"
#include "wtf/Forward.h"

namespace blink {

class ExecutionContext;
class ScriptPromise;
class ScriptState;
class ServiceWorkerClient;

class ServiceWorkerClients final : public GarbageCollected<ServiceWorkerClients>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static ServiceWorkerClients* create();

    // ServiceWorkerClients.idl
    ScriptPromise getAll(ScriptState*, const ServiceWorkerClientQueryOptions&);

    void trace(Visitor*) { }

private:
    ServiceWorkerClients();
};

} // namespace blink

#endif // ServiceWorkerClients_h
