/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/mediasource/MediaSourceBase.h"

#include "core/dom/Event.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/GenericEventQueue.h"
#include "core/platform/graphics/SourceBufferPrivate.h"
#include "modules/mediasource/MediaSourceRegistry.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

MediaSourceBase::MediaSourceBase(ScriptExecutionContext* context)
    : ActiveDOMObject(context)
    , m_readyState(closedKeyword())
    , m_asyncEventQueue(GenericEventQueue::create(this))
{
}

MediaSourceBase::~MediaSourceBase()
{
}

const AtomicString& MediaSourceBase::openKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, open, ("open", AtomicString::ConstructFromLiteral));
    return open;
}

const AtomicString& MediaSourceBase::closedKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, closed, ("closed", AtomicString::ConstructFromLiteral));
    return closed;
}

const AtomicString& MediaSourceBase::endedKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, ended, ("ended", AtomicString::ConstructFromLiteral));
    return ended;
}

void MediaSourceBase::setPrivateAndOpen(PassOwnPtr<MediaSourcePrivate> mediaSourcePrivate)
{
    ASSERT(mediaSourcePrivate);
    ASSERT(!m_private);
    m_private = mediaSourcePrivate;
    setReadyState(openKeyword());
}

void MediaSourceBase::addedToRegistry()
{
    setPendingActivity(this);
}

void MediaSourceBase::removedFromRegistry()
{
    unsetPendingActivity(this);
}

double MediaSourceBase::duration() const
{
    return isClosed() ? std::numeric_limits<float>::quiet_NaN() : m_private->duration();
}

void MediaSourceBase::setDuration(double duration, ExceptionCode& ec)
{
    if (duration < 0.0 || std::isnan(duration)) {
        ec = INVALID_ACCESS_ERR;
        return;
    }
    if (!isOpen()) {
        ec = INVALID_STATE_ERR;
        return;
    }
    m_private->setDuration(duration);
}


void MediaSourceBase::setReadyState(const AtomicString& readyState)
{
    m_readyState = readyState;

    if (isClosed())
        m_private.clear();
}

void MediaSourceBase::endOfStream(const AtomicString& error, ExceptionCode& ec)
{
    DEFINE_STATIC_LOCAL(const AtomicString, network, ("network", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, decode, ("decode", AtomicString::ConstructFromLiteral));

    // 3.1 http://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#dom-endofstream
    // 1. If the readyState attribute is not in the "open" state then throw an
    // INVALID_STATE_ERR exception and abort these steps.
    if (!isOpen()) {
        ec = INVALID_STATE_ERR;
        return;
    }

    MediaSourcePrivate::EndOfStreamStatus eosStatus = MediaSourcePrivate::EosNoError;

    if (error.isNull() || error.isEmpty()) {
        eosStatus = MediaSourcePrivate::EosNoError;
    } else if (error == network) {
        eosStatus = MediaSourcePrivate::EosNetworkError;
    } else if (error == decode) {
        eosStatus = MediaSourcePrivate::EosDecodeError;
    } else {
        ec = INVALID_ACCESS_ERR;
        return;
    }

    // 2. Change the readyState attribute value to "ended".
    setReadyState(endedKeyword());
    m_private->endOfStream(eosStatus);
}

bool MediaSourceBase::isOpen() const
{
    return readyState() == openKeyword();
}

bool MediaSourceBase::isClosed() const
{
    return readyState() == closedKeyword();
}

void MediaSourceBase::close()
{
    setReadyState(closedKeyword());
}

void MediaSourceBase::openIfInEndedState()
{
    if (m_readyState != endedKeyword())
        return;

    setReadyState(openKeyword());
}

bool MediaSourceBase::hasPendingActivity() const
{
    return m_private || m_asyncEventQueue->hasPendingEvents()
        || ActiveDOMObject::hasPendingActivity();
}

void MediaSourceBase::stop()
{
    m_asyncEventQueue->close();
    if (!isClosed())
        setReadyState(closedKeyword());
    m_private.clear();
}

PassOwnPtr<SourceBufferPrivate> MediaSourceBase::createSourceBufferPrivate(const String& type, const MediaSourcePrivate::CodecsArray& codecs, ExceptionCode& ec)
{
    OwnPtr<SourceBufferPrivate> sourceBufferPrivate;
    switch (m_private->addSourceBuffer(type, codecs, &sourceBufferPrivate)) {
    case MediaSourcePrivate::Ok: {
        return sourceBufferPrivate.release();
    }
    case MediaSourcePrivate::NotSupported:
        // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
        // Step 2: If type contains a MIME type ... that is not supported with the types
        // specified for the other SourceBuffer objects in sourceBuffers, then throw
        // a NOT_SUPPORTED_ERR exception and abort these steps.
        ec = NOT_SUPPORTED_ERR;
        return nullptr;
    case MediaSourcePrivate::ReachedIdLimit:
        // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
        // Step 3: If the user agent can't handle any more SourceBuffer objects then throw
        // a QUOTA_EXCEEDED_ERR exception and abort these steps.
        ec = QUOTA_EXCEEDED_ERR;
        return nullptr;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

void MediaSourceBase::scheduleEvent(const AtomicString& eventName)
{
    ASSERT(m_asyncEventQueue);

    RefPtr<Event> event = Event::create(eventName, false, false);
    event->setTarget(this);

    m_asyncEventQueue->enqueueEvent(event.release());
}

ScriptExecutionContext* MediaSourceBase::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

EventTargetData* MediaSourceBase::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* MediaSourceBase::ensureEventTargetData()
{
    return &m_eventTargetData;
}

URLRegistry& MediaSourceBase::registry() const
{
    return MediaSourceRegistry::registry();
}

void MediaSourceBase::reportMemoryUsage(MemoryObjectInfo* memoryObjectInfo) const
{
    MemoryClassInfo info(memoryObjectInfo, this, WebCoreMemoryTypes::DOM);
    ActiveDOMObject::reportMemoryUsage(memoryObjectInfo);
    info.addMember(m_eventTargetData, "eventTargetData");
    info.addMember(m_readyState, "readyState");
    info.addMember(m_private, "private");
    info.addMember(m_asyncEventQueue, "asyncEventQueue");
}

}
