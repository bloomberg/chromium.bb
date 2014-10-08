// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BatteryManager_h
#define BatteryManager_h

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/frame/PlatformEventController.h"
#include "modules/EventTargetModules.h"
#include "platform/heap/Handle.h"

namespace blink {

class BatteryStatus;

class BatteryManager final : public RefCountedGarbageCollectedWillBeGarbageCollectedFinalized<BatteryManager>, public ActiveDOMObject, public PlatformEventController, public EventTargetWithInlineData {
    DEFINE_EVENT_TARGET_REFCOUNTING_WILL_BE_REMOVED(RefCountedGarbageCollected<BatteryManager>);
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(BatteryManager);
public:
    static BatteryManager* create(ExecutionContext*);
    virtual ~BatteryManager();

    // Returns a promise object that will be resolved with this BatteryManager.
    ScriptPromise startRequest(ScriptState*);

    // EventTarget implementation.
    virtual const WTF::AtomicString& interfaceName() const override { return EventTargetNames::BatteryManager; }
    virtual ExecutionContext* executionContext() const override { return ContextLifecycleObserver::executionContext(); }

    bool charging();
    double chargingTime();
    double dischargingTime();
    double level();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(chargingchange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(chargingtimechange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dischargingtimechange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(levelchange);

    // Inherited from PlatformEventController.
    virtual void didUpdateData() override;
    virtual void registerWithDispatcher() override;
    virtual void unregisterWithDispatcher() override;
    virtual bool hasLastData() override;

    // ActiveDOMObject implementation.
    virtual bool canSuspend() const { return true; }
    virtual void suspend() override;
    virtual void resume() override;
    virtual void stop() override;
    virtual bool hasPendingActivity() const override;

    virtual void trace(Visitor*) override;

private:
    enum State {
        NotStarted,
        Pending,
        Resolved,
    };

    explicit BatteryManager(ExecutionContext*);

    RefPtr<ScriptPromiseResolver> m_resolver;
    Member<BatteryStatus> m_batteryStatus;
    State m_state;
};

} // namespace blink

#endif // BatteryManager_h
