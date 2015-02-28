// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Presentation_h
#define Presentation_h

#include "bindings/core/v8/ScriptPromise.h"
#include "core/events/EventTarget.h"
#include "core/frame/DOMWindowProperty.h"
#include "modules/presentation/PresentationSession.h"
#include "platform/heap/Handle.h"
#include "platform/heap/Heap.h"

namespace WTF {
class String;
} // namespace WTF

namespace blink {

class LocalFrame;
class PresentationController;
class ScriptState;

// Implements the main entry point of the Presentation API corresponding to the Presentation.idl
// See https://w3c.github.io/presentation-api/#navigatorpresentation for details.
class Presentation final
    : public RefCountedGarbageCollectedEventTargetWithInlineData<Presentation>
    , public DOMWindowProperty {
    DEFINE_EVENT_TARGET_REFCOUNTING_WILL_BE_REMOVED(RefCountedGarbageCollected<Presentation>);
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(Presentation);
    DEFINE_WRAPPERTYPEINFO();
public:
    static Presentation* create(LocalFrame*);
    virtual ~Presentation();

    // EventTarget implementation.
    virtual const AtomicString& interfaceName() const override;
    virtual ExecutionContext* executionContext() const override;

    DECLARE_VIRTUAL_TRACE();

    PresentationSession* session() const;

    ScriptPromise startSession(ScriptState*, const String& presentationUrl, const String& presentationId);
    ScriptPromise joinSession(ScriptState*, const String& presentationUrl, const String& presentationId);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(availablechange);

    // The embedder needs to keep track if anything is listening to the event so it could stop the
    // might be expensive screen discovery process.
    virtual bool addEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture) override;
    virtual bool removeEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture) override;
    virtual void removeAllEventListeners() override;

    // Called when the |availablechange| event needs to be fired.
    void didChangeAvailability(bool available);
    // Queried by the controller if |availablechange| event has any listeners.
    bool isAvailableChangeWatched() const;
    // Adds a session to the open sessions list.
    void registerSession(PresentationSession*);

private:
    explicit Presentation(LocalFrame*);

    // Returns the |PresentationController| object associated with the frame |Presentation| corresponds to.
    // Can return |nullptr| if the frame is detached from the document.
    PresentationController* presentationController();

    // The session object provided to the presentation page. Not supported.
    Member<PresentationSession> m_session;

    // The sessions opened for this frame.
    HeapHashSet<Member<PresentationSession>> m_openSessions;
};

} // namespace blink

#endif // Presentation_h
