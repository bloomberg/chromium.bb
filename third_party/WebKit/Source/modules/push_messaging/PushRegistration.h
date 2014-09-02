// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushRegistration_h
#define PushRegistration_h

#include "bindings/core/v8/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "public/platform/WebPushRegistration.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ScriptPromiseResolver;

class PushRegistration FINAL : public GarbageCollectedFinalized<PushRegistration>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    // For CallbackPromiseAdapter.
    typedef WebPushRegistration WebType;
    static PushRegistration* take(ScriptPromiseResolver*, WebType* registrationRaw);
    static void dispose(WebType* registrationRaw);

    virtual ~PushRegistration();

    const String& pushEndpoint() const { return m_pushEndpoint; }
    const String& pushRegistrationId() const { return m_pushRegistrationId; }

    void trace(Visitor*) { }

private:
    PushRegistration(const String& pushEndpoint, const String& pushRegistrationId);

    String m_pushEndpoint;
    String m_pushRegistrationId;
};

} // namespace blink

#endif // PushRegistration_h
