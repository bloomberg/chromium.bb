// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "modules/mediarecorder/MediaRecorder.h"

#include "core/dom/DOMError.h"
#include "core/fileapi/Blob.h"
#include "modules/EventModules.h"
#include "modules/EventTargetModules.h"
#include "modules/mediarecorder/BlobEvent.h"
#include "modules/mediarecorder/MediaRecorderErrorEvent.h"
#include "platform/NotImplemented.h"
#include "platform/blob/BlobData.h"
#include "public/platform/Platform.h"
#include "public/platform/WebMediaStream.h"

namespace blink {

namespace {

String stateToString(MediaRecorder::State state)
{
    switch (state) {
    case MediaRecorder::State::Inactive:
        return "inactive";
    case MediaRecorder::State::Recording:
        return "recording";
    case MediaRecorder::State::Paused:
        return "paused";
    }

    ASSERT_NOT_REACHED();
    return String();
}

} // namespace

MediaRecorder* MediaRecorder::create(ExecutionContext* context, MediaStream* stream, ExceptionState& exceptionState)
{
    MediaRecorder* recorder = new MediaRecorder(context, stream, String(), exceptionState);
    recorder->suspendIfNeeded();

    return recorder;
}

MediaRecorder* MediaRecorder::create(ExecutionContext* context, MediaStream* stream,  const String& mimeType, ExceptionState& exceptionState)
{
    MediaRecorder* recorder = new MediaRecorder(context, stream, mimeType, exceptionState);
    recorder->suspendIfNeeded();

    return recorder;
}

MediaRecorder::MediaRecorder(ExecutionContext* context, MediaStream* stream, const String& mimeType, ExceptionState& exceptionState)
    : ActiveDOMObject(context)
    , m_stream(stream)
    , m_mimeType(mimeType)
    , m_stopped(true)
    , m_ignoreMutedMedia(true)
    , m_state(State::Inactive)
    , m_dispatchScheduledEventRunner(this, &MediaRecorder::dispatchScheduledEvent)
{
    ASSERT(m_stream->getTracks().size());

    m_recorderHandler = adoptPtr(Platform::current()->createMediaRecorderHandler());
    ASSERT(m_recorderHandler);

    // We deviate from the spec by not returning |UnsupportedOption|, see https://github.com/w3c/mediacapture-record/issues/18
    if (!m_recorderHandler) {
        exceptionState.throwDOMException(NotSupportedError, "No MediaRecorder handler can be created.");
        return;
    }
    if (!m_recorderHandler->initialize(this, stream->descriptor(), m_mimeType)) {
        exceptionState.throwDOMException(NotSupportedError, "Failed to initialize native MediaRecorder, the type provided " + m_mimeType + "is unsupported." );
        return;
    }
    m_stopped = false;
}

String MediaRecorder::state() const
{
    return stateToString(m_state);
}

void MediaRecorder::start(ExceptionState& exceptionState)
{
    if (m_state != State::Inactive) {
        exceptionState.throwDOMException(InvalidStateError, "The MediaRecorder's state is '" + stateToString(m_state) + "'.");
        return;
    }
    m_state = State::Recording;

    m_recorderHandler->start();
}

void MediaRecorder::start(int timeSlice, ExceptionState& exceptionState)
{
    if (m_state != State::Inactive) {
        exceptionState.throwDOMException(InvalidStateError, "The MediaRecorder's state is '" + stateToString(m_state) + "'.");
        return;
    }
    m_state = State::Recording;

    m_recorderHandler->start(timeSlice);
    scheduleDispatchEvent(Event::create(EventTypeNames::start));
}

void MediaRecorder::stop(ExceptionState& exceptionState)
{
    if (m_state == State::Inactive) {
        exceptionState.throwDOMException(InvalidStateError, "The MediaRecorder's state is '" + stateToString(m_state) + "'.");
        return;
    }

    stopRecording();
}

void MediaRecorder::pause(ExceptionState& exceptionState)
{
    if (m_state == State::Inactive) {
        exceptionState.throwDOMException(InvalidStateError, "The MediaRecorder's state is '" + stateToString(m_state) + "'.");
        return;
    }
    if (m_state == State::Paused)
        return;

    m_state = State::Paused;

    m_recorderHandler->pause();

    scheduleDispatchEvent(Event::create(EventTypeNames::pause));
}

void MediaRecorder::resume(ExceptionState& exceptionState)
{
    if (m_state == State::Inactive) {
        exceptionState.throwDOMException(InvalidStateError, "The MediaRecorder's state is '" + stateToString(m_state) + "'.");
        return;
    }
    if (m_state == State::Recording)
        return;

    m_state = State::Recording;

    m_recorderHandler->resume();
    scheduleDispatchEvent(Event::create(EventTypeNames::resume));
}

void MediaRecorder::requestData(ExceptionState& exceptionState)
{
    if (m_state != State::Recording) {
        exceptionState.throwDOMException(InvalidStateError, "The MediaRecorder's state is '" + stateToString(m_state) + "'.");
        return;
    }

    createBlobEvent(nullptr);
}

String MediaRecorder::canRecordMimeType(const String& mimeType)
{
    RawPtr<WebMediaRecorderHandler> handler = Platform::current()->createMediaRecorderHandler();
    if (!handler)
        return emptyString();

    // MediaRecorder canRecordMimeType() MUST return 'probably' "if the UA is
    // confident that mimeType represents a type that it can record" [1], but a
    // number of reasons could prevent the recording from happening as expected,
    // so 'maybe' is a better option: "Implementors are encouraged to return
    // "maybe" unless the type can be confidently established as being supported
    // or not.". Hence this method returns '' or 'maybe', never 'probably'.
    // [1] http://w3c.github.io/mediacapture-record/MediaRecorder.html#methods
    return handler->canSupportMimeType(mimeType) ? "maybe" : emptyString();
}

const AtomicString& MediaRecorder::interfaceName() const
{
    return EventTargetNames::MediaRecorder;
}

ExecutionContext* MediaRecorder::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

void MediaRecorder::suspend()
{
    m_dispatchScheduledEventRunner.suspend();
}

void MediaRecorder::resume()
{
    m_dispatchScheduledEventRunner.resume();
}

void MediaRecorder::stop()
{
    if (m_stopped)
        return;

    m_stopped = true;
    m_stream.clear();
    m_recorderHandler.clear();

    scheduleDispatchEvent(Event::create(EventTypeNames::stop));
}

void MediaRecorder::writeData(const char* data, size_t length, bool lastInSlice)
{
    if (!lastInSlice && m_stopped) {
        m_stopped = false;
        scheduleDispatchEvent(Event::create(EventTypeNames::start));
    }

    // TODO(mcasas): Act as |m_ignoredMutedMedia| instructs if |m_stream| track(s) is in muted() state.
    // TODO(mcasas): Use |lastInSlice| to indicate to JS that recording is done.
    OwnPtr<BlobData> blobData = BlobData::create();
    blobData->appendBytes(data, length);
    createBlobEvent(Blob::create(BlobDataHandle::create(blobData.release(), length)));
}

void MediaRecorder::failOutOfMemory(const WebString& message)
{
    scheduleDispatchEvent(MediaRecorderErrorEvent::create(
        EventTypeNames::error, false, false, "OutOfMemory", message));

    if (m_state == State::Recording)
        stopRecording();
}

void MediaRecorder::failIllegalStreamModification(const WebString& message)
{
    scheduleDispatchEvent(MediaRecorderErrorEvent::create(
        EventTypeNames::error, false, false, "IllegalStreamModification", message));

    if (m_state == State::Recording)
        stopRecording();
}

void MediaRecorder::failOtherRecordingError(const WebString& message)
{
    scheduleDispatchEvent(MediaRecorderErrorEvent::create(
        EventTypeNames::error, false, false, "OtherRecordingError", message));

    if (m_state == State::Recording)
        stopRecording();
}

void MediaRecorder::createBlobEvent(Blob* blob)
{
    // TODO(mcasas): Consider launching an Event with a TypedArray inside, see https://github.com/w3c/mediacapture-record/issues/17.
    scheduleDispatchEvent(BlobEvent::create(EventTypeNames::dataavailable, blob));
}

void MediaRecorder::stopRecording()
{
    ASSERT(m_state != State::Inactive);
    m_state = State::Inactive;

    m_recorderHandler->stop();

    createBlobEvent(nullptr);

    scheduleDispatchEvent(Event::create(EventTypeNames::stop));
}

void MediaRecorder::scheduleDispatchEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    m_scheduledEvents.append(event);

    m_dispatchScheduledEventRunner.runAsync();
}

void MediaRecorder::dispatchScheduledEvent()
{
    WillBeHeapVector<RefPtrWillBeMember<Event>> events;
    events.swap(m_scheduledEvents);

    for (const auto& event : events)
        dispatchEvent(event);
}

DEFINE_TRACE(MediaRecorder)
{
    visitor->trace(m_stream);
    visitor->trace(m_scheduledEvents);
    RefCountedGarbageCollectedEventTargetWithInlineData<MediaRecorder>::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
