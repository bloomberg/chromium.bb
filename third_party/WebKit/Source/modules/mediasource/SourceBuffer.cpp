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
#include "modules/mediasource/SourceBuffer.h"

#include "bindings/v8/ExceptionState.h"
#include "core/dom/Event.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/GenericEventQueue.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/fileapi/FileReaderLoader.h"
#include "core/fileapi/Stream.h"
#include "core/html/TimeRanges.h"
#include "core/platform/Logging.h"
#include "core/platform/graphics/SourceBufferPrivate.h"
#include "modules/mediasource/MediaSource.h"
#include "wtf/ArrayBuffer.h"
#include "wtf/ArrayBufferView.h"
#include "wtf/MathExtras.h"

#include <limits>

namespace WebCore {

PassRefPtr<SourceBuffer> SourceBuffer::create(PassOwnPtr<SourceBufferPrivate> sourceBufferPrivate, MediaSource* source, GenericEventQueue* asyncEventQueue)
{
    RefPtr<SourceBuffer> sourceBuffer(adoptRef(new SourceBuffer(sourceBufferPrivate, source, asyncEventQueue)));
    sourceBuffer->suspendIfNeeded();
    return sourceBuffer.release();
}

SourceBuffer::SourceBuffer(PassOwnPtr<SourceBufferPrivate> sourceBufferPrivate, MediaSource* source, GenericEventQueue* asyncEventQueue)
    : ActiveDOMObject(source->scriptExecutionContext())
    , m_private(sourceBufferPrivate)
    , m_source(source)
    , m_asyncEventQueue(asyncEventQueue)
    , m_updating(false)
    , m_timestampOffset(0)
    , m_appendWindowStart(0)
    , m_appendWindowEnd(std::numeric_limits<double>::infinity())
    , m_appendBufferTimer(this, &SourceBuffer::appendBufferTimerFired)
    , m_removeTimer(this, &SourceBuffer::removeTimerFired)
    , m_pendingRemoveStart(-1)
    , m_pendingRemoveEnd(-1)
    , m_streamMaxSizeValid(false)
    , m_streamMaxSize(0)
    , m_appendStreamTimer(this, &SourceBuffer::appendStreamTimerFired)
{
    ASSERT(m_private);
    ASSERT(m_source);
    ScriptWrappable::init(this);
}

SourceBuffer::~SourceBuffer()
{
    ASSERT(isRemoved());
    ASSERT(!m_loader);
    ASSERT(!m_stream);
}

PassRefPtr<TimeRanges> SourceBuffer::buffered(ExceptionState& es) const
{
    // Section 3.1 buffered attribute steps.
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    InvalidStateError exception and abort these steps.
    if (isRemoved()) {
        es.throwDOMException(InvalidStateError);
        return 0;
    }

    // 2. Return a new static normalized TimeRanges object for the media segments buffered.
    return m_private->buffered();
}

double SourceBuffer::timestampOffset() const
{
    return m_timestampOffset;
}

void SourceBuffer::setTimestampOffset(double offset, ExceptionState& es)
{
    // Section 3.1 timestampOffset attribute setter steps.
    // 1. Let new timestamp offset equal the new value being assigned to this attribute.
    // 2. If this object has been removed from the sourceBuffers attribute of the parent media source, then throw an
    //    InvalidStateError exception and abort these steps.
    // 3. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (isRemoved() || m_updating) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    // 4. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    // 4.1 Set the readyState attribute of the parent media source to "open"
    // 4.2 Queue a task to fire a simple event named sourceopen at the parent media source.
    m_source->openIfInEndedState();

    // 5. If this object is waiting for the end of a media segment to be appended, then throw an InvalidStateError
    // and abort these steps.
    //
    // FIXME: Add step 6 text when mode attribute is implemented.
    if (!m_private->setTimestampOffset(offset)) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    // 7. Update the attribute to new timestamp offset.
    m_timestampOffset = offset;
}

double SourceBuffer::appendWindowStart() const
{
    return m_appendWindowStart;
}

void SourceBuffer::setAppendWindowStart(double start, ExceptionState& es)
{
    // Enforce throwing an exception on restricted double values.
    if (std::isnan(start)
        || start == std::numeric_limits<double>::infinity()
        || start == -std::numeric_limits<double>::infinity()) {
        es.throwDOMException(TypeMismatchError);
        return;
    }

    // Section 3.1 appendWindowStart attribute setter steps.
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    InvalidStateError exception and abort these steps.
    // 2. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (isRemoved() || m_updating) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    // 3. If the new value is less than 0 or greater than or equal to appendWindowEnd then throw an InvalidAccessError
    //    exception and abort these steps.
    if (start < 0 || start >= m_appendWindowEnd) {
        es.throwDOMException(InvalidAccessError);
        return;
    }

    m_private->setAppendWindowStart(start);

    // 4. Update the attribute to the new value.
    m_appendWindowStart = start;
}

double SourceBuffer::appendWindowEnd() const
{
    return m_appendWindowEnd;
}

void SourceBuffer::setAppendWindowEnd(double end, ExceptionState& es)
{
    // Section 3.1 appendWindowEnd attribute setter steps.
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    InvalidStateError exception and abort these steps.
    // 2. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (isRemoved() || m_updating) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    // 3. If the new value equals NaN, then throw an InvalidAccessError and abort these steps.
    // 4. If the new value is less than or equal to appendWindowStart then throw an InvalidAccessError
    //    exception and abort these steps.
    if (std::isnan(end) || end <= m_appendWindowStart) {
        es.throwDOMException(InvalidAccessError);
        return;
    }

    m_private->setAppendWindowEnd(end);

    // 5. Update the attribute to the new value.
    m_appendWindowEnd = end;
}

void SourceBuffer::appendBuffer(PassRefPtr<ArrayBuffer> data, ExceptionState& es)
{
    // Section 3.2 appendBuffer()
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendBuffer-void-ArrayBufferView-data
    // 1. If data is null then throw an InvalidAccessError exception and abort these steps.
    if (!data) {
        es.throwDOMException(InvalidAccessError);
        return;
    }

    appendBufferInternal(static_cast<const unsigned char*>(data->data()), data->byteLength(), es);
}

void SourceBuffer::appendBuffer(PassRefPtr<ArrayBufferView> data, ExceptionState& es)
{
    // Section 3.2 appendBuffer()
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendBuffer-void-ArrayBufferView-data
    // 1. If data is null then throw an InvalidAccessError exception and abort these steps.
    if (!data) {
        es.throwDOMException(InvalidAccessError);
        return;
    }

    appendBufferInternal(static_cast<const unsigned char*>(data->baseAddress()), data->byteLength(), es);
}

void SourceBuffer::appendStream(PassRefPtr<Stream> stream, ExceptionState& es)
{
    m_streamMaxSizeValid = false;
    appendStreamInternal(stream, es);
}

void SourceBuffer::appendStream(PassRefPtr<Stream> stream, unsigned long long maxSize, ExceptionState& es)
{
    m_streamMaxSizeValid = maxSize > 0;
    if (m_streamMaxSizeValid)
        m_streamMaxSize = maxSize;
    appendStreamInternal(stream, es);
}

void SourceBuffer::abort(ExceptionState& es)
{
    // Section 3.2 abort() method steps.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-abort-void
    // 1. If this object has been removed from the sourceBuffers attribute of the parent media source
    //    then throw an InvalidStateError exception and abort these steps.
    // 2. If the readyState attribute of the parent media source is not in the "open" state
    //    then throw an InvalidStateError exception and abort these steps.
    if (isRemoved() || !m_source->isOpen()) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    // 3. If the sourceBuffer.updating attribute equals true, then run the following steps: ...
    abortIfUpdating();

    // 4. Run the reset parser state algorithm.
    m_private->abort();

    // 5. Set appendWindowStart to 0.
    setAppendWindowStart(0, es);

    // 6. Set appendWindowEnd to positive Infinity.
    setAppendWindowEnd(std::numeric_limits<double>::infinity(), es);
}

void SourceBuffer::remove(double start, double end, ExceptionState& es)
{
    // Section 3.2 remove() method steps.
    // 1. If start is negative or greater than duration, then throw an InvalidAccessError exception and abort these steps.
    // 2. If end is less than or equal to start, then throw an InvalidAccessError exception and abort these steps.
    if (start < 0 || (m_source && (std::isnan(m_source->duration()) || start > m_source->duration())) || end <= start) {
        es.throwDOMException(InvalidAccessError);
        return;
    }

    // 3. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an
    //    InvalidStateError exception and abort these steps.
    // 4. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (isRemoved() || m_updating) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    // 5. If the readyState attribute of the parent media source is in the "ended" state then run the following steps:
    // 5.1. Set the readyState attribute of the parent media source to "open"
    // 5.2. Queue a task to fire a simple event named sourceopen at the parent media source .
    m_source->openIfInEndedState();

    // 6. Set the updating attribute to true.
    m_updating = true;

    // 7. Queue a task to fire a simple event named updatestart at this SourceBuffer object.
    scheduleEvent(eventNames().updatestartEvent);

    // 8. Return control to the caller and run the rest of the steps asynchronously.
    m_pendingRemoveStart = start;
    m_pendingRemoveEnd = end;
    m_removeTimer.startOneShot(0);
}

void SourceBuffer::abortIfUpdating()
{
    // Section 3.2 abort() method step 3 substeps.
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-abort-void

    if (!m_updating)
        return;

    // 3.1. Abort the buffer append and stream append loop algorithms if they are running.
    m_appendBufferTimer.stop();
    m_pendingAppendData.clear();

    m_removeTimer.stop();
    m_pendingRemoveStart = -1;
    m_pendingRemoveEnd = -1;

    m_appendStreamTimer.stop();
    clearAppendStreamState();

    // 3.2. Set the updating attribute to false.
    m_updating = false;

    // 3.3. Queue a task to fire a simple event named abort at this SourceBuffer object.
    scheduleEvent(eventNames().abortEvent);

    // 3.4. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(eventNames().updateendEvent);
}

void SourceBuffer::removedFromMediaSource()
{
    if (isRemoved())
        return;

    abortIfUpdating();

    m_private->removedFromMediaSource();
    m_source = 0;
    m_asyncEventQueue = 0;
}

bool SourceBuffer::hasPendingActivity() const
{
    return m_source;
}

void SourceBuffer::stop()
{
    m_appendBufferTimer.stop();
    m_removeTimer.stop();
    m_appendStreamTimer.stop();
}

ScriptExecutionContext* SourceBuffer::scriptExecutionContext() const
{
    return ActiveDOMObject::scriptExecutionContext();
}

const AtomicString& SourceBuffer::interfaceName() const
{
    return eventNames().interfaceForSourceBuffer;
}

EventTargetData* SourceBuffer::eventTargetData()
{
    return &m_eventTargetData;
}

EventTargetData* SourceBuffer::ensureEventTargetData()
{
    return &m_eventTargetData;
}

bool SourceBuffer::isRemoved() const
{
    return !m_source;
}

void SourceBuffer::scheduleEvent(const AtomicString& eventName)
{
    ASSERT(m_asyncEventQueue);

    RefPtr<Event> event = Event::create(eventName);
    event->setTarget(this);

    m_asyncEventQueue->enqueueEvent(event.release());
}

void SourceBuffer::appendBufferInternal(const unsigned char* data, unsigned size, ExceptionState& es)
{
    // Section 3.2 appendBuffer()
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendBuffer-void-ArrayBufferView-data

    // Step 1 is enforced by the caller.
    // 2. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an InvalidStateError exception and abort these steps.
    // 3. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (isRemoved() || m_updating) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    // 4. If the readyState attribute of the parent media source is in the "ended" state then run the following steps: ...
    m_source->openIfInEndedState();

    // Steps 5-6

    // 7. Add data to the end of the input buffer.
    m_pendingAppendData.append(data, size);

    // 8. Set the updating attribute to true.
    m_updating = true;

    // 9. Queue a task to fire a simple event named updatestart at this SourceBuffer object.
    scheduleEvent(eventNames().updatestartEvent);

    // 10. Asynchronously run the buffer append algorithm.
    m_appendBufferTimer.startOneShot(0);
}

void SourceBuffer::appendBufferTimerFired(Timer<SourceBuffer>*)
{
    ASSERT(m_updating);

    // Section 3.5.4 Buffer Append Algorithm
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-buffer-append

    // 1. Run the segment parser loop algorithm.
    // Step 2 doesn't apply since we run Step 1 synchronously here.
    size_t appendSize = m_pendingAppendData.size();
    if (!appendSize) {
        // Resize buffer for 0 byte appends so we always have a valid pointer.
        // We need to convey all appends, even 0 byte ones to |m_private| so
        // that it can clear its end of stream state if necessary.
        m_pendingAppendData.resize(1);
    }
    m_private->append(m_pendingAppendData.data(), appendSize);

    // 3. Set the updating attribute to false.
    m_updating = false;
    m_pendingAppendData.clear();

    // 4. Queue a task to fire a simple event named update at this SourceBuffer object.
    scheduleEvent(eventNames().updateEvent);

    // 5. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(eventNames().updateendEvent);
}

void SourceBuffer::removeTimerFired(Timer<SourceBuffer>*)
{
    ASSERT(m_updating);
    ASSERT(m_pendingRemoveStart >= 0);
    ASSERT(m_pendingRemoveStart < m_pendingRemoveEnd);

    // Section 3.2 remove() method steps
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-remove-void-double-start-double-end

    // 9. Run the coded frame removal algorithm with start and end as the start and end of the removal range.
    m_private->remove(m_pendingRemoveStart, m_pendingRemoveEnd);

    // 10. Set the updating attribute to false.
    m_updating = false;
    m_pendingRemoveStart = -1;
    m_pendingRemoveEnd = -1;

    // 11. Queue a task to fire a simple event named update at this SourceBuffer object.
    scheduleEvent(eventNames().updateEvent);

    // 12. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(eventNames().updateendEvent);
}

void SourceBuffer::appendStreamInternal(PassRefPtr<Stream> stream, ExceptionState& es)
{
    // Section 3.2 appendStream()
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#widl-SourceBuffer-appendStream-void-Stream-stream-unsigned-long-long-maxSize
    // 1. If stream is null then throw an InvalidAccessError exception and abort these steps.
    if (!stream || stream->isNeutered()) {
        es.throwDOMException(InvalidAccessError);
        return;
    }

    // 2. Run the prepare append algorithm.
    //  Section 3.5.4 Prepare Append Algorithm.
    //  https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-prepare-append
    //  1. If this object has been removed from the sourceBuffers attribute of the parent media source then throw an InvalidStateError exception and abort these steps.
    //  2. If the updating attribute equals true, then throw an InvalidStateError exception and abort these steps.
    if (isRemoved() || m_updating) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    //  3. If the readyState attribute of the parent media source is in the "ended" state then run the following steps: ...
    m_source->openIfInEndedState();

    //  Steps 4-5 of the prepare append algorithm are handled by m_private.

    // 3. Set the updating attribute to true.
    m_updating = true;

    // 4. Queue a task to fire a simple event named updatestart at this SourceBuffer object.
    scheduleEvent(eventNames().updatestartEvent);

    // 5. Asynchronously run the stream append loop algorithm with stream and maxSize.

    stream->neuter();
    m_loader = adoptPtr(new FileReaderLoader(FileReaderLoader::ReadByClient, this));
    m_stream = stream;
    m_appendStreamTimer.startOneShot(0);
}

void SourceBuffer::appendStreamTimerFired(Timer<SourceBuffer>*)
{
    ASSERT(m_updating);
    ASSERT(m_loader);
    ASSERT(m_stream);

    // Section 3.5.6 Stream Append Loop
    // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-stream-append-loop

    // 1. If maxSize is set, then let bytesLeft equal maxSize.
    // 2. Loop Top: If maxSize is set and bytesLeft equals 0, then jump to the loop done step below.
    if (m_streamMaxSizeValid && !m_streamMaxSize) {
        appendStreamDone(true);
        return;
    }

    // Steps 3-11 are handled by m_loader.
    // Note: Passing 0 here signals that maxSize was not set. (i.e. Read all the data in the stream).
    m_loader->start(scriptExecutionContext(), *m_stream, m_streamMaxSizeValid ? m_streamMaxSize : 0);
}

void SourceBuffer::appendStreamDone(bool success)
{
    ASSERT(m_updating);
    ASSERT(m_loader);
    ASSERT(m_stream);

    clearAppendStreamState();

    if (!success) {
        // Section 3.5.3 Append Error Algorithm
        // https://dvcs.w3.org/hg/html-media/raw-file/default/media-source/media-source.html#sourcebuffer-append-error
        //
        // 1. Run the reset parser state algorithm. (Handled by caller)
        // 2. Set the updating attribute to false.
        m_updating = false;

        // 3. Queue a task to fire a simple event named error at this SourceBuffer object.
        scheduleEvent(eventNames().errorEvent);

        // 4. Queue a task to fire a simple event named updateend at this SourceBuffer object.
        scheduleEvent(eventNames().updateendEvent);
        return;
    }

    // Section 3.5.6 Stream Append Loop
    // Steps 1-11 are handled by appendStreamTimerFired(), |m_loader|, and |m_private|.
    // 12. Loop Done: Set the updating attribute to false.
    m_updating = false;

    // 13. Queue a task to fire a simple event named update at this SourceBuffer object.
    scheduleEvent(eventNames().updateEvent);

    // 14. Queue a task to fire a simple event named updateend at this SourceBuffer object.
    scheduleEvent(eventNames().updateendEvent);
}

void SourceBuffer::clearAppendStreamState()
{
    m_streamMaxSizeValid = false;
    m_streamMaxSize = 0;
    m_loader.clear();
    m_stream = 0;
}

void SourceBuffer::didStartLoading()
{
    LOG(Media, "SourceBuffer::didStartLoading() %p", this);
}

void SourceBuffer::didReceiveDataForClient(const char* data, unsigned dataLength)
{
    LOG(Media, "SourceBuffer::didReceiveDataForClient(%d) %p", dataLength, this);
    ASSERT(m_updating);
    ASSERT(m_loader);

    m_private->append(reinterpret_cast<const unsigned char*>(data), dataLength);
}

void SourceBuffer::didFinishLoading()
{
    LOG(Media, "SourceBuffer::didFinishLoading() %p", this);
    appendStreamDone(true);
}

void SourceBuffer::didFail(FileError::ErrorCode errorCode)
{
    LOG(Media, "SourceBuffer::didFail(%d) %p", errorCode, this);
    appendStreamDone(false);
}

} // namespace WebCore
