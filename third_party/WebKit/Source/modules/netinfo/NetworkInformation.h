// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NetworkInformation_h
#define NetworkInformation_h

#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "core/page/NetworkStateNotifier.h"
#include "public/platform/WebConnectionType.h"

namespace blink {

class ExecutionContext;

class NetworkInformation FINAL
    : public RefCountedGarbageCollectedWillBeGarbageCollectedFinalized<NetworkInformation>
    , public ActiveDOMObject
    , public EventTargetWithInlineData
    , public NetworkStateNotifier::NetworkStateObserver {
    DEFINE_EVENT_TARGET_REFCOUNTING_WILL_BE_REMOVED(RefCountedGarbageCollected<NetworkInformation>);
    DEFINE_WRAPPERTYPEINFO();
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(NetworkInformation);
public:
    static NetworkInformation* create(ExecutionContext*);
    virtual ~NetworkInformation();

    String type() const;

    virtual void connectionTypeChange(WebConnectionType) OVERRIDE;

    // EventTarget overrides.
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;
    virtual bool addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture = false) OVERRIDE;
    virtual bool removeEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture = false) OVERRIDE;
    virtual void removeAllEventListeners() OVERRIDE;

    // ActiveDOMObject overrides.
    virtual bool hasPendingActivity() const OVERRIDE;
    virtual void stop() OVERRIDE;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(typechange);

private:
    explicit NetworkInformation(ExecutionContext*);
    void startObserving();
    void stopObserving();

    // Touched only on context thread.
    WebConnectionType m_type;

    // Whether this object is listening for events from NetworkStateNotifier.
    bool m_observing;

    // Whether ActiveDOMObject::stop has been called.
    bool m_contextStopped;
};

} // namespace blink

#endif // NetworkInformation_h
