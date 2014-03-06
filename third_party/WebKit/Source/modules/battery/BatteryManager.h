// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BatteryManager_h
#define BatteryManager_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/Document.h"
#include "core/events/EventTarget.h"
#include "modules/battery/BatteryStatus.h"

namespace WebCore {

class Navigator;

class BatteryManager : public ContextLifecycleObserver, public RefCounted<BatteryManager>, public EventTarget {
public:
    virtual ~BatteryManager();
    static PassRefPtr<BatteryManager> create(ExecutionContext*);

    // EventTarget implementation.
    virtual const WTF::AtomicString& interfaceName() const { return EventTargetNames::BatteryManager; }
    virtual ExecutionContext* executionContext() const OVERRIDE FINAL { return ContextLifecycleObserver::executionContext(); }

    bool charging();
    double chargingTime();
    double dischargingTime();
    double level();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(chargingchange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(chargingtimechange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dischargingtimechange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(levelchange);

    void didChangeBatteryStatus(PassRefPtr<Event>, PassOwnPtr<BatteryStatus>);

    using RefCounted<BatteryManager>::ref;
    using RefCounted<BatteryManager>::deref;

protected:
    virtual EventTargetData* eventTargetData() { return &m_eventTargetData; }
    virtual EventTargetData& ensureEventTargetData() { return m_eventTargetData; }

private:
    explicit BatteryManager(ExecutionContext*);

    // EventTarget implementation.
    virtual void refEventTarget() { ref(); }
    virtual void derefEventTarget() { deref(); }

    EventTargetData m_eventTargetData;
    OwnPtr<BatteryStatus> m_batteryStatus;
};

}

#endif // BatteryManager_h

