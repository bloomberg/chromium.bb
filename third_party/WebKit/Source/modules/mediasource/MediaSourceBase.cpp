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

#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/dom/ExceptionCode.h"
#include "core/events/Event.h"
#include "core/events/GenericEventQueue.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/TimeRanges.h"
#include "modules/mediasource/MediaSourceRegistry.h"
#include "platform/Logging.h"
#include "platform/TraceEvent.h"
#include "public/platform/WebMediaSource.h"
#include "public/platform/WebSourceBuffer.h"
#include "wtf/text/WTFString.h"

using blink::WebMediaSource;
using blink::WebSourceBuffer;

namespace WebCore {

namespace {

static bool throwExceptionIfClosedOrUpdating(bool isOpen, bool isUpdating, ExceptionState& exceptionState)
{
    if (!isOpen) {
        exceptionState.throwDOMException(InvalidStateError, "The MediaSource's readyState is not 'open'.");
        return true;
    }
    if (isUpdating) {
        exceptionState.throwDOMException(InvalidStateError, "The 'updating' attribute is true on one or more of this MediaSource's SourceBuffers.");
        return true;
    }

    return false;
}

} // namespace

MediaSourceBase::MediaSourceBase(ExecutionContext* context)
    : ActiveDOMObject(context)
    , m_readyState(closedKeyword())
    , m_asyncEventQueue(GenericEventQueue::create(this))
    , m_attachedElement(0)
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

void MediaSourceBase::setWebMediaSourceAndOpen(PassOwnPtr<WebMediaSource> webMediaSource)
{
    TRACE_EVENT_ASYNC_END0("media", "MediaSourceBase::attachToElement", this);
    ASSERT(webMediaSource);
    ASSERT(!m_webMediaSource);
    ASSERT(m_attachedElement);
    m_webMediaSource = webMediaSource;
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
    return isClosed() ? std::numeric_limits<float>::quiet_NaN() : m_webMediaSource->duration();
}

PassRefPtr<TimeRanges> MediaSourceBase::buffered() const
{
    // Implements MediaSource algorithm for HTMLMediaElement.buffered.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#htmlmediaelement-extensions
    Vector<RefPtr<TimeRanges> > ranges = activeRanges();

    // 1. If activeSourceBuffers.length equals 0 then return an empty TimeRanges object and abort these steps.
    if (ranges.isEmpty())
        return TimeRanges::create();

    // 2. Let active ranges be the ranges returned by buffered for each SourceBuffer object in activeSourceBuffers.
    // 3. Let highest end time be the largest range end time in the active ranges.
    double highestEndTime = -1;
    for (size_t i = 0; i < ranges.size(); ++i) {
        unsigned length = ranges[i]->length();
        if (length)
            highestEndTime = std::max(highestEndTime, ranges[i]->end(length - 1, ASSERT_NO_EXCEPTION));
    }

    // Return an empty range if all ranges are empty.
    if (highestEndTime < 0)
        return TimeRanges::create();

    // 4. Let intersection ranges equal a TimeRange object containing a single range from 0 to highest end time.
    RefPtr<TimeRanges> intersectionRanges = TimeRanges::create(0, highestEndTime);

    // 5. For each SourceBuffer object in activeSourceBuffers run the following steps:
    bool ended = readyState() == endedKeyword();
    for (size_t i = 0; i < ranges.size(); ++i) {
        // 5.1 Let source ranges equal the ranges returned by the buffered attribute on the current SourceBuffer.
        TimeRanges* sourceRanges = ranges[i].get();

        // 5.2 If readyState is "ended", then set the end time on the last range in source ranges to highest end time.
        if (ended && sourceRanges->length())
            sourceRanges->add(sourceRanges->start(sourceRanges->length() - 1, ASSERT_NO_EXCEPTION), highestEndTime);

        // 5.3 Let new intersection ranges equal the the intersection between the intersection ranges and the source ranges.
        // 5.4 Replace the ranges in intersection ranges with the new intersection ranges.
        intersectionRanges->intersectWith(sourceRanges);
    }

    return intersectionRanges.release();
}

void MediaSourceBase::setDuration(double duration, ExceptionState& exceptionState)
{
    // 2.1 http://www.w3.org/TR/media-source/#widl-MediaSource-duration
    // 1. If the value being set is negative or NaN then throw an InvalidAccessError
    // exception and abort these steps.
    if (std::isnan(duration)) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::notAFiniteNumber(duration, "duration"));
        return;
    }
    if (duration < 0.0) {
        exceptionState.throwDOMException(InvalidAccessError, ExceptionMessages::indexExceedsMinimumBound("duration", duration, 0.0));
        return;
    }

    // 2. If the readyState attribute is not "open" then throw an InvalidStateError
    // exception and abort these steps.
    // 3. If the updating attribute equals true on any SourceBuffer in sourceBuffers,
    // then throw an InvalidStateError exception and abort these steps.
    if (throwExceptionIfClosedOrUpdating(isOpen(), isUpdating(), exceptionState))
        return;

    // 4. Run the duration change algorithm with new duration set to the value being
    // assigned to this attribute.
    // Synchronously process duration change algorithm to enforce any required
    // seek is started prior to returning.
    m_attachedElement->durationChanged(duration);
    m_webMediaSource->setDuration(duration);
}

void MediaSourceBase::setReadyState(const AtomicString& state)
{
    ASSERT(state == openKeyword() || state == closedKeyword() || state == endedKeyword());

    AtomicString oldState = readyState();
    WTF_LOG(Media, "MediaSourceBase::setReadyState() %p : %s -> %s", this, oldState.ascii().data(), state.ascii().data());

    if (state == closedKeyword()) {
        m_webMediaSource.clear();
        m_attachedElement = 0;
    }

    if (oldState == state)
        return;

    m_readyState = state;

    onReadyStateChange(oldState, state);
}

void MediaSourceBase::endOfStream(const AtomicString& error, ExceptionState& exceptionState)
{
    DEFINE_STATIC_LOCAL(const AtomicString, network, ("network", AtomicString::ConstructFromLiteral));
    DEFINE_STATIC_LOCAL(const AtomicString, decode, ("decode", AtomicString::ConstructFromLiteral));

    if (error == network) {
        endOfStreamInternal(blink::WebMediaSource::EndOfStreamStatusNetworkError, exceptionState);
    } else if (error == decode) {
        endOfStreamInternal(blink::WebMediaSource::EndOfStreamStatusDecodeError, exceptionState);
    } else {
        ASSERT_NOT_REACHED(); // IDL enforcement should prevent this case.
        exceptionState.throwTypeError("parameter 1 is not a valid enum value.");
    }
}

void MediaSourceBase::endOfStream(ExceptionState& exceptionState)
{
    endOfStreamInternal(blink::WebMediaSource::EndOfStreamStatusNoError, exceptionState);
}

void MediaSourceBase::endOfStreamInternal(const blink::WebMediaSource::EndOfStreamStatus eosStatus, ExceptionState& exceptionState)
{
    // 2.2 http://www.w3.org/TR/media-source/#widl-MediaSource-endOfStream-void-EndOfStreamError-error
    // 1. If the readyState attribute is not in the "open" state then throw an
    // InvalidStateError exception and abort these steps.
    // 2. If the updating attribute equals true on any SourceBuffer in sourceBuffers, then throw an
    // InvalidStateError exception and abort these steps.
    if (throwExceptionIfClosedOrUpdating(isOpen(), isUpdating(), exceptionState))
        return;

    // 3. Run the end of stream algorithm with the error parameter set to error.
    //   1. Change the readyState attribute value to "ended".
    //   2. Queue a task to fire a simple event named sourceended at the MediaSource.
    setReadyState(endedKeyword());

    //   3. Do various steps based on |eosStatus|.
    m_webMediaSource->markEndOfStream(eosStatus);
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

bool MediaSourceBase::attachToElement(HTMLMediaElement* element)
{
    if (m_attachedElement)
        return false;

    ASSERT(isClosed());

    TRACE_EVENT_ASYNC_BEGIN0("media", "MediaSourceBase::attachToElement", this);
    m_attachedElement = element;
    return true;
}

void MediaSourceBase::openIfInEndedState()
{
    if (m_readyState != endedKeyword())
        return;

    setReadyState(openKeyword());
    m_webMediaSource->unmarkEndOfStream();
}

bool MediaSourceBase::hasPendingActivity() const
{
    return m_attachedElement || m_webMediaSource
        || m_asyncEventQueue->hasPendingEvents()
        || ActiveDOMObject::hasPendingActivity();
}

void MediaSourceBase::stop()
{
    m_asyncEventQueue->close();
    if (!isClosed())
        setReadyState(closedKeyword());
    m_webMediaSource.clear();
}

PassOwnPtr<WebSourceBuffer> MediaSourceBase::createWebSourceBuffer(const String& type, const Vector<String>& codecs, ExceptionState& exceptionState)
{
    WebSourceBuffer* webSourceBuffer = 0;

    // FIXME: Always use the new frame processor once it has stabilized enough. See http://crbug.com/249422.
    WebMediaSource::FrameProcessorChoice frameProcessorChoice = RuntimeEnabledFeatures::mediaSourceExperimentalEnabled() ?
        WebMediaSource::UseNewFrameProcessor : WebMediaSource::UseLegacyFrameProcessor;

    WTF_LOG(Media, "MediaSourceBase::createWebSourceBuffer() %p : frameProcessorChoice = %i", this, frameProcessorChoice);

    switch (m_webMediaSource->addSourceBuffer(type, codecs, frameProcessorChoice, &webSourceBuffer)) {
    case WebMediaSource::AddStatusOk:
        return adoptPtr(webSourceBuffer);
    case WebMediaSource::AddStatusNotSupported:
        ASSERT(!webSourceBuffer);
        // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
        // Step 2: If type contains a MIME type ... that is not supported with the types
        // specified for the other SourceBuffer objects in sourceBuffers, then throw
        // a NotSupportedError exception and abort these steps.
        exceptionState.throwDOMException(NotSupportedError, "The type provided ('" + type + "') is not supported.");
        return nullptr;
    case WebMediaSource::AddStatusReachedIdLimit:
        ASSERT(!webSourceBuffer);
        // 2.2 https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-MediaSource-addSourceBuffer-SourceBuffer-DOMString-type
        // Step 3: If the user agent can't handle any more SourceBuffer objects then throw
        // a QuotaExceededError exception and abort these steps.
        exceptionState.throwDOMException(QuotaExceededError, "This MediaSource has reached the limit of SourceBuffer objects it can handle. No additional SourceBuffer objects may be added.");
        return nullptr;
    }

    ASSERT_NOT_REACHED();
    return nullptr;
}

void MediaSourceBase::scheduleEvent(const AtomicString& eventName)
{
    ASSERT(m_asyncEventQueue);

    RefPtrWillBeRawPtr<Event> event = Event::create(eventName);
    event->setTarget(this);

    m_asyncEventQueue->enqueueEvent(event.release());
}

ExecutionContext* MediaSourceBase::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

URLRegistry& MediaSourceBase::registry() const
{
    return MediaSourceRegistry::registry();
}

}
