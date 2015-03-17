// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ServiceWorkerClient_h
#define ServiceWorkerClient_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "bindings/core/v8/SerializedScriptValue.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebServiceWorkerClientsInfo.h"
#include "wtf/Forward.h"

namespace blink {

class ExecutionContext;
class ScriptState;

class ServiceWorkerClient : public GarbageCollectedFinalized<ServiceWorkerClient>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static ServiceWorkerClient* create(const WebServiceWorkerClientInfo&);

    virtual ~ServiceWorkerClient();

    // Client.idl
    String url() const { return m_url; }
    void postMessage(ExecutionContext*, PassRefPtr<SerializedScriptValue> message, const MessagePortArray*, ExceptionState&);

    DEFINE_INLINE_VIRTUAL_TRACE() { }

protected:
    explicit ServiceWorkerClient(const WebServiceWorkerClientInfo&);

    String uuid() const { return m_uuid; }

private:
    String m_uuid;
    String m_url;
};

} // namespace blink

#endif // ServiceWorkerClient_h
