/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MediaKeySession_h
#define MediaKeySession_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/events/EventTarget.h"
#include "core/platform/Timer.h"
#include "core/platform/graphics/ContentDecryptionModuleSession.h"
#include "wtf/Deque.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Uint8Array.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class ContentDecryptionModule;
class ContentDecryptionModuleSession;
class ExceptionState;
class GenericEventQueue;
class MediaKeyError;
class MediaKeys;

// References are held by JS and MediaKeys.
// Because this object controls the lifetime of the ContentDecryptionModuleSession,
// it may outlive any references to it as long as the MediaKeys object is alive.
// The ContentDecryptionModuleSession has the same lifetime as this object.
class MediaKeySession
    : public RefCounted<MediaKeySession>, public ScriptWrappable, public EventTarget, public ContextLifecycleObserver
    , private ContentDecryptionModuleSessionClient {
public:
    static PassRefPtr<MediaKeySession> create(ScriptExecutionContext*, ContentDecryptionModule*, MediaKeys*);
    ~MediaKeySession();

    const String& keySystem() const { return m_keySystem; }
    String sessionId() const;

    void setError(MediaKeyError*);
    MediaKeyError* error() { return m_error.get(); }

    void generateKeyRequest(const String& mimeType, Uint8Array* initData);
    void update(Uint8Array* key, ExceptionState&);
    void close();

    using RefCounted<MediaKeySession>::ref;
    using RefCounted<MediaKeySession>::deref;

    void enqueueEvent(PassRefPtr<Event>);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitkeyadded);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitkeyerror);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitkeymessage);

    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ScriptExecutionContext* scriptExecutionContext() const OVERRIDE;

private:
    MediaKeySession(ScriptExecutionContext*, ContentDecryptionModule*, MediaKeys*);
    void keyRequestTimerFired(Timer<MediaKeySession>*);
    void addKeyTimerFired(Timer<MediaKeySession>*);

    // ContentDecryptionModuleSessionClient
    virtual void keyAdded() OVERRIDE;
    virtual void keyError(MediaKeyErrorCode, unsigned long systemCode) OVERRIDE;
    virtual void keyMessage(const unsigned char* message, size_t messageLength, const KURL& destinationURL) OVERRIDE;

    String m_keySystem;
    RefPtr<MediaKeyError> m_error;
    OwnPtr<GenericEventQueue> m_asyncEventQueue;
    OwnPtr<ContentDecryptionModuleSession> m_session;
    // Used to remove the reference from the parent MediaKeys when close()'d.
    MediaKeys* m_keys;

    struct PendingKeyRequest {
        PendingKeyRequest(const String& mimeType, Uint8Array* initData) : mimeType(mimeType), initData(initData) { }
        String mimeType;
        RefPtr<Uint8Array> initData;
    };
    Deque<PendingKeyRequest> m_pendingKeyRequests;
    Timer<MediaKeySession> m_keyRequestTimer;

    Deque<RefPtr<Uint8Array> > m_pendingKeys;
    Timer<MediaKeySession> m_addKeyTimer;

private:
    virtual void refEventTarget() OVERRIDE { ref(); }
    virtual void derefEventTarget() OVERRIDE { deref(); }

    virtual EventTargetData* eventTargetData() OVERRIDE { return &m_eventTargetData; }
    virtual EventTargetData* ensureEventTargetData() OVERRIDE { return &m_eventTargetData; }

    EventTargetData m_eventTargetData;
};

}

#endif // MediaKeySession_h
