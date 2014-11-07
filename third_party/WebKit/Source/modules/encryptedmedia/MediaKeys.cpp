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

#include "config.h"
#include "modules/encryptedmedia/MediaKeys.h"

#include "bindings/core/v8/ScriptState.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "modules/encryptedmedia/MediaKeySession.h"
#include "modules/encryptedmedia/SimpleContentDecryptionModuleResult.h"
#include "platform/ContentType.h"
#include "platform/Logging.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/Timer.h"
#include "public/platform/WebContentDecryptionModule.h"
#include "wtf/RefPtr.h"

namespace blink {

static bool isKeySystemSupportedWithContentType(const String& keySystem, const String& contentType)
{
    ASSERT(!keySystem.isEmpty());

    ContentType type(contentType);
    String codecs = type.parameter("codecs");
    return MIMETypeRegistry::isSupportedEncryptedMediaMIMEType(keySystem, type.type(), codecs);
}

// A class holding a pending action.
class MediaKeys::PendingAction : public GarbageCollectedFinalized<MediaKeys::PendingAction> {
public:
    const Persistent<ContentDecryptionModuleResult> result() const
    {
        return m_result;
    }

    const RefPtr<DOMArrayBuffer> data() const
    {
        return m_data;
    }

    static PendingAction* CreatePendingSetServerCertificate(ContentDecryptionModuleResult* result, PassRefPtr<DOMArrayBuffer> serverCertificate)
    {
        ASSERT(result);
        ASSERT(serverCertificate);
        return new PendingAction(result, serverCertificate);
    }

    ~PendingAction()
    {
    }

    void trace(Visitor* visitor)
    {
        visitor->trace(m_result);
    }

private:
    PendingAction(ContentDecryptionModuleResult* result, PassRefPtr<DOMArrayBuffer> data)
        : m_result(result)
        , m_data(data)
    {
    }

    const Member<ContentDecryptionModuleResult> m_result;
    const RefPtr<DOMArrayBuffer> m_data;
};

MediaKeys::MediaKeys(ExecutionContext* context, const String& keySystem, PassOwnPtr<WebContentDecryptionModule> cdm)
    : ContextLifecycleObserver(context)
    , m_keySystem(keySystem)
    , m_cdm(cdm)
    , m_timer(this, &MediaKeys::timerFired)
{
    WTF_LOG(Media, "MediaKeys(%p)::MediaKeys", this);

    // Step 4.4 of MediaKeys::create():
    // 4.4.1 Set the keySystem attribute to keySystem.
    ASSERT(!m_keySystem.isEmpty());
}

MediaKeys::~MediaKeys()
{
    WTF_LOG(Media, "MediaKeys(%p)::~MediaKeys", this);
}

MediaKeySession* MediaKeys::createSession(ScriptState* scriptState, const String& sessionType)
{
    WTF_LOG(Media, "MediaKeys(%p)::createSession", this);

    // From <http://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#dom-createsession>:
    // The createSession(sessionType) method returns a new MediaKeySession
    // object. It must run the following steps:
    // 1. If sessionType is not supported by the content decryption module
    //    corresponding to the keySystem, throw a DOMException whose name is
    //    "NotSupportedError".
    // FIXME: Check whether sessionType is actually supported by the CDM.
    ASSERT(MediaKeySession::isValidSessionType(sessionType));

    // 2. Let session be a new MediaKeySession object, and initialize it as
    //    follows:
    //    (Initialization is performed in the constructor.)
    // 3. Return session.
    return MediaKeySession::create(scriptState, this, sessionType);
}

ScriptPromise MediaKeys::setServerCertificate(ScriptState* scriptState, DOMArrayBuffer* serverCertificate)
{
    RefPtr<DOMArrayBuffer> serverCertificateCopy = DOMArrayBuffer::create(serverCertificate->data(), serverCertificate->byteLength());
    return setServerCertificateInternal(scriptState, serverCertificateCopy.release());
}

ScriptPromise MediaKeys::setServerCertificate(ScriptState* scriptState, DOMArrayBufferView* serverCertificate)
{
    RefPtr<DOMArrayBuffer> serverCertificateCopy = DOMArrayBuffer::create(serverCertificate->baseAddress(), serverCertificate->byteLength());
    return setServerCertificateInternal(scriptState, serverCertificateCopy.release());
}

ScriptPromise MediaKeys::setServerCertificateInternal(ScriptState* scriptState, PassRefPtr<DOMArrayBuffer> serverCertificate)
{
    // From https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#dom-setservercertificate:
    // The setServerCertificate(serverCertificate) method provides a server
    // certificate to be used to encrypt messages to the license server.
    // It must run the following steps:
    // 1. If serverCertificate is an empty array, return a promise rejected
    //    with a new DOMException whose name is "InvalidAccessError".
    if (!serverCertificate->byteLength()) {
        return ScriptPromise::rejectWithDOMException(
            scriptState, DOMException::create(InvalidAccessError, "The serverCertificate parameter is empty."));
    }

    // 2. If the keySystem does not support server certificates, return a
    //    promise rejected with a new DOMException whose name is
    //    "NotSupportedError".
    //    (Let the CDM decide whether to support this or not.)

    // 3. Let certificate be a copy of the contents of the serverCertificate
    //    parameter.
    //    (Done in caller.)

    // 4. Let promise be a new promise.
    SimpleContentDecryptionModuleResult* result = new SimpleContentDecryptionModuleResult(scriptState);
    ScriptPromise promise = result->promise();

    // 5. Run the following steps asynchronously (documented in timerFired()).
    m_pendingActions.append(PendingAction::CreatePendingSetServerCertificate(result, serverCertificate));
    if (!m_timer.isActive())
        m_timer.startOneShot(0, FROM_HERE);

    // 6. Return promise.
    return promise;
}

bool MediaKeys::isTypeSupported(const String& keySystem, const String& contentType)
{
    WTF_LOG(Media, "MediaKeys::isTypeSupported(%s, %s)", keySystem.ascii().data(), contentType.ascii().data());

    // 1. If keySystem is an empty string, return false and abort these steps.
    if (keySystem.isEmpty())
        return false;

    // 2. If keySystem contains an unrecognized or unsupported Key System, return false and abort
    // these steps. Key system string comparison is case-sensitive.
    if (!isKeySystemSupportedWithContentType(keySystem, ""))
        return false;

    // 3. If contentType is an empty string, return true and abort these steps.
    if (contentType.isEmpty())
        return true;

    // 4. If the Key System specified by keySystem does not support decrypting the container and/or
    // codec specified by contentType, return false and abort these steps.
    return isKeySystemSupportedWithContentType(keySystem, contentType);
}

void MediaKeys::timerFired(Timer<MediaKeys>*)
{
    ASSERT(m_pendingActions.size());

    // Swap the queue to a local copy to avoid problems if resolving promises
    // run synchronously.
    HeapDeque<Member<PendingAction> > pendingActions;
    pendingActions.swap(m_pendingActions);

    while (!pendingActions.isEmpty()) {
        PendingAction* action = pendingActions.takeFirst();
        WTF_LOG(Media, "MediaKeys(%p)::timerFired: Certificate", this);

        // 5.1 Let cdm be the cdm during the initialization of this object.
        WebContentDecryptionModule* cdm = contentDecryptionModule();

        // 5.2 Use the cdm to process certificate.
        cdm->setServerCertificate(static_cast<unsigned char*>(action->data()->data()), action->data()->byteLength(), action->result()->result());
        // 5.3 If any of the preceding steps failed, reject promise with a
        //     new DOMException whose name is the appropriate error name.
        // 5.4 Resolve promise.
        // (These are handled by Chromium and the CDM.)
    }
}

WebContentDecryptionModule* MediaKeys::contentDecryptionModule()
{
    return m_cdm.get();
}

void MediaKeys::trace(Visitor* visitor)
{
    visitor->trace(m_pendingActions);
}

void MediaKeys::contextDestroyed()
{
    ContextLifecycleObserver::contextDestroyed();

    // We don't need the CDM anymore.
    m_cdm.clear();
}

} // namespace blink
