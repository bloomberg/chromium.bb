// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Presentation_h
#define Presentation_h

#include "core/dom/ContextLifecycleObserver.h"
#include "core/events/EventTarget.h"

namespace blink {

class Presentation FINAL
    : public RefCountedGarbageCollectedWillBeGarbageCollectedFinalized<Presentation>
    , public EventTargetWithInlineData
    , public ContextLifecycleObserver {
    DEFINE_EVENT_TARGET_REFCOUNTING_WILL_BE_REMOVED(RefCountedGarbageCollected<Presentation>);
    DEFINE_WRAPPERTYPEINFO();
public:
    static Presentation* create(ExecutionContext*);
    virtual ~Presentation();

    // EventTarget implementation.
    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

    virtual void trace(Visitor*) OVERRIDE;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(availablechange);

private:
    explicit Presentation(ExecutionContext*);
};

} // namespace blink

#endif // Presentation_h
