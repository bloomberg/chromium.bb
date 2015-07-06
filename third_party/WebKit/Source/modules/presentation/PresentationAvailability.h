// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationAvailability_h
#define PresentationAvailability_h

#include "core/dom/ActiveDOMObject.h"
#include "core/events/EventTarget.h"
#include "public/platform/modules/presentation/WebPresentationAvailabilityObserver.h"

namespace blink {

class ExecutionContext;
class ScriptPromiseResolver;

// Expose whether there is a presentation display available. The object will be
// initialized with a default value passed via ::take() and will then subscribe
// to receive callbacks if the status were to change. The object will only
// listen to changes when required.
class PresentationAvailability final
    : public RefCountedGarbageCollectedEventTargetWithInlineData<PresentationAvailability>
    , public ActiveDOMObject
    , public WebPresentationAvailabilityObserver {
    REFCOUNTED_GARBAGE_COLLECTED_EVENT_TARGET(PresentationAvailability);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(PresentationAvailability);
    DEFINE_WRAPPERTYPEINFO();
public:
    static PresentationAvailability* take(ScriptPromiseResolver*, bool);

    static PresentationAvailability* create(ExecutionContext*, bool);
    ~PresentationAvailability() override;

    // EventTarget implementation.
    const AtomicString& interfaceName() const override;
    ExecutionContext* executionContext() const override;

    // WebPresentationAvailabilityObserver implementation.
    void availabilityChanged(bool) override;

    // ActiveDOMObject implementation.
    bool hasPendingActivity() const override;
    void suspend() override;
    void resume() override;
    void stop() override;

    bool value() const;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(change);

    DECLARE_VIRTUAL_TRACE();

private:
    PresentationAvailability(ExecutionContext*, bool);

    void startListening();
    void stopListening();

    bool m_value;
    bool m_listening;
};

} // namespace blink

#endif // PresentationAvailability_h
