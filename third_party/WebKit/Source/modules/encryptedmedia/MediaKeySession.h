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
#include "platform/Timer.h"
#include "platform/drm/ContentDecryptionModuleSession.h"
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
class MediaKeySession FINAL
    : public RefCounted<MediaKeySession>, public ScriptWrappable, public EventTargetWithInlineData, public ContextLifecycleObserver
    , private ContentDecryptionModuleSessionClient {
    REFCOUNTED_EVENT_TARGET(MediaKeySession);
public:
    static PassRefPtr<MediaKeySession> create(ExecutionContext*, ContentDecryptionModule*, MediaKeys*);
    virtual ~MediaKeySession();

    const String& keySystem() const { return m_keySystem; }
    String sessionId() const;

    void setError(MediaKeyError*);
    MediaKeyError* error() { return m_error.get(); }

    void initializeNewSession(const String& mimeType, const Uint8Array& initData);
    void update(Uint8Array* response, ExceptionState&);
    void release(ExceptionState&);

    void enqueueEvent(PassRefPtr<Event>);

    virtual const AtomicString& interfaceName() const OVERRIDE;
    virtual ExecutionContext* executionContext() const OVERRIDE;

private:
    MediaKeySession(ExecutionContext*, ContentDecryptionModule*, MediaKeys*);
    void updateTimerFired(Timer<MediaKeySession>*);

    // ContentDecryptionModuleSessionClient
    virtual void message(const unsigned char* message, size_t messageLength, const KURL& destinationURL) OVERRIDE;
    virtual void ready() OVERRIDE;
    virtual void close() OVERRIDE;
    virtual void error(MediaKeyErrorCode, unsigned long systemCode) OVERRIDE;

    String m_keySystem;
    RefPtr<MediaKeyError> m_error;
    OwnPtr<GenericEventQueue> m_asyncEventQueue;
    OwnPtr<ContentDecryptionModuleSession> m_session;
    // Used to remove the reference from the parent MediaKeys when close()'d.
    MediaKeys* m_keys;

    Deque<RefPtr<Uint8Array> > m_pendingUpdates;
    Timer<MediaKeySession> m_updateTimer;
};

}

#endif // MediaKeySession_h
