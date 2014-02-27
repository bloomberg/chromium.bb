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

#include "bindings/v8/ExceptionState.h"
#include "core/dom/ContextLifecycleObserver.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/ThreadLocalEventNames.h"
#include "core/html/HTMLMediaElement.h"
#include "modules/encryptedmedia/MediaKeyMessageEvent.h"
#include "platform/ContentType.h"
#include "platform/Logging.h"
#include "platform/MIMETypeRegistry.h"
#include "platform/UUID.h"
#include "platform/drm/ContentDecryptionModule.h"
#include "wtf/HashSet.h"

namespace WebCore {

PassRefPtrWillBeRawPtr<MediaKeys> MediaKeys::create(ExecutionContext* context, const String& keySystem, ExceptionState& exceptionState)
{
    // From <http://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#dom-media-keys-constructor>:
    // The MediaKeys(keySystem) constructor must run the following steps:

    // 1. If keySystem is null or an empty string, throw an InvalidAccessError exception and abort these steps.
    if (keySystem.isEmpty()) {
        exceptionState.throwDOMException(InvalidAccessError, "The key system provided is invalid.");
        return nullptr;
    }

    // 2. If keySystem is not one of the user agent's supported Key Systems, throw a NotSupportedError and abort these steps.
    if (!ContentDecryptionModule::supportsKeySystem(keySystem)) {
        exceptionState.throwDOMException(NotSupportedError, "The '" + keySystem + "' key system is not supported.");
        return nullptr;
    }

    // 3. Let cdm be the content decryption module corresponding to keySystem.
    // 4. Load cdm if necessary.
    OwnPtr<ContentDecryptionModule> cdm = ContentDecryptionModule::create(keySystem);
    if (!cdm) {
        exceptionState.throwDOMException(NotSupportedError, "A content decryption module could not be loaded for the '" + keySystem + "' key system.");
        return nullptr;
    }

    // 5. Create a new MediaKeys object.
    // 5.1 Let the keySystem attribute be keySystem.
    // 6. Return the new object to the caller.
    return adoptRefWillBeNoop(new MediaKeys(context, keySystem, cdm.release()));
}

MediaKeys::MediaKeys(ExecutionContext* context, const String& keySystem, PassOwnPtr<ContentDecryptionModule> cdm)
    : ContextLifecycleObserver(context)
    , m_mediaElement(0)
    , m_keySystem(keySystem)
    , m_cdm(cdm)
    , m_initializeNewSessionTimer(this, &MediaKeys::initializeNewSessionTimerFired)
    , m_weakFactory(this)
{
    WTF_LOG(Media, "MediaKeys::MediaKeys");
    ScriptWrappable::init(this);
}

MediaKeys::~MediaKeys()
{
}

PassRefPtrWillBeRawPtr<MediaKeySession> MediaKeys::createSession(ExecutionContext* context, const String& contentType, Uint8Array* initData, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "MediaKeys::createSession");

    // From <http://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#dom-createsession>:
    // The createSession(type, initData) method must run the following steps:
    // Note: The contents of initData are container-specific Initialization Data.

    if (contentType.isEmpty()) {
        exceptionState.throwDOMException(InvalidAccessError, "The contentType provided ('" + contentType + "') is empty.");
        return nullptr;
    }

    if (!initData || !initData->length()) {
        exceptionState.throwDOMException(InvalidAccessError, "The initData provided is null or empty.");
        return nullptr;
    }

    // 1. If type contains a MIME type that is not supported or is not supported by the keySystem,
    // throw a NOT_SUPPORTED_ERR exception and abort these steps.
    if (!m_cdm->supportsMIMEType(contentType)) {
        exceptionState.throwDOMException(NotSupportedError, "The type provided ('" + contentType + "') is unsupported.");
        return nullptr;
    }

    // 2. Create a new MediaKeySession object.
    RefPtrWillBeRawPtr<MediaKeySession> session = MediaKeySession::create(context, m_cdm.get(), m_weakFactory.createWeakPtr());
    // 2.1 Let the keySystem attribute be keySystem.
    ASSERT(!session->keySystem().isEmpty());
    // FIXME: 2.2 Let the state of the session be CREATED.

    // 3. Add the new object to an internal list of session objects (not needed).

    // 4. Schedule a task to initialize the session, providing type, initData, and the new object.
    m_pendingInitializeNewSessionData.append(InitializeNewSessionData(session, contentType, initData));

    if (!m_initializeNewSessionTimer.isActive())
        m_initializeNewSessionTimer.startOneShot(0);

    // 5. Return the new object to the caller.
    return session;
}

bool MediaKeys::isTypeSupported(const String& keySystem, const String& contentType)
{
    WTF_LOG(Media, "MediaKeys::isTypeSupported(%s, %s)", keySystem.ascii().data(), contentType.ascii().data());

    // 1. If keySystem is null or an empty string, return false and abort these steps.
    if (keySystem.isEmpty())
        return false;

    // 2. If keySystem contains an unrecognized or unsupported Key System, return false and abort
    // these steps. Key system string comparison is case-sensitive.
    if (!MIMETypeRegistry::isSupportedEncryptedMediaMIMEType(keySystem, "", ""))
        return false;

    // 3. If contentType is null or an empty string, return true and abort these steps.
    if (contentType.isEmpty())
        return true;

    // 4. If the Key System specified by keySystem does not support decrypting the container and/or
    // codec specified by contentType, return false and abort these steps.
    ContentType type(contentType);
    String codecs = type.parameter("codecs");

    return MIMETypeRegistry::isSupportedEncryptedMediaMIMEType(keySystem, type.type(), codecs);
}

void MediaKeys::setMediaElement(HTMLMediaElement* element)
{
    // FIXME: Cause HTMLMediaElement::setMediaKeys() to throw an exception if m_mediaElement is not 0
    // and remove the code that prevents the assert below in HTMLMediaElement.
    ASSERT(!m_mediaElement != !element);
    m_mediaElement = element;
}

blink::WebContentDecryptionModule* MediaKeys::contentDecryptionModule()
{
    return m_cdm ? m_cdm->contentDecryptionModule() : 0;
}

void MediaKeys::initializeNewSessionTimerFired(Timer<MediaKeys>*)
{
    ASSERT(m_pendingInitializeNewSessionData.size());

    while (!m_pendingInitializeNewSessionData.isEmpty()) {
        InitializeNewSessionData data = m_pendingInitializeNewSessionData.takeFirst();
        // FIXME: Refer to the spec to see what needs to be done in blink.
        data.session->initializeNewSession(data.contentType, *data.initData);
    }
}

void MediaKeys::trace(Visitor* visitor)
{
    visitor->trace(m_pendingInitializeNewSessionData);
}

void MediaKeys::InitializeNewSessionData::trace(Visitor* visitor)
{
    visitor->trace(session);
}

void MediaKeys::contextDestroyed()
{
    ContextLifecycleObserver::contextDestroyed();

    // We don't need the CDM anymore.
    m_cdm.clear();
}

}
