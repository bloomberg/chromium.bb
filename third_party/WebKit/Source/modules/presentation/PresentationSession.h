// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationSession_h
#define PresentationSession_h

#include "core/events/EventTarget.h"
#include "core/frame/DOMWindowProperty.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/WTFString.h"

namespace blink {

class Presentation;
class WebPresentationSessionClient;
class WebString;

class PresentationSession final
    : public RefCountedGarbageCollectedEventTargetWithInlineData<PresentationSession>
    , public DOMWindowProperty {
    DEFINE_EVENT_TARGET_REFCOUNTING_WILL_BE_REMOVED(RefCountedGarbageCollected<PresentationSession>);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(PresentationSession);
    DEFINE_WRAPPERTYPEINFO();
public:
    static PresentationSession* take(WebPresentationSessionClient*, Presentation*);
    static void dispose(WebPresentationSessionClient*);
    virtual ~PresentationSession();

    // EventTarget implementation.
    virtual const AtomicString& interfaceName() const override;
    virtual ExecutionContext* executionContext() const override;

    DECLARE_VIRTUAL_TRACE();

    const String& id() const { return m_id; }
    const AtomicString& state() const { return m_state; }

    void postMessage(const String& message);
    void close();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(message);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(statechange);

private:
    PresentationSession(LocalFrame*, const WebString& id);

    String m_id;
    AtomicString m_state;
};

} // namespace blink

#endif // PresentationSession_h
