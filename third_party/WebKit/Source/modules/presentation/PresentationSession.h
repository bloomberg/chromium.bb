// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationSession_h
#define PresentationSession_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/events/EventTarget.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace blink {

class PresentationSession final
    : public RefCountedGarbageCollectedWillBeGarbageCollectedFinalized<PresentationSession>
    , public EventTargetWithInlineData
    , public ContextLifecycleObserver {
    DEFINE_EVENT_TARGET_REFCOUNTING_WILL_BE_REMOVED(RefCountedGarbageCollected<PresentationSession>);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(PresentationSession);
    DEFINE_WRAPPERTYPEINFO();
public:
    static PresentationSession* create(ExecutionContext*);
    virtual ~PresentationSession();

    // EventTarget implementation.
    virtual const AtomicString& interfaceName() const override;
    virtual ExecutionContext* executionContext() const override;

    virtual void trace(Visitor*) override;

    const String& id() const { return m_id; }
    const AtomicString& state() const { return m_state; }

    void postMessage(const String& message);
    void close();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(message);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(statechange);

private:
    explicit PresentationSession(ExecutionContext*);

    String m_id;
    AtomicString m_state;
};

} // namespace blink

#endif // PresentationSession_h
