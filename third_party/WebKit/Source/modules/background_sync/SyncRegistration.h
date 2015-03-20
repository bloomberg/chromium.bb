// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SyncRegistration_h
#define SyncRegistration_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "modules/background_sync/SyncRegistrationOptions.h"
#include "platform/heap/Handle.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ServiceWorkerRegistration;
class ScriptPromiseResolver;
class ScriptState;
struct WebSyncRegistration;

class SyncRegistration final : public GarbageCollectedFinalized<SyncRegistration>, public ScriptWrappable {
    DEFINE_WRAPPERTYPEINFO();
public:
    static SyncRegistration* take(ScriptPromiseResolver*, WebSyncRegistration*, ServiceWorkerRegistration*);
    static void dispose(WebSyncRegistration* registrationRaw);

    virtual ~SyncRegistration();

    bool hasAllowOnBattery() const { return !m_allowOnBattery.isNull(); }
    bool allowOnBattery() const { return m_allowOnBattery.get(); }
    void setAllowOnBattery(bool value) { m_allowOnBattery = value; }

    bool hasId() const { return !m_id.isNull(); }
    String id() const { return m_id; }
    void setId(String value) { m_id = value; }

    bool hasIdleRequired() const { return !m_idleRequired.isNull(); }
    bool idleRequired() const { return m_idleRequired.get(); }
    void setIdleRequired(bool value) { m_idleRequired = value; }

    bool hasMaxDelay() const { return !m_maxDelay.isNull(); }
    unsigned long maxDelay() const { return m_maxDelay.get(); }
    void setMaxDelay(unsigned long value) { m_maxDelay = value; }

    bool hasMinDelay() const { return !m_minDelay.isNull(); }
    unsigned long minDelay() const { return m_minDelay.get(); }
    void setMinDelay(unsigned long value) { m_minDelay = value; }

    bool hasMinPeriod() const { return !m_minPeriod.isNull(); }
    unsigned long minPeriod() const { return m_minPeriod.get(); }
    void setMinPeriod(unsigned long value) { m_minPeriod = value; }

    bool hasMinRequiredNetwork() const { return !m_minRequiredNetwork.isNull(); }
    String minRequiredNetwork() const { return m_minRequiredNetwork; }
    void setMinRequiredNetwork(String value) { m_minRequiredNetwork = value; }

    ScriptPromise unregister(ScriptState*);

    DECLARE_TRACE();

private:
    SyncRegistration(const SyncRegistrationOptions&, ServiceWorkerRegistration*);

    Nullable<bool> m_allowOnBattery;
    String m_id;
    Nullable<bool> m_idleRequired;
    Nullable<unsigned long> m_maxDelay;
    Nullable<unsigned long> m_minDelay;
    Nullable<unsigned long> m_minPeriod;
    String m_minRequiredNetwork;

    Member<ServiceWorkerRegistration> m_serviceWorkerRegistration;
};

} // namespace blink

#endif // SyncRegistration_h
