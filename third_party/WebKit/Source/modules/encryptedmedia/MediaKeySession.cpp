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
#include "modules/encryptedmedia/MediaKeySession.h"

#include "bindings/v8/ExceptionState.h"
#include "core/events/Event.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/GenericEventQueue.h"
#include "core/html/MediaKeyError.h"
#include "core/platform/graphics/ContentDecryptionModule.h"
#include "modules/encryptedmedia/MediaKeyMessageEvent.h"
#include "modules/encryptedmedia/MediaKeys.h"

namespace WebCore {

PassRefPtr<MediaKeySession> MediaKeySession::create(ScriptExecutionContext* context, ContentDecryptionModule* cdm, MediaKeys* keys)
{
    return adoptRef(new MediaKeySession(context, cdm, keys));
}

MediaKeySession::MediaKeySession(ScriptExecutionContext* context, ContentDecryptionModule* cdm, MediaKeys* keys)
    : ContextLifecycleObserver(context)
    , m_asyncEventQueue(GenericEventQueue::create(this))
    , m_keySystem(keys->keySystem())
    , m_session(cdm->createSession(this))
    , m_keys(keys)
    , m_keyRequestTimer(this, &MediaKeySession::keyRequestTimerFired)
    , m_addKeyTimer(this, &MediaKeySession::addKeyTimerFired)
{
    ScriptWrappable::init(this);
}

MediaKeySession::~MediaKeySession()
{
    close();
}

void MediaKeySession::setError(MediaKeyError* error)
{
    m_error = error;
}

void MediaKeySession::close()
{
    ASSERT(!m_keys == !m_session);

    if (m_session)
        m_session->close();
    m_session.clear();
    m_asyncEventQueue->cancelAllEvents();

    // FIXME: Release ref that MediaKeys has by removing it from m_sessions.
    // if (m_keys) m_keys->sessionClosed(this);
    m_keys = 0;
}

String MediaKeySession::sessionId() const
{
    return m_session->sessionId();
}

void MediaKeySession::generateKeyRequest(const String& mimeType, Uint8Array* initData)
{
    m_pendingKeyRequests.append(PendingKeyRequest(mimeType, initData));
    // FIXME: Eliminate timers. Asynchronicity will be handled in Chromium.
    m_keyRequestTimer.startOneShot(0);
}

void MediaKeySession::keyRequestTimerFired(Timer<MediaKeySession>*)
{
    ASSERT(m_pendingKeyRequests.size());
    if (!m_session)
        return;

    while (!m_pendingKeyRequests.isEmpty()) {
        PendingKeyRequest request = m_pendingKeyRequests.takeFirst();

        // NOTE: Continued from step 5 in MediaKeys::createSession().
        // The user agent will asynchronously execute the following steps in the task:

        // 1. Let cdm be the cdm loaded in the MediaKeys constructor.
        // 2. Let destinationURL be null.

        // 3. Use cdm to generate a key request and follow the steps for the first matching condition from the following list:
        m_session->generateKeyRequest(request.mimeType, *request.initData);
    }
}

void MediaKeySession::update(Uint8Array* key, ExceptionState& es)
{
    // From <http://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#dom-addkey>:
    // The addKey(key) method must run the following steps:
    // 1. If the first or second argument [sic] is null or an empty array, throw an InvalidAccessError.
    // NOTE: the reference to a "second argument" is a spec bug.
    if (!key || !key->length()) {
        es.throwUninformativeAndGenericDOMException(InvalidAccessError);
        return;
    }

    // 2. Schedule a task to handle the call, providing key.
    m_pendingKeys.append(key);
    m_addKeyTimer.startOneShot(0);
}

void MediaKeySession::addKeyTimerFired(Timer<MediaKeySession>*)
{
    ASSERT(m_pendingKeys.size());
    if (!m_session)
        return;

    while (!m_pendingKeys.isEmpty()) {
        RefPtr<Uint8Array> pendingKey = m_pendingKeys.takeFirst();
        unsigned short errorCode = 0;
        unsigned long systemCode = 0;

        // NOTE: Continued from step 2. of MediaKeySession::update()
        // 2.1. Let cdm be the cdm loaded in the MediaKeys constructor.
        // NOTE: This is m_session.
        // 2.2. Let 'did store key' be false.
        // 2.3. Let 'next message' be null.
        // 2.4. Use cdm to handle key.
        m_session->update(*pendingKey);
    }
}

void MediaKeySession::keyAdded()
{
    RefPtr<Event> event = Event::create(eventNames().webkitkeyaddedEvent);
    event->setTarget(this);
    m_asyncEventQueue->enqueueEvent(event.release());
}

// Queue a task to fire a simple event named keyadded at the MediaKeySession object.
void MediaKeySession::keyError(MediaKeyErrorCode errorCode, unsigned long systemCode)
{
    MediaKeyError::Code mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_UNKNOWN;
    switch (errorCode) {
    case UnknownError:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_UNKNOWN;
        break;
    case ClientError:
        mediaKeyErrorCode = MediaKeyError::MEDIA_KEYERR_CLIENT;
        break;
    }

    // 1. Create a new MediaKeyError object with the following attributes:
    //    code = the appropriate MediaKeyError code
    //    systemCode = a Key System-specific value, if provided, and 0 otherwise
    // 2. Set the MediaKeySession object's error attribute to the error object created in the previous step.
    m_error = MediaKeyError::create(mediaKeyErrorCode, systemCode);

    // 3. queue a task to fire a simple event named keyerror at the MediaKeySession object.
    RefPtr<Event> event = Event::create(eventNames().webkitkeyerrorEvent);
    event->setTarget(this);
    m_asyncEventQueue->enqueueEvent(event.release());
}

// Queue a task to fire a simple event named keymessage at the new object
void MediaKeySession::keyMessage(const unsigned char* message, size_t messageLength, const KURL& destinationURL)
{
    MediaKeyMessageEventInit init;
    init.bubbles = false;
    init.cancelable = false;
    init.message = Uint8Array::create(message, messageLength);
    init.destinationURL = destinationURL;

    RefPtr<MediaKeyMessageEvent> event = MediaKeyMessageEvent::create(eventNames().webkitkeymessageEvent, init);
    event->setTarget(this);
    m_asyncEventQueue->enqueueEvent(event.release());
}

const AtomicString& MediaKeySession::interfaceName() const
{
    return eventNames().interfaceForMediaKeySession;
}

ScriptExecutionContext* MediaKeySession::scriptExecutionContext() const
{
    return ContextLifecycleObserver::scriptExecutionContext();
}

}
