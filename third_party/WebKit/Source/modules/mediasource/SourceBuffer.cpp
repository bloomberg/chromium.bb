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

#include "modules/mediasource/SourceBuffer.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/DOMArrayBuffer.h"
#include "core/dom/DOMArrayBufferView.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/Event.h"
#include "core/events/GenericEventQueue.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/MediaError.h"
#include "core/html/TimeRanges.h"
#include "core/html/track/AudioTrack.h"
#include "core/html/track/AudioTrackList.h"
#include "core/html/track/VideoTrack.h"
#include "core/html/track/VideoTrackList.h"
#include "core/streams/Stream.h"
#include "modules/mediasource/MediaSource.h"
#include "modules/mediasource/SourceBufferTrackBaseSupplement.h"
#include "platform/Logging.h"
#include "platform/TraceEvent.h"
#include "public/platform/WebSourceBuffer.h"
#include "wtf/MathExtras.h"

#include <limits>
#include <sstream>

using blink::WebSourceBuffer;

namespace blink {

namespace {

static bool throwExceptionIfRemovedOrUpdating(bool isRemoved, bool isUpdating, ExceptionState& exceptionState)
{
    if (isRemoved) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidStateError, "This SourceBuffer has been removed from the parent media source.");
        return true;
    }
    if (isUpdating) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidStateError, "This SourceBuffer is still processing an 'appendBuffer', 'appendStream', or 'remove' operation.");
        return true;
    }

    return false;
}

#if !LOG_DISABLED
WTF::String webTimeRangesToString(const WebTimeRanges& ranges)
{
    StringBuilder stringBuilder;
    stringBuilder.append("{");
    for (auto& r : ranges) {
        stringBuilder.append(" [");
        stringBuilder.appendNumber(r.start);
        stringBuilder.append(";");
        stringBuilder.appendNumber(r.end);
        stringBuilder.append("]");
    }
    stringBuilder.append(" }");
    return stringBuilder.toString();
}
#endif

} // namespace

SourceBuffer* SourceBuffer::create(PassOwnPtr<WebSourceBuffer> webSourceBuffer, MediaSource* source, GenericEventQueue* asyncEventQueue)
{
    SourceBuffer* sourceBuffer = new SourceBuffer(std::move(webSourceBuffer), source, asyncEventQueue);
    sourceBuffer->suspendIfNeeded();
    return sourceBuffer;
}

SourceBuffer::SourceBuffer(PassOwnPtr<WebSourceBuffer> webSourceBuffer, MediaSource* source, GenericEventQueue* asyncEventQueue)
    : ActiveScriptWrappable(this)
    , ActiveDOMObject(source->getExecutionContext())
    , m_webSourceBuffer(std::move(webSourceBuffer))
    , m_source(source)
    , m_trackDefaults(TrackDefaultList::create())
    , m_asyncEventQueue(asyncEventQueue)
    , m_mode(segmentsKeyword())
    , m_updating(false)
    , m_timestampOffset(0)
    , m_appendWindowStart(0)
    , m_appendWindowEnd(std::numeric_limits<double>::infinity())
    , m_firstInitializationSegmentReceived(false)
    , m_pendingAppendDataOffset(0)
    , m_appendBufferAsyncPartRunner(AsyncMethodRunner<SourceBuffer>::create(this, &SourceBuffer::appendBufferAsyncPart))
    , m_pendingRemoveStart(-1)
    , m_pendingRemoveEnd(-1)
    , m_removeAsyncPartRunner(AsyncMethodRunner<SourceBuffer>::create(this, &SourceBuffer::removeAsyncPart))
    , m_streamMaxSizeValid(false)
    , m_streamMaxSize(0)
    , m_appendStreamAsyncPartRunner(AsyncMethodRunner<SourceBuffer>::create(this, &SourceBuffer::appendStreamAsyncPart))
{
    ASSERT(m_webSourceBuffer);
    ASSERT(m_source);
    ASSERT(m_source->mediaElement());
    ThreadState::current()->registerPreFinalizer(this);
    m_audioTracks = AudioTrackList::create(*m_source->mediaElement());
    m_videoTracks = VideoTrackList::create(*m_source->mediaElement());
    m_webSourceBuffer->setClient(this);
}

SourceBuffer::~SourceBuffer()
{
    WTF_LOG(Media, "SourceBuffer(%p)::~SourceBuffer", this);
}

void SourceBuffer::dispose()
{
    // Promptly clears a raw reference from content/ to an on-heap object
    // so that content/ doesn't access it in a lazy sweeping phase.
    m_webSourceBuffer.clear();
}

const AtomicString& SourceBuffer::segmentsKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, segments, ("segments"));
    return segments;
}

const AtomicString& SourceBuffer::sequenceKeyword()
{
    DEFINE_STATIC_LOCAL(const AtomicString, sequence, ("sequence"));
    return sequence;
}

void SourceBuffer::setMode(const AtomicString& newMode, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "SourceBuffer::setMode %p newMode=%s", this, newMode.utf8().data());
    // Section 3.1 On setting mode attribute steps.
    // 1. Let new mode equal the new value being assigned to this attribute.
    // 2. If this object has been removed from the sourceBuffers attribute of the parent media source, then throw
    //    an INVALID_STATE_ERR exception and abort these steps.
    // 3. If the updating attribute equals true, then throw an INVALID_STATE_ERR exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState))
        return;

    // 4. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    // 4.1 Set the readyState attribute of the parent media source to "open"
    // 4.2 Queue a task to fire a simple event named sourceopen at the parent media source.
    m_source->openIfInEndedState();

    // 5. If the append state equals PARSING_MEDIA_SEGMENT, then throw an INVALID_STATE_ERR and abort these steps.
    // 6. If the new mode equals "sequence", then set the group start timestamp to the highest presentation end timestamp.
    WebSourceBuffer::AppendMode appendMode = WebSourceBuffer::AppendModeSegments;
    if (newMode == sequenceKeyword())
        appendMode = WebSourceBuffer::AppendModeSequence;
    if (!m_webSourceBuffer->setMode(appendMode)) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidStateError, "The mode may not be set while the SourceBuffer's append state is 'PARSING_MEDIA_SEGMENT'.");
        return;
    }

    // 7. Update the attribute to new mode.
    m_mode = newMode;
}

TimeRanges* SourceBuffer::buffered(ExceptionState& exceptionState) const
{
    // Section 3.1 buffered attribute steps.
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    InvalidStateError exception and abort these steps.
    if (isRemoved()) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidStateError, "This SourceBuffer has been removed from the parent media source.");
        return nullptr;
    }

    // 2. Return a new static normalized TimeRanges object for the media segments buffered.
    return TimeRanges::create(m_webSourceBuffer->buffered());
}

double SourceBuffer::timestampOffset() const
{
    return m_timestampOffset;
}

void SourceBuffer::setTimestampOffset(double offset, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "SourceBuffer::setTimestampOffset %p offset=%f", this, offset);
    // Section 3.1 timestampOffset attribute setter steps.
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#widl-SourceBuffer-timestampOffset
    // 1. Let new timestamp offset equal the new value being assigned to this attribute.
    // 2. If this object has been removed from the sourceBuffers attribute of the parent media source, then throw an
    //    InvalidStateError exception and abort these steps.
    // 3. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState))
        return;

    // 4. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    // 4.1 Set the readyState attribute of the parent media source to "open"
    // 4.2 Queue a task to fire a simple event named sourceopen at the parent media source.
    m_source->openIfInEndedState();

    // 5. If the append state equals PARSING_MEDIA_SEGMENT, then throw an INVALID_STATE_ERR and abort these steps.
    // 6. If the mode attribute equals "sequence", then set the group start timestamp to new timestamp offset.
    if (!m_webSourceBuffer->setTimestampOffset(offset)) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidStateError, "The timestamp offset may not be set while the SourceBuffer's append state is 'PARSING_MEDIA_SEGMENT'.");
        return;
    }

    // 7. Update the attribute to new timestamp offset.
    m_timestampOffset = offset;
}

AudioTrackList& SourceBuffer::audioTracks()
{
    ASSERT(RuntimeEnabledFeatures::audioVideoTracksEnabled());
    return *m_audioTracks;
}

VideoTrackList& SourceBuffer::videoTracks()
{
    ASSERT(RuntimeEnabledFeatures::audioVideoTracksEnabled());
    return *m_videoTracks;
}

double SourceBuffer::appendWindowStart() const
{
    return m_appendWindowStart;
}

void SourceBuffer::setAppendWindowStart(double start, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "SourceBuffer::setAppendWindowStart %p start=%f", this, start);
    // Section 3.1 appendWindowStart attribute setter steps.
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#widl-SourceBuffer-appendWindowStart
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    InvalidStateError exception and abort these steps.
    // 2. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState))
        return;

    // 3. If the new value is less than 0 or greater than or equal to appendWindowEnd then throw an InvalidAccessError
    //    exception and abort these steps.
    if (start < 0 || start >= m_appendWindowEnd) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidAccessError, ExceptionMessages::indexOutsideRange("value", start, 0.0, ExceptionMessages::ExclusiveBound, m_appendWindowEnd, ExceptionMessages::InclusiveBound));
        return;
    }

    m_webSourceBuffer->setAppendWindowStart(start);

    // 4. Update the attribute to the new value.
    m_appendWindowStart = start;
}

double SourceBuffer::appendWindowEnd() const
{
    return m_appendWindowEnd;
}

void SourceBuffer::setAppendWindowEnd(double end, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "SourceBuffer::setAppendWindowEnd %p end=%f", this, end);
    // Section 3.1 appendWindowEnd attribute setter steps.
    // https://dvcs.w3.org/hg/html-media/raw-file/tip/media-source/media-source.html#widl-SourceBuffer-appendWindowEnd
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    InvalidStateError exception and abort these steps.
    // 2. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState))
        return;

    // 3. If the new value equals NaN, then throw an InvalidAccessError and abort these steps.
    if (std::isnan(end)) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidAccessError, ExceptionMessages::notAFiniteNumber(end));
        return;
    }
    // 4. If the new value is less than or equal to appendWindowStart then throw an InvalidAccessError
    //    exception and abort these steps.
    if (end <= m_appendWindowStart) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidAccessError, ExceptionMessages::indexExceedsMinimumBound("value", end, m_appendWindowStart));
        return;
    }

    m_webSourceBuffer->setAppendWindowEnd(end);

    // 5. Update the attribute to the new value.
    m_appendWindowEnd = end;
}

void SourceBuffer::appendBuffer(DOMArrayBuffer* data, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "SourceBuffer(%p)::appendBuffer size=%u", this, data->byteLength());
    // Section 3.2 appendBuffer()
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendBuffer-void-ArrayBufferView-data
    appendBufferInternal(static_cast<const unsigned char*>(data->data()), data->byteLength(), exceptionState);
}

void SourceBuffer::appendBuffer(DOMArrayBufferView* data, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "SourceBuffer(%p)::appendBuffer size=%u", this, data->byteLength());
    // Section 3.2 appendBuffer()
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendBuffer-void-ArrayBufferView-data
    appendBufferInternal(static_cast<const unsigned char*>(data->baseAddress()), data->byteLength(), exceptionState);
}

void SourceBuffer::appendStream(Stream* stream, ExceptionState& exceptionState)
{
    m_streamMaxSizeValid = false;
    appendStreamInternal(stream, exceptionState);
}

void SourceBuffer::appendStream(Stream* stream, unsigned long long maxSize, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "SourceBuffer(%p)::appendStream maxSize=%llu", this, maxSize);
    m_streamMaxSizeValid = maxSize > 0;
    if (m_streamMaxSizeValid)
        m_streamMaxSize = maxSize;
    appendStreamInternal(stream, exceptionState);
}

void SourceBuffer::abort(ExceptionState& exceptionState)
{
    WTF_LOG(Media, "SourceBuffer::abort %p", this);
    // Section 3.2 abort() method steps.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-abort-void
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source
    //    then throw an InvalidStateError exception and abort these steps.
    // 2. If the readyState attribute of the parent media source is not in the "open" state
    //    then throw an InvalidStateError exception and abort these steps.
    if (isRemoved()) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidStateError, "This SourceBuffer has been removed from the parent media source.");
        return;
    }
    if (!m_source->isOpen()) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidStateError, "The parent media source's readyState is not 'open'.");
        return;
    }

    // 3. If the sourceBuffer.updating attribute equals true, then run the following steps: ...
    abortIfUpdating();

    // 4. Run the reset parser state algorithm.
    m_webSourceBuffer->resetParserState();

    // 5. Set appendWindowStart to 0.
    setAppendWindowStart(0, exceptionState);

    // 6. Set appendWindowEnd to positive Infinity.
    setAppendWindowEnd(std::numeric_limits<double>::infinity(), exceptionState);
}

void SourceBuffer::remove(double start, double end, ExceptionState& exceptionState)
{
    WTF_LOG(Media, "SourceBuffer(%p)::remove start=%f end=%f", this, start, end);

    // Section 3.2 remove() method steps.
    // 1. If duration equals NaN, then throw an InvalidAccessError exception and abort these steps.
    // 2. If start is negative or greater than duration, then throw an InvalidAccessError exception and abort these steps.

    if (start < 0 || (m_source && (std::isnan(m_source->duration()) || start > m_source->duration()))) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidAccessError, ExceptionMessages::indexOutsideRange("start", start, 0.0, ExceptionMessages::ExclusiveBound, !m_source || std::isnan(m_source->duration()) ? 0 : m_source->duration(), ExceptionMessages::ExclusiveBound));
        return;
    }

    // 3. If end is less than or equal to start or end equals NaN, then throw an InvalidAccessError exception and abort these steps.
    if (end <= start || std::isnan(end)) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidAccessError, "The end value provided (" + String::number(end) + ") must be greater than the start value provided (" + String::number(start) + ").");
        return;
    }

    // 4. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    InvalidStateError exception and abort these steps.
    // 5. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState))
        return;

    TRACE_EVENT_ASYNC_BEGIN0("media", "SourceBuffer::remove", this);

    // 6. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    // 6.1. Set the readyState attribute of the parent media source to "open"
    // 6.2. Queue a task to fire a simple event named sourceopen at the parent media source .
    m_source->openIfInEndedState();

    // 7. Run the range removal algorithm with start and end as the start and end of the removal range.
    // 7.3. Set the updating attribute to true.
    m_updating = true;

    // 7.4. Queue a task to fire a simple event named updatestart at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updatestart);

    // 7.5. Return control to the caller and run the rest of the steps asynchronously.
    m_pendingRemoveStart = start;
    m_pendingRemoveEnd = end;
    m_removeAsyncPartRunner->runAsync();
}

void SourceBuffer::setTrackDefaults(TrackDefaultList* trackDefaults, ExceptionState& exceptionState)
{
    // Per 02 Dec 2014 Editor's Draft
    // http://w3c.github.io/media-source/#widl-SourceBuffer-trackDefaults
    // 1. If this object has been removed from the sourceBuffers attribute of
    //    the parent media source, then throw an InvalidStateError exception
    //    and abort these steps.
    // 2. If the updating attribute equals true, then throw an InvalidStateError
    //    exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState))
        return;

    // 3. Update the attribute to the new value.
    m_trackDefaults = trackDefaults;
}

void SourceBuffer::abortIfUpdating()
{
    // Section 3.2 abort() method step 3 substeps.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-abort-void

    if (!m_updating)
        return;

    const char* traceEventName = 0;
    if (!m_pendingAppendData.isEmpty()) {
        traceEventName = "SourceBuffer::appendBuffer";
    } else if (m_stream) {
        traceEventName = "SourceBuffer::appendStream";
    } else if (m_pendingRemoveStart != -1) {
        traceEventName = "SourceBuffer::remove";
    } else {
        ASSERT_NOT_REACHED();
    }

    // 3.1. Abort the buffer append and stream append loop algorithms if they are running.
    m_appendBufferAsyncPartRunner->stop();
    m_pendingAppendData.clear();
    m_pendingAppendDataOffset = 0;

    m_removeAsyncPartRunner->stop();
    m_pendingRemoveStart = -1;
    m_pendingRemoveEnd = -1;

    m_appendStreamAsyncPartRunner->stop();
    clearAppendStreamState();

    // 3.2. Set the updating attribute to false.
    m_updating = false;

    // 3.3. Queue a task to fire a simple event named abort at this SourceBuffer object.
    scheduleEvent(EventTypeNames::abort);

    // 3.4. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updateend);

    TRACE_EVENT_ASYNC_END0("media", traceEventName, this);
}

void SourceBuffer::removedFromMediaSource()
{
    if (isRemoved())
        return;

    WTF_LOG(Media, "SourceBuffer(%p)::removedFromMediaSource", this);
    abortIfUpdating();

    if (RuntimeEnabledFeatures::audioVideoTracksEnabled()) {
        ASSERT(m_source);
        if (m_source->mediaElement()->audioTracks().length() > 0
            || m_source->mediaElement()->videoTracks().length() > 0) {
            removeMediaTracks();
        }
    }

    m_webSourceBuffer->removedFromMediaSource();
    m_webSourceBuffer.clear();
    m_source = nullptr;
    m_asyncEventQueue = nullptr;
}

void SourceBuffer::removeMediaTracks()
{
    ASSERT(RuntimeEnabledFeatures::audioVideoTracksEnabled());
    // Spec: http://w3c.github.io/media-source/#widl-MediaSource-removeSourceBuffer-void-SourceBuffer-sourceBuffer
    ASSERT(m_source);

    HTMLMediaElement* mediaElement = m_source->mediaElement();
    ASSERT(mediaElement);
    // 3. Let SourceBuffer audioTracks list equal the AudioTrackList object returned by sourceBuffer.audioTracks.
    // 4. If the SourceBuffer audioTracks list is not empty, then run the following steps:
    // 4.1 Let HTMLMediaElement audioTracks list equal the AudioTrackList object returned by the audioTracks attribute on the HTMLMediaElement.
    // 4.2 Let the removed enabled audio track flag equal false.
    bool removedEnabledAudioTrack = false;
    // 4.3 For each AudioTrack object in the SourceBuffer audioTracks list, run the following steps:
    while (audioTracks().length() > 0) {
        AudioTrack* audioTrack = audioTracks().anonymousIndexedGetter(0);
        // 4.3.1 Set the sourceBuffer attribute on the AudioTrack object to null.
        SourceBufferTrackBaseSupplement::setSourceBuffer(*audioTrack, nullptr);
        // 4.3.2 If the enabled attribute on the AudioTrack object is true, then set the removed enabled audio track flag to true.
        if (audioTrack->enabled())
            removedEnabledAudioTrack = true;
        // 4.3.3 Remove the AudioTrack object from the HTMLMediaElement audioTracks list.
        // 4.3.4 Queue a task to fire a trusted event named removetrack, that does not bubble and is not cancelable, and that uses the TrackEvent interface, at the HTMLMediaElement audioTracks list.
        mediaElement->audioTracks().remove(audioTrack->trackId());
        // 4.3.5 Remove the AudioTrack object from the SourceBuffer audioTracks list.
        // 4.3.6 Queue a task to fire a trusted event named removetrack, that does not bubble and is not cancelable, and that uses the TrackEvent interface, at the SourceBuffer audioTracks list.
        audioTracks().remove(audioTrack->trackId());
    }
    // 4.4 If the removed enabled audio track flag equals true, then queue a task to fire a simple event named change at the HTMLMediaElement audioTracks list.
    if (removedEnabledAudioTrack) {
        Event* event = Event::create(EventTypeNames::change);
        event->setTarget(&mediaElement->audioTracks());
        mediaElement->scheduleEvent(event);
    }

    // 5. Let SourceBuffer videoTracks list equal the VideoTrackList object returned by sourceBuffer.videoTracks.
    // 6. If the SourceBuffer videoTracks list is not empty, then run the following steps:
    // 6.1 Let HTMLMediaElement videoTracks list equal the VideoTrackList object returned by the videoTracks attribute on the HTMLMediaElement.
    // 6.2 Let the removed selected video track flag equal false.
    bool removedSelectedVideoTrack = false;
    // 6.3 For each VideoTrack object in the SourceBuffer videoTracks list, run the following steps:
    while (videoTracks().length() > 0) {
        VideoTrack* videoTrack = videoTracks().anonymousIndexedGetter(0);
        // 6.3.1 Set the sourceBuffer attribute on the VideoTrack object to null.
        SourceBufferTrackBaseSupplement::setSourceBuffer(*videoTrack, nullptr);
        // 6.3.2 If the selected attribute on the VideoTrack object is true, then set the removed selected video track flag to true.
        if (videoTrack->selected())
            removedSelectedVideoTrack = true;
        // 6.3.3 Remove the VideoTrack object from the HTMLMediaElement videoTracks list.
        // 6.3.4 Queue a task to fire a trusted event named removetrack, that does not bubble and is not cancelable, and that uses the TrackEvent interface, at the HTMLMediaElement videoTracks list.
        mediaElement->videoTracks().remove(videoTrack->trackId());
        // 6.3.5 Remove the VideoTrack object from the SourceBuffer videoTracks list.
        // 6.3.6 Queue a task to fire a trusted event named removetrack, that does not bubble and is not cancelable, and that uses the TrackEvent interface, at the SourceBuffer videoTracks list.
        videoTracks().remove(videoTrack->trackId());
    }
    // 6.4 If the removed selected video track flag equals true, then queue a task to fire a simple event named change at the HTMLMediaElement videoTracks list.
    if (removedSelectedVideoTrack) {
        Event* event = Event::create(EventTypeNames::change);
        event->setTarget(&mediaElement->videoTracks());
        mediaElement->scheduleEvent(event);
    }

    // 7-8. TODO(servolk): Remove text tracks once SourceBuffer has text tracks.
}

template<class T>
T* findExistingTrackById(const TrackListBase<T>& trackList, const String& id)
{
    // According to MSE specification (https://w3c.github.io/media-source/#sourcebuffer-init-segment-received) step 3.1:
    // > If more than one track for a single type are present (ie 2 audio tracks), then the Track IDs match the ones in the first initialization segment.
    // I.e. we only need to search by TrackID if there is more than one track, otherwise we can assume that the only
    // track of the given type is the same one that we had in previous init segments.
    if (trackList.length() == 1)
        return trackList.anonymousIndexedGetter(0);
    return trackList.getTrackById(id);
}

WebVector<WebMediaPlayer::TrackId> SourceBuffer::initializationSegmentReceived(const WebVector<MediaTrackInfo>& newTracks)
{
    WTF_LOG(Media, "SourceBuffer::initializationSegmentReceived %p tracks=%zu", this, newTracks.size());
    ASSERT(m_source);
    ASSERT(m_source->mediaElement());
    ASSERT(m_updating);

    // TODO(servolk): Implement proper 'initialization segment received' algorithm according to MSE spec:
    // https://w3c.github.io/media-source/#sourcebuffer-init-segment-received
    WebVector<WebMediaPlayer::TrackId> result(newTracks.size());
    unsigned resultIdx = 0;
    for (const auto& trackInfo : newTracks) {
        if (!RuntimeEnabledFeatures::audioVideoTracksEnabled()) {
            static WebMediaPlayer::TrackId nextTrackId = 0;
            result[resultIdx++] = ++nextTrackId;
            continue;
        }

        const TrackBase* trackBase = nullptr;
        if (trackInfo.trackType == WebMediaPlayer::AudioTrack) {
            AudioTrack* audioTrack = nullptr;
            if (!m_firstInitializationSegmentReceived) {
                audioTrack = AudioTrack::create(trackInfo.byteStreamTrackId, trackInfo.kind, trackInfo.label, trackInfo.language, false);
                SourceBufferTrackBaseSupplement::setSourceBuffer(*audioTrack, this);
                audioTracks().add(audioTrack);
                m_source->mediaElement()->audioTracks().add(audioTrack);
            } else {
                audioTrack = findExistingTrackById(audioTracks(), trackInfo.byteStreamTrackId);
                ASSERT(audioTrack);
            }
            trackBase = audioTrack;
            result[resultIdx++] = audioTrack->trackId();
        } else if (trackInfo.trackType == WebMediaPlayer::VideoTrack) {
            VideoTrack* videoTrack = nullptr;
            if (!m_firstInitializationSegmentReceived) {
                videoTrack = VideoTrack::create(trackInfo.byteStreamTrackId, trackInfo.kind, trackInfo.label, trackInfo.language, false);
                SourceBufferTrackBaseSupplement::setSourceBuffer(*videoTrack, this);
                videoTracks().add(videoTrack);
                m_source->mediaElement()->videoTracks().add(videoTrack);
            } else {
                videoTrack = findExistingTrackById(videoTracks(), trackInfo.byteStreamTrackId);
                ASSERT(videoTrack);
            }
            trackBase = videoTrack;
            result[resultIdx++] = videoTrack->trackId();
        } else {
            NOTREACHED();
        }
        (void)trackBase;
#if !LOG_DISABLED
        const char* logActionStr = m_firstInitializationSegmentReceived ? "using existing" : "added";
        const char* logTrackTypeStr = (trackInfo.trackType == WebMediaPlayer::AudioTrack) ? "audio" : "video";
        WTF_LOG(Media, "Tracks (sb=%p): %s %sTrack %p trackId=%d id=%s label=%s lang=%s", this, logActionStr, logTrackTypeStr, trackBase, trackBase->trackId(), trackBase->id().utf8().data(), trackBase->label().utf8().data(), trackBase->language().utf8().data());
#endif
    }

    if (!m_firstInitializationSegmentReceived) {
        // 5. If active track flag equals true, then run the following steps:
        // 5.1. Add this SourceBuffer to activeSourceBuffers.
        // 5.2. Queue a task to fire a simple event named addsourcebuffer at
        // activesourcebuffers.
        m_source->setSourceBufferActive(this);

        // 6. Set first initialization segment received flag to true.
        m_firstInitializationSegmentReceived = true;
    }

    return result;
}

bool SourceBuffer::hasPendingActivity() const
{
    return m_source;
}

void SourceBuffer::suspend()
{
    m_appendBufferAsyncPartRunner->suspend();
    m_removeAsyncPartRunner->suspend();
    m_appendStreamAsyncPartRunner->suspend();
}

void SourceBuffer::resume()
{
    m_appendBufferAsyncPartRunner->resume();
    m_removeAsyncPartRunner->resume();
    m_appendStreamAsyncPartRunner->resume();
}

void SourceBuffer::stop()
{
    m_appendBufferAsyncPartRunner->stop();
    m_removeAsyncPartRunner->stop();
    m_appendStreamAsyncPartRunner->stop();
}

ExecutionContext* SourceBuffer::getExecutionContext() const
{
    return ActiveDOMObject::getExecutionContext();
}

const AtomicString& SourceBuffer::interfaceName() const
{
    return EventTargetNames::SourceBuffer;
}

bool SourceBuffer::isRemoved() const
{
    return !m_source;
}

void SourceBuffer::scheduleEvent(const AtomicString& eventName)
{
    ASSERT(m_asyncEventQueue);

    Event* event = Event::create(eventName);
    event->setTarget(this);

    m_asyncEventQueue->enqueueEvent(event);
}

bool SourceBuffer::prepareAppend(size_t newDataSize, ExceptionState& exceptionState)
{
    TRACE_EVENT_ASYNC_BEGIN0("media", "SourceBuffer::prepareAppend", this);
    // http://w3c.github.io/media-source/#sourcebuffer-prepare-append
    // 3.5.4 Prepare Append Algorithm
    // 1. If the SourceBuffer has been removed from the sourceBuffers attribute of the parent media source then throw an InvalidStateError exception and abort these steps.
    // 2. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (throwExceptionIfRemovedOrUpdating(isRemoved(), m_updating, exceptionState)) {
        TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::prepareAppend", this);
        return false;
    }

    // 3. If the HTMLMediaElement.error attribute is not null, then throw an InvalidStateError exception and abort these steps.
    ASSERT(m_source);
    ASSERT(m_source->mediaElement());
    if (m_source->mediaElement()->error()) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidStateError, "The HTMLMediaElement.error attribute is not null.");
        TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::prepareAppend", this);
        return false;
    }

    // 4. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    //    1. Set the readyState attribute of the parent media source to "open"
    //    2. Queue a task to fire a simple event named sourceopen at the parent media source.
    m_source->openIfInEndedState();

    // 5. Run the coded frame eviction algorithm.
    if (!evictCodedFrames(newDataSize)) {
        // 6. If the buffer full flag equals true, then throw a QUOTA_EXCEEDED_ERR exception and abort these steps.
        WTF_LOG(Media, "SourceBuffer(%p)::prepareAppend -> throw QuotaExceededError", this);
        MediaSource::logAndThrowDOMException(exceptionState, QuotaExceededError, "The SourceBuffer is full, and cannot free space to append additional buffers.");
        TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::prepareAppend", this);
        return false;
    }

    TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::prepareAppend", this);
    return true;
}

bool SourceBuffer::evictCodedFrames(size_t newDataSize)
{
    ASSERT(m_source);
    ASSERT(m_source->mediaElement());
    double currentTime = m_source->mediaElement()->currentTime();
    bool result = m_webSourceBuffer->evictCodedFrames(currentTime, newDataSize);
    if (!result) {
        WTF_LOG(Media, "SourceBuffer(%p)::evictCodedFrames failed. newDataSize=%zu currentTime=%f buffered=%s", this, newDataSize, currentTime, webTimeRangesToString(m_webSourceBuffer->buffered()).utf8().data());
    }
    return result;
}

void SourceBuffer::appendBufferInternal(const unsigned char* data, unsigned size, ExceptionState& exceptionState)
{
    TRACE_EVENT_ASYNC_BEGIN1("media", "SourceBuffer::appendBuffer", this, "size", size);
    // Section 3.2 appendBuffer()
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendBuffer-void-ArrayBufferView-data

    // 1. Run the prepare append algorithm.
    if (!prepareAppend(size, exceptionState)) {
        TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::appendBuffer", this);
        return;
    }
    TRACE_EVENT_ASYNC_STEP_INTO0("media", "SourceBuffer::appendBuffer", this, "prepareAppend");

    // 2. Add data to the end of the input buffer.
    ASSERT(data || size == 0);
    if (data)
        m_pendingAppendData.append(data, size);
    m_pendingAppendDataOffset = 0;

    // 3. Set the updating attribute to true.
    m_updating = true;

    // 4. Queue a task to fire a simple event named updatestart at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updatestart);

    // 5. Asynchronously run the buffer append algorithm.
    m_appendBufferAsyncPartRunner->runAsync();

    TRACE_EVENT_ASYNC_STEP_INTO0("media", "SourceBuffer::appendBuffer", this, "initialDelay");
}

void SourceBuffer::appendBufferAsyncPart()
{
    ASSERT(m_updating);

    // Section 3.5.4 Buffer Append Algorithm
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-buffer-append

    // 1. Run the segment parser loop algorithm.
    // Step 2 doesn't apply since we run Step 1 synchronously here.
    ASSERT(m_pendingAppendData.size() >= m_pendingAppendDataOffset);
    size_t appendSize = m_pendingAppendData.size() - m_pendingAppendDataOffset;

    // Impose an arbitrary max size for a single append() call so that an append
    // doesn't block the renderer event loop very long. This value was selected
    // by looking at YouTube SourceBuffer usage across a variety of bitrates.
    // This value allows relatively large appends while keeping append() call
    // duration in the  ~5-15ms range.
    const size_t MaxAppendSize = 128 * 1024;
    if (appendSize > MaxAppendSize)
        appendSize = MaxAppendSize;

    TRACE_EVENT_ASYNC_STEP_INTO1("media", "SourceBuffer::appendBuffer", this, "appending", "appendSize", static_cast<unsigned>(appendSize));

    // |zero| is used for 0 byte appends so we always have a valid pointer.
    // We need to convey all appends, even 0 byte ones to |m_webSourceBuffer|
    // so that it can clear its end of stream state if necessary.
    unsigned char zero = 0;
    unsigned char* appendData = &zero;
    if (appendSize)
        appendData = m_pendingAppendData.data() + m_pendingAppendDataOffset;

    m_webSourceBuffer->append(appendData, appendSize, &m_timestampOffset);

    m_pendingAppendDataOffset += appendSize;

    if (m_pendingAppendDataOffset < m_pendingAppendData.size()) {
        m_appendBufferAsyncPartRunner->runAsync();
        TRACE_EVENT_ASYNC_STEP_INTO0("media", "SourceBuffer::appendBuffer", this, "nextPieceDelay");
        return;
    }

    // 3. Set the updating attribute to false.
    m_updating = false;
    m_pendingAppendData.clear();
    m_pendingAppendDataOffset = 0;

    // 4. Queue a task to fire a simple event named update at this SourceBuffer object.
    scheduleEvent(EventTypeNames::update);

    // 5. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updateend);
    TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::appendBuffer", this);
    WTF_LOG(Media, "SourceBuffer(%p)::appendBuffer ended. buffered=%s", this, webTimeRangesToString(m_webSourceBuffer->buffered()).utf8().data());
}

void SourceBuffer::removeAsyncPart()
{
    ASSERT(m_updating);
    ASSERT(m_pendingRemoveStart >= 0);
    ASSERT(m_pendingRemoveStart < m_pendingRemoveEnd);

    // Section 3.2 remove() method steps
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-remove-void-double-start-double-end

    // 9. Run the coded frame removal algorithm with start and end as the start and end of the removal range.
    m_webSourceBuffer->remove(m_pendingRemoveStart, m_pendingRemoveEnd);

    // 10. Set the updating attribute to false.
    m_updating = false;
    m_pendingRemoveStart = -1;
    m_pendingRemoveEnd = -1;

    // 11. Queue a task to fire a simple event named update at this SourceBuffer object.
    scheduleEvent(EventTypeNames::update);

    // 12. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updateend);
}

void SourceBuffer::appendStreamInternal(Stream* stream, ExceptionState& exceptionState)
{
    TRACE_EVENT_ASYNC_BEGIN0("media", "SourceBuffer::appendStream", this);

    // Section 3.2 appendStream()
    // http://w3c.github.io/media-source/#widl-SourceBuffer-appendStream-void-ReadableStream-stream-unsigned-long-long-maxSize
    // (0. If the stream has been neutered, then throw an InvalidAccessError exception and abort these steps.)
    if (stream->isNeutered()) {
        MediaSource::logAndThrowDOMException(exceptionState, InvalidAccessError, "The stream provided has been neutered.");
        TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::appendStream", this);
        return;
    }

    // 1. Run the prepare append algorithm.
    size_t newDataSize = m_streamMaxSizeValid ? m_streamMaxSize : 0;
    if (!prepareAppend(newDataSize, exceptionState)) {
        TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::appendStream", this);
        return;
    }

    // 2. Set the updating attribute to true.
    m_updating = true;

    // 3. Queue a task to fire a simple event named updatestart at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updatestart);

    // 4. Asynchronously run the stream append loop algorithm with stream and maxSize.
    stream->neuter();
    m_loader = FileReaderLoader::create(FileReaderLoader::ReadByClient, this);
    m_stream = stream;
    m_appendStreamAsyncPartRunner->runAsync();
}

void SourceBuffer::appendStreamAsyncPart()
{
    ASSERT(m_updating);
    ASSERT(m_loader);
    ASSERT(m_stream);
    TRACE_EVENT_ASYNC_STEP_INTO0("media", "SourceBuffer::appendStream", this, "appendStreamAsyncPart");

    // Section 3.5.6 Stream Append Loop
    // http://w3c.github.io/media-source/#sourcebuffer-stream-append-loop

    // 1. If maxSize is set, then let bytesLeft equal maxSize.
    // 2. Loop Top: If maxSize is set and bytesLeft equals 0, then jump to the loop done step below.
    if (m_streamMaxSizeValid && !m_streamMaxSize) {
        appendStreamDone(true);
        return;
    }

    // Steps 3-11 are handled by m_loader.
    // Note: Passing 0 here signals that maxSize was not set. (i.e. Read all the data in the stream).
    m_loader->start(getExecutionContext(), *m_stream, m_streamMaxSizeValid ? m_streamMaxSize : 0);
}

void SourceBuffer::appendStreamDone(bool success)
{
    ASSERT(m_updating);
    ASSERT(m_loader);
    ASSERT(m_stream);

    clearAppendStreamState();

    if (!success) {
        appendError(false);
        TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::appendStream", this);
        return;
    }

    // Section 3.5.6 Stream Append Loop
    // Steps 1-11 are handled by appendStreamAsyncPart(), |m_loader|, and |m_webSourceBuffer|.

    // 12. Loop Done: Set the updating attribute to false.
    m_updating = false;

    // 13. Queue a task to fire a simple event named update at this SourceBuffer object.
    scheduleEvent(EventTypeNames::update);

    // 14. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updateend);
    TRACE_EVENT_ASYNC_END0("media", "SourceBuffer::appendStream", this);
    WTF_LOG(Media, "SourceBuffer(%p)::appendStream ended. buffered=%s", this, webTimeRangesToString(m_webSourceBuffer->buffered()).utf8().data());
}

void SourceBuffer::clearAppendStreamState()
{
    m_streamMaxSizeValid = false;
    m_streamMaxSize = 0;
    m_loader.clear();
    m_stream = nullptr;
}

void SourceBuffer::appendError(bool decodeError)
{
    WTF_LOG(Media, "SourceBuffer::appendError %p decodeError=%d", this, decodeError);
    // Section 3.5.3 Append Error Algorithm
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-append-error

    // 1. Run the reset parser state algorithm.
    m_webSourceBuffer->resetParserState();

    // 2. Set the updating attribute to false.
    m_updating = false;

    // 3. Queue a task to fire a simple event named error at this SourceBuffer object.
    scheduleEvent(EventTypeNames::error);

    // 4. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(EventTypeNames::updateend);

    // 5. If decode error is true, then run the end of stream algorithm with the
    // error parameter set to "decode".
    if (decodeError)
        m_source->endOfStream("decode", ASSERT_NO_EXCEPTION);
}

void SourceBuffer::didStartLoading()
{
    WTF_LOG(Media, "SourceBuffer(%p)::didStartLoading", this);
}

void SourceBuffer::didReceiveDataForClient(const char* data, unsigned dataLength)
{
    WTF_LOG(Media, "SourceBuffer(%p)::didReceiveDataForClient dataLength=%u", this, dataLength);
    ASSERT(m_updating);
    ASSERT(m_loader);

    // Section 3.5.6 Stream Append Loop
    // http://w3c.github.io/media-source/#sourcebuffer-stream-append-loop

    // 10. Run the coded frame eviction algorithm.
    if (!evictCodedFrames(dataLength)) {
        // 11. (in appendStreamDone) If the buffer full flag equals true, then run the append error algorithm with the decode error parameter set to false and abort this algorithm.
        appendStreamDone(false);
        return;
    }

    m_webSourceBuffer->append(reinterpret_cast<const unsigned char*>(data), dataLength, &m_timestampOffset);
}

void SourceBuffer::didFinishLoading()
{
    WTF_LOG(Media, "SourceBuffer(%p)::didFinishLoading", this);
    ASSERT(m_loader);
    appendStreamDone(true);
}

void SourceBuffer::didFail(FileError::ErrorCode errorCode)
{
    WTF_LOG(Media, "SourceBuffer(%p)::didFail errorCode=%d", this, errorCode);
    // m_loader might be already released, in case appendStream has failed due
    // to evictCodedFrames failing in didReceiveDataForClient. In that case
    // appendStreamDone will be invoked from there, no need to repeat it here.
    if (m_loader)
        appendStreamDone(false);
}

DEFINE_TRACE(SourceBuffer)
{
    visitor->trace(m_source);
    visitor->trace(m_trackDefaults);
    visitor->trace(m_asyncEventQueue);
    visitor->trace(m_appendBufferAsyncPartRunner);
    visitor->trace(m_removeAsyncPartRunner);
    visitor->trace(m_appendStreamAsyncPartRunner);
    visitor->trace(m_stream);
    visitor->trace(m_audioTracks);
    visitor->trace(m_videoTracks);
    EventTargetWithInlineData::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
