// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushRegistration_h
#define PushRegistration_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ServiceWorkerRegistration;
class ScriptPromiseResolver;
class ScriptState;
struct WebPushRegistration;

class PushRegistration final : public GarbageCollectedFinalized<PushRegistration>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PushRegistration* take(ScriptPromiseResolver*, WebPushRegistration*, ServiceWorkerRegistration*);
    static void dispose(WebPushRegistration* registrationRaw);

    virtual ~PushRegistration();

    const String& pushEndpoint() const { return m_pushEndpoint; }
    const String& pushRegistrationId() const { return m_pushRegistrationId; }
    ScriptPromise unregister(ScriptState*);

    void trace(Visitor*);

private:
    PushRegistration(const String& pushEndpoint, const String& pushRegistrationId, ServiceWorkerRegistration*);

    String m_pushEndpoint;
    String m_pushRegistrationId;
    Member<ServiceWorkerRegistration> m_serviceWorkerRegistration;
};

} // namespace blink

#endif // PushRegistration_h
