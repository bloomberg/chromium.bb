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
#include "modules/mediasource/MediaSource.h"

#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/GenericEventQueue.h"
#include "core/html/TimeRanges.h"
#include "modules/mediasource/MediaSourceRegistry.h"
#include "platform/ContentType.h"
#include "platform/Logging.h"
#include "platform/MIMETypeRegistry.h"
#include "public/platform/WebSourceBuffer.h"
#include "wtf/Uint8Array.h"
#include "wtf/text/CString.h"

using blink::WebSourceBuffer;

namespace WebCore {

PassRefPtrWillBeRawPtr<MediaSource> MediaSource::create(ExecutionContext* context)
{
    RefPtrWillBeRawPtr<MediaSource> mediaSource(adoptRefWillBeRefCountedGarbageCollected(new MediaSource(context)));
    mediaSource->suspendIfNeeded();
    return mediaSource.release();
}

MediaSource::MediaSource(ExecutionContext* context)
    : MediaSourceBase(context)
{
    WTF_LOG(Media, "MediaSource::MediaSource %p", this);
    ScriptWrappable::init(this);
    m_sourceBuffers = SourceBufferList::create(executionContext(), asyncEventQueue());
    m_activeSourceBuffers = SourceBufferList::create(executionContext(), asyncEventQueue());
}

MediaSource::~MediaSource()
{
    WTF_LOG(Media, "MediaSource::~MediaSource %p", this);
    ASSERT(isClosed());
}

SourceBuffer* MediaSource::addSourceBuffer(const String& type, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "MediaSource::addSourceBuffer(%s) %p", type.ascii().data(), this);

    // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
    // 1. If type is an empty string then throw an InvalidAccessError exception
    // and abort these steps.
    if (type.isEmpty()) {
        exceptionState.throwDOMException(InvalidAccessError, "The type provided is empty.");
        return 0;
    }

    // 2. If type contains a MIME type that is not supported ..., then throw a
    // NotSupportedError exception and abort these steps.
    if (!isTypeSupported(type)) {
        exceptionState.throwDOMException(NotSupportedError, "The type provided ('" + type + "') is unsupported.");
        return 0;
    }

    // 4. If the readyState attribute is not in the "open" state then throw an
    // InvalidStateError exception and abort these steps.
    if (!isOpen()) {
        exceptionState.throwDOMException(InvalidStateError, "The MediaSource's readyState is not 'open'.");
        return 0;
    }

    // 5. Create a new SourceBuffer object and associated resources.
    ContentType contentType(type);
    Vector<String> codecs = contentType.codecs();
    OwnPtr<WebSourceBuffer> webSourceBuffer = createWebSourceBuffer(contentType.type(), codecs, exceptionState);

    if (!webSourceBuffer) {
        ASSERT(exceptionState.code() == NotSupportedError || exceptionState.code() == QuotaExceededError);
        // 2. If type contains a MIME type that is not supported ..., then throw a NotSupportedError exception and abort these steps.
        // 3. If the user agent can't handle any more SourceBuffer objects then throw a QuotaExceededError exception and abort these steps
        return 0;
    }

    RefPtrWillBeRawPtr<SourceBuffer> buffer = SourceBuffer::create(webSourceBuffer.release(), this, asyncEventQueue());
    // 6. Add the new object to sourceBuffers and fire a addsourcebuffer on that object.
    m_sourceBuffers->add(buffer);
    m_activeSourceBuffers->add(buffer);
    // 7. Return the new object to the caller.
    return buffer.get();
}

void MediaSource::removeSourceBuffer(SourceBuffer* buffer, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "MediaSource::removeSourceBuffer() %p", this);
    RefPtr<SourceBuffer> protect(buffer);

    // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-removeSourceBuffer-void-SourceBuffer-sourceBuffer

    // 1. If sourceBuffer specifies an object that is not in sourceBuffers then
    // throw a NotFoundError exception and abort these steps.
    if (!m_sourceBuffers->length() || !m_sourceBuffers->contains(buffer)) {
        exceptionState.throwDOMException(NotFoundError, "The SourceBuffer provided is not contained in this MediaSource.");
        return;
    }

    // 2. If the sourceBuffer.updating attribute equals true, then run the following steps: ...
    buffer->abortIfUpdating();

    // Steps 3-8 are related to updating audioTracks, videoTracks, and textTracks which aren't implmented yet.
    // FIXME(91649): support track selection

    // 9. If sourceBuffer is in activeSourceBuffers, then remove sourceBuffer from activeSourceBuffers ...
    m_activeSourceBuffers->remove(buffer);

    // 10. Remove sourceBuffer from sourceBuffers and fire a removesourcebuffer event
    // on that object.
    m_sourceBuffers->remove(buffer);

    // 11. Destroy all resources for sourceBuffer.
    buffer->removedFromMediaSource();
}

void MediaSource::onReadyStateChange(const AtomicString& oldState, const AtomicString& newState)
{
    if (isOpen()) {
        scheduleEvent(EventTypeNames::sourceopen);
        return;
    }

    if (oldState == openKeyword() && newState == endedKeyword()) {
        scheduleEvent(EventTypeNames::sourceended);
        return;
    }

    ASSERT(isClosed());

    m_activeSourceBuffers->clear();

    // Clear SourceBuffer references to this object.
    for (unsigned long i = 0; i < m_sourceBuffers->length(); ++i)
        m_sourceBuffers->item(i)->removedFromMediaSource();
    m_sourceBuffers->clear();

    scheduleEvent(EventTypeNames::sourceclose);
}

Vector<RefPtr<TimeRanges> > MediaSource::activeRanges() const
{
    Vector<RefPtr<TimeRanges> > activeRanges(m_activeSourceBuffers->length());
    for (size_t i = 0; i < m_activeSourceBuffers->length(); ++i)
        activeRanges[i] = m_activeSourceBuffers->item(i)->buffered(ASSERT_NO_EXCEPTION);

    return activeRanges;
}

bool MediaSource::isUpdating() const
{
    // Return true if any member of |m_sourceBuffers| is updating.
    for (unsigned long i = 0; i < m_sourceBuffers->length(); ++i) {
        if (m_sourceBuffers->item(i)->updating())
            return true;
    }

    return false;
}

bool MediaSource::isTypeSupported(const String& type)
{
    WTF_LOG(Media, "MediaSource::isTypeSupported(%s)", type.ascii().data());

    // Section 2.2 isTypeSupported() method steps.
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#widl-MediaSource-isTypeSupported-boolean-DOMString-type
    // 1. If type is an empty string, then return false.
    if (type.isNull() || type.isEmpty())
        return false;

    ContentType contentType(type);
    String codecs = contentType.parameter("codecs");

    // 2. If type does not contain a valid MIME type string, then return false.
    if (contentType.type().isEmpty())
        return false;

    // 3. If type contains a media type or media subtype that the MediaSource does not support, then return false.
    // 4. If type contains at a codec that the MediaSource does not support, then return false.
    // 5. If the MediaSource does not support the specified combination of media type, media subtype, and codecs then return false.
    // 6. Return true.
    return MIMETypeRegistry::isSupportedMediaSourceMIMEType(contentType.type(), codecs);
}

const AtomicString& MediaSource::interfaceName() const
{
    return EventTargetNames::MediaSource;
}

void MediaSource::trace(Visitor* visitor)
{
    visitor->trace(m_sourceBuffers);
    visitor->trace(m_activeSourceBuffers);
    MediaSourceBase::trace(visitor);
}

} // namespace WebCore
