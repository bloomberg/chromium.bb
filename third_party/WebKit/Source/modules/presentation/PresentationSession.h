// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PresentationSession_h
#define PresentationSession_h

#include "core/events/EventTarget.h"
#include "core/frame/DOMWindowProperty.h"
#include "public/platform/modules/presentation/WebPresentationSessionClient.h"
#include "wtf/text/WTFString.h"

namespace WTF {
class AtomicString;
} // namespace WTF

namespace blink {

class DOMArrayBuffer;
class DOMArrayBufferView;
class Presentation;
class PresentationController;

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

    const String id() const { return m_id; }
    const WTF::AtomicString& state() const;

    void send(const String& message, ExceptionState&);
    void send(PassRefPtr<DOMArrayBuffer> data, ExceptionState&);
    void send(PassRefPtr<DOMArrayBufferView> data, ExceptionState&);
    void close();

    DEFINE_ATTRIBUTE_EVENT_LISTENER(message);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(statechange);

    // Returns true if and only if the WebPresentationSessionClient represents this session.
    bool matches(WebPresentationSessionClient*) const;

    // Notifies the session about its state change.
    void didChangeState(WebPresentationSessionState);

    // Notifies the session about new text message.
    void didReceiveTextMessage(const String& message);

private:
    PresentationSession(LocalFrame*, const String& id, const String& url);

    // Returns the |PresentationController| object associated with the frame
    // |Presentation| corresponds to. Can return |nullptr| if the frame is
    // detached from the document.
    PresentationController* presentationController();

    // Common send method for both ArrayBufferView and ArrayBuffer.
    void sendInternal(const uint8_t* data, size_t, ExceptionState&);

    String m_id;
    String m_url;
    WebPresentationSessionState m_state;
};

} // namespace blink

#endif // PresentationSession_h
