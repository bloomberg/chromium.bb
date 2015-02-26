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

#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptWrappable.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/DOMArrayPiece.h"
#include "platform/Timer.h"
#include "public/platform/WebEncryptedMediaTypes.h"
#include "public/platform/WebString.h"
#include "public/platform/WebVector.h"
#include "wtf/Forward.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class ExecutionContext;
class MediaKeySession;
class ScriptState;
class WebContentDecryptionModule;

// References are held by JS and HTMLMediaElement.
// The WebContentDecryptionModule has the same lifetime as this object.
class MediaKeys : public GarbageCollectedFinalized<MediaKeys>, public ContextLifecycleObserver, public ScriptWrappable {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(MediaKeys);
    DEFINE_WRAPPERTYPEINFO();
public:
    MediaKeys(ExecutionContext*, const String& keySystem, const blink::WebVector<WebEncryptedMediaSessionType>& supportedSessionTypes, PassOwnPtr<WebContentDecryptionModule>);
    virtual ~MediaKeys();

    // FIXME: This should be removed after crbug.com/425186 is fully
    // implemented.
    const String& keySystem() const { return m_keySystem; }

    MediaKeySession* createSession(ScriptState*, const String& sessionTypeString, ExceptionState&);

    ScriptPromise setServerCertificate(ScriptState*, const DOMArrayPiece& serverCertificate);

    blink::WebContentDecryptionModule* contentDecryptionModule();

    DECLARE_VIRTUAL_TRACE();

    // ContextLifecycleObserver
    virtual void contextDestroyed() override;

private:
    class PendingAction;

    bool sessionTypeSupported(WebEncryptedMediaSessionType);
    void timerFired(Timer<MediaKeys>*);

    const String m_keySystem;
    const blink::WebVector<WebEncryptedMediaSessionType> m_supportedSessionTypes;
    OwnPtr<blink::WebContentDecryptionModule> m_cdm;

    HeapDeque<Member<PendingAction>> m_pendingActions;
    Timer<MediaKeys> m_timer;
};

} // namespace blink

#endif // MediaKeys_h
