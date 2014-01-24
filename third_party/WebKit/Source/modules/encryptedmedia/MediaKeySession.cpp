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
#include "modules/encryptedmedia/MediaKeyMessageEvent.h"
#include "modules/encryptedmedia/MediaKeys.h"
#include "platform/Logging.h"
#include "platform/drm/ContentDecryptionModule.h"

namespace WebCore {

PassRefPtr<MediaKeySession> MediaKeySession::create(ExecutionContext* context, ContentDecryptionModule* cdm, MediaKeys* keys)
{
    return adoptRef(new MediaKeySession(context, cdm, keys));
}

MediaKeySession::MediaKeySession(ExecutionContext* context, ContentDecryptionModule* cdm, MediaKeys* keys)
    : ContextLifecycleObserver(context)
    , m_keySystem(keys->keySystem())
    , m_asyncEventQueue(GenericEventQueue::create(this))
    , m_session(cdm->createSession(this))
    , m_keys(keys)
    , m_updateTimer(this, &MediaKeySession::updateTimerFired)
{
    WTF_LOG(Media, "MediaKeySession::MediaKeySession");
    ScriptWrappable::init(this);
    ASSERT(m_session);
}

MediaKeySession::~MediaKeySession()
{
    m_session.clear();
    m_asyncEventQueue->cancelAllEvents();

    // FIXME: Release ref that MediaKeys has by removing it from m_sessions.
    // if (m_keys) m_keys->sessionClosed(this);
    m_keys = 0;
}

void MediaKeySession::setError(MediaKeyError* error)
{
    m_error = error;
}

void MediaKeySession::release(ExceptionState& exceptionState)
{
    m_session->release();
}

String MediaKeySession::sessionId() const
{
    return m_session->sessionId();
}

void MediaKeySession::initializeNewSession(const String& mimeType, const Uint8Array& initData)
{
    m_session->initializeNewSession(mimeType, initData);
}

void MediaKeySession::update(Uint8Array* response, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "MediaKeySession::update");

    // From <https://dvcs.w3.org/hg/html-media/raw-file/default/encrypted-media/encrypted-media.html#dom-update>:
    // The update(response) method must run the following steps:
    // 1. If the argument is null or an empty array, throw an INVALID_ACCESS_ERR.
    if (!response || !response->length()) {
        exceptionState.throwDOMException(InvalidAccessError, String::format("The response argument provided is %s.", response ? "an empty array" : "invalid"));
        return;
    }

    // 2. If the session is not in the PENDING state, throw an INVALID_STATE_ERR.
    // FIXME: Implement states in MediaKeySession.

    // 3. Schedule a task to handle the call, providing response.
    m_pendingUpdates.append(response);

    if (!m_updateTimer.isActive())
        m_updateTimer.startOneShot(0);
}

void MediaKeySession::updateTimerFired(Timer<MediaKeySession>*)
{
    ASSERT(m_pendingUpdates.size());

    while (!m_pendingUpdates.isEmpty()) {
        RefPtr<Uint8Array> pendingUpdate = m_pendingUpdates.takeFirst();

        // NOTE: Continued from step 3. of MediaKeySession::update()
        // 3.1. Let cdm be the cdm loaded in the MediaKeys constructor.
        // NOTE: This is m_session.
        // 3.2. Let request be null.
        // 3.3. Use cdm to execute the following steps:
        // 3.3.1 Process response.
        m_session->update(*pendingUpdate);
    }
}

// Queue a task to fire a simple event named keymessage at the new object
void MediaKeySession::message(const unsigned char* message, size_t messageLength, const KURL& destinationURL)
{
    WTF_LOG(Media, "MediaKeySession::message");

    MediaKeyMessageEventInit init;
    init.bubbles = false;
    init.cancelable = false;
    init.message = Uint8Array::create(message, messageLength);
    init.destinationURL = destinationURL;

    RefPtr<MediaKeyMessageEvent> event = MediaKeyMessageEvent::create(EventTypeNames::message, init);
    event->setTarget(this);
    m_asyncEventQueue->enqueueEvent(event.release());
}

void MediaKeySession::ready()
{
    WTF_LOG(Media, "MediaKeySession::ready");

    RefPtr<Event> event = Event::create(EventTypeNames::ready);
    event->setTarget(this);
    m_asyncEventQueue->enqueueEvent(event.release());
}

void MediaKeySession::close()
{
    WTF_LOG(Media, "MediaKeySession::close");

    RefPtr<Event> event = Event::create(EventTypeNames::close);
    event->setTarget(this);
    m_asyncEventQueue->enqueueEvent(event.release());
}

// Queue a task to fire a simple event named keyadded at the MediaKeySession object.
void MediaKeySession::error(MediaKeyErrorCode errorCode, unsigned long systemCode)
{
    WTF_LOG(Media, "MediaKeySession::error: errorCode=%d, systemCode=%lu", errorCode, systemCode);

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
    RefPtr<Event> event = Event::create(EventTypeNames::error);
    event->setTarget(this);
    m_asyncEventQueue->enqueueEvent(event.release());
}

const AtomicString& MediaKeySession::interfaceName() const
{
    return EventTargetNames::MediaKeySession;
}

ExecutionContext* MediaKeySession::executionContext() const
{
    return ContextLifecycleObserver::executionContext();
}

}
