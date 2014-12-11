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

class ServiceWorkerClient final : public GarbageCollectedFinalized<ServiceWorkerClient>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static ServiceWorkerClient* create(const WebServiceWorkerClientInfo&);

    ~ServiceWorkerClient();

    // ServiceWorkerClient.idl
    String visibilityState() const { return m_visibilityState; }
    bool focused() const { return m_isFocused; }
    String url() const { return m_url; }
    String frameType() const;
    void postMessage(ExecutionContext*, PassRefPtr<SerializedScriptValue> message, const MessagePortArray*, ExceptionState&);
    ScriptPromise focus(ScriptState*);

    void trace(Visitor*) { }

private:
    explicit ServiceWorkerClient(const WebServiceWorkerClientInfo&);

    unsigned m_id;
    String m_visibilityState;
    bool m_isFocused;
    String m_url;
    WebURLRequest::FrameType m_frameType;
};

} // namespace blink

#endif // ServiceWorkerClient_h
