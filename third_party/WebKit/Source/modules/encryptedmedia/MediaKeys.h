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

#ifndef MediaKeys_h
#define MediaKeys_h

#include "bindings/v8/ScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/EventTarget.h"
#include "heap/Handle.h"
#include "modules/encryptedmedia/MediaKeySession.h"
#include "platform/Timer.h"
#include "wtf/Deque.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefCounted.h"
#include "wtf/Uint8Array.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {
class WebContentDecryptionModule;
}

namespace WebCore {

class ContentDecryptionModule;
class HTMLMediaElement;
class ExceptionState;

// References are held by JS and HTMLMediaElement.
// The ContentDecryptionModule has the same lifetime as this object.
class MediaKeys : public RefCountedWillBeGarbageCollectedFinalized<MediaKeys>, public ContextLifecycleObserver, public ScriptWrappable {
public:
    static PassRefPtrWillBeRawPtr<MediaKeys> create(ExecutionContext*, const String& keySystem, ExceptionState&);
    ~MediaKeys();

    const String& keySystem() const { return m_keySystem; }

    PassRefPtrWillBeRawPtr<MediaKeySession> createSession(ExecutionContext*, const String& contentType, Uint8Array* initData, ExceptionState&);

    static bool isTypeSupported(const String& keySystem, const String& contentType);

    void setMediaElement(HTMLMediaElement*);

    blink::WebContentDecryptionModule* contentDecryptionModule();

    void trace(Visitor*);

    // ContextLifecycleObserver
    virtual void contextDestroyed() OVERRIDE;

protected:
    MediaKeys(ExecutionContext*, const String& keySystem, PassOwnPtr<ContentDecryptionModule>);
    void initializeNewSessionTimerFired(Timer<MediaKeys>*);

    HTMLMediaElement* m_mediaElement;
    const String m_keySystem;
    OwnPtr<ContentDecryptionModule> m_cdm;

    // FIXME: Check whether |initData| can be changed by JS. Maybe we should not pass it as a pointer.
    struct InitializeNewSessionData {
        InitializeNewSessionData(PassRefPtrWillBeRawPtr<MediaKeySession> session, const String& contentType, PassRefPtr<Uint8Array> initData)
            : session(session)
            , contentType(contentType)
            , initData(initData) { }

        void trace(Visitor*);

        RefPtrWillBeMember<MediaKeySession> session;
        String contentType;
        RefPtr<Uint8Array> initData;
    };
    Deque<InitializeNewSessionData> m_pendingInitializeNewSessionData;
    Timer<MediaKeys> m_initializeNewSessionTimer;

    WeakPtrFactory<MediaKeys> m_weakFactory;
};

}

#endif // MediaKeys_h
