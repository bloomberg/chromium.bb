// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/mediarecorder/MediaRecorder.h"

#include "bindings/core/v8/Dictionary.h"
#include "core/events/Event.h"
#include "core/fileapi/Blob.h"
#include "modules/EventTargetModules.h"
#include "modules/mediarecorder/BlobEvent.h"
#include "platform/ContentType.h"
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
    MediaRecorder* recorder = new MediaRecorder(context, stream, MediaRecorderOptions(), exceptionState);
    recorder->suspendIfNeeded();

    return recorder;
}

MediaRecorder* MediaRecorder::create(ExecutionContext* context, MediaStream* stream,  const MediaRecorderOptions& options, ExceptionState& exceptionState)
{
    MediaRecorder* recorder = new MediaRecorder(context, stream, options, exceptionState);
    recorder->suspendIfNeeded();

    return recorder;
}

MediaRecorder::MediaRecorder(ExecutionContext* context, MediaStream* stream, const MediaRecorderOptions& options, ExceptionState& exceptionState)
    : ActiveDOMObject(context)
    , m_stream(stream)
    , m_streamAmountOfTracks(stream->getTracks().size())
    , m_mimeType(options.mimeType())
    , m_stopped(true)
    , m_ignoreMutedMedia(true)
    , m_state(State::Inactive)
    , m_dispatchScheduledEventRunner(AsyncMethodRunner<MediaRecorder>::create(this, &MediaRecorder::dispatchScheduledEvent))
{
    ASSERT(m_stream->getTracks().size());

    m_recorderHandler = adoptPtr(Platform::current()->createMediaRecorderHandler());
    ASSERT(m_recorderHandler);

    // We deviate from the spec by not returning |UnsupportedOption|, see https://github.com/w3c/mediacapture-record/issues/18
    if (!m_recorderHandler) {
        exceptionState.throwDOMException(NotSupportedError, "No MediaRecorder handler can be created.");
        return;
    }
    ContentType contentType(m_mimeType);
    if (!m_recorderHandler->initialize(this, stream->descriptor(), contentType.type(), contentType.parameter("codecs"))) {
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
    start(0 /* timeSlice */, exceptionState);
}

void MediaRecorder::start(int timeSlice, ExceptionState& exceptionState)
{
    if (m_state != State::Inactive) {
        exceptionState.throwDOMException(InvalidStateError, "The MediaRecorder's state is '" + stateToString(m_state) + "'.");
        return;
    }
    m_state = State::Recording;

    if (!m_recorderHandler->start(timeSlice)) {
        exceptionState.throwDOMException(UnknownError, "The MediaRecorder failed to start because there are no audio or video tracks available.");
        return;
    }
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
    writeData(nullptr /* data */, 0 /* length */, true /* lastInSlice */);
}

bool MediaRecorder::isTypeSupported(const String& type)
{
    RawPtr<WebMediaRecorderHandler> handler = Platform::current()->createMediaRecorderHandler();
    if (!handler)
        return false;

    // If true is returned from this method, it only indicates that the
    // MediaRecorder implementation is capable of recording Blob objects for the
    // specified MIME type. Recording may still fail if sufficient resources are
    // not available to support the concrete media encoding.
    // [1] https://w3c.github.io/mediacapture-record/MediaRecorder.html#methods
    ContentType contentType(type);
    return handler->canSupportMimeType(contentType.type(), contentType.parameter("codecs"));
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
    m_dispatchScheduledEventRunner->suspend();
}

void MediaRecorder::resume()
{
    m_dispatchScheduledEventRunner->resume();
}

void MediaRecorder::stop()
{
    if (m_stopped)
        return;

    m_stopped = true;
    m_stream.clear();
    m_recorderHandler.clear();
}

void MediaRecorder::writeData(const char* data, size_t length, bool lastInSlice)
{
    if (m_stopped && !lastInSlice) {
        m_stopped = false;
        scheduleDispatchEvent(Event::create(EventTypeNames::start));
    }
    if (m_stream && m_streamAmountOfTracks != m_stream->getTracks().size()) {
        m_streamAmountOfTracks = m_stream->getTracks().size();
        onError("Amount of tracks in MediaStream has changed.");
    }

    // TODO(mcasas): Act as |m_ignoredMutedMedia| instructs if |m_stream| track(s) is in muted() state.

    if (!m_blobData)
        m_blobData = BlobData::create();
    if (data)
        m_blobData->appendBytes(data, length);

    if (!lastInSlice)
        return;

    // Cache |m_blobData->length()| before release()ng it.
    const long long blobDataLength = m_blobData->length();
    createBlobEvent(Blob::create(BlobDataHandle::create(m_blobData.release(), blobDataLength)));
}

void MediaRecorder::onError(const WebString& message)
{
    // TODO(mcasas): Beef up the Error Event and add the |message|, see https://github.com/w3c/mediacapture-record/issues/31
    scheduleDispatchEvent(Event::create(EventTypeNames::error));
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

    writeData(nullptr /* data */, 0 /* length */, true /* lastInSlice */);
    scheduleDispatchEvent(Event::create(EventTypeNames::stop));
}

void MediaRecorder::scheduleDispatchEvent(PassRefPtrWillBeRawPtr<Event> event)
{
    m_scheduledEvents.append(event);

    m_dispatchScheduledEventRunner->runAsync();
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
    visitor->trace(m_dispatchScheduledEventRunner);
    visitor->trace(m_scheduledEvents);
    RefCountedGarbageCollectedEventTargetWithInlineData<MediaRecorder>::trace(visitor);
    ActiveDOMObject::trace(visitor);
}

} // namespace blink
