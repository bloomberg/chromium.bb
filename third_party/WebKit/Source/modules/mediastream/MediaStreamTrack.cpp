/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "modules/mediastream/MediaStreamTrack.h"

#include "bindings/v8/ExceptionMessages.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/Event.h"
#include "core/platform/mediastream/MediaStreamCenter.h"
#include "core/platform/mediastream/MediaStreamComponent.h"
#include "modules/mediastream/MediaStreamTrackSourcesCallback.h"
#include "modules/mediastream/MediaStreamTrackSourcesRequest.h"
#include "public/platform/WebSourceInfo.h"

namespace WebCore {

PassRefPtr<MediaStreamTrack> MediaStreamTrack::create(ExecutionContext* context, MediaStreamComponent* component)
{
    RefPtr<MediaStreamTrack> track = adoptRef(new MediaStreamTrack(context, component));
    track->suspendIfNeeded();
    return track.release();
}

MediaStreamTrack::MediaStreamTrack(ExecutionContext* context, MediaStreamComponent* component)
    : ActiveDOMObject(context)
    , m_stopped(false)
    , m_component(component)
{
    ScriptWrappable::init(this);
    m_component->source()->addObserver(this);
}

MediaStreamTrack::~MediaStreamTrack()
{
    m_component->source()->removeObserver(this);
}

String MediaStreamTrack::kind() const
{
    DEFINE_STATIC_LOCAL(String, audioKind, ("audio"));
    DEFINE_STATIC_LOCAL(String, videoKind, ("video"));

    switch (m_component->source()->type()) {
    case MediaStreamSource::TypeAudio:
        return audioKind;
    case MediaStreamSource::TypeVideo:
        return videoKind;
    }

    ASSERT_NOT_REACHED();
    return audioKind;
}

String MediaStreamTrack::id() const
{
    return m_component->id();
}

String MediaStreamTrack::label() const
{
    return m_component->source()->name();
}

bool MediaStreamTrack::enabled() const
{
    return m_component->enabled();
}

void MediaStreamTrack::setEnabled(bool enabled)
{
    if (m_stopped || enabled == m_component->enabled())
        return;

    m_component->setEnabled(enabled);

    if (m_component->stream()->ended())
        return;

    MediaStreamCenter::instance().didSetMediaStreamTrackEnabled(m_component->stream(), m_component.get());
}

String MediaStreamTrack::readyState() const
{
    if (m_stopped)
        return "ended";

    switch (m_component->source()->readyState()) {
    case MediaStreamSource::ReadyStateLive:
        return "live";
    case MediaStreamSource::ReadyStateMuted:
        return "muted";
    case MediaStreamSource::ReadyStateEnded:
        return "ended";
    }

    ASSERT_NOT_REACHED();
    return String();
}

void MediaStreamTrack::getSources(ExecutionContext* context, PassRefPtr<MediaStreamTrackSourcesCallback> callback, ExceptionState& es)
{
    RefPtr<MediaStreamTrackSourcesRequest> request = MediaStreamTrackSourcesRequest::create(context, callback);
    if (!MediaStreamCenter::instance().getMediaStreamTrackSources(request.release()))
        es.throwDOMException(NotSupportedError, ExceptionMessages::failedToExecute("getSources", "MediaStreamTrack", "Functionality not implemented yet"));
}

void MediaStreamTrack::stopTrack(ExceptionState& es)
{
    if (ended())
        return;

    if (!MediaStreamCenter::instance().didStopMediaStreamTrack(component()))
        es.throwDOMException(NotSupportedError, ExceptionMessages::failedToExecute("stop", "MediaStreamTrack", "Functionality not implemented yet"));
}

bool MediaStreamTrack::ended() const
{
    return m_stopped || (m_component->source()->readyState() == MediaStreamSource::ReadyStateEnded);
}

void MediaStreamTrack::sourceChangedState()
{
    if (m_stopped)
        return;

    switch (m_component->source()->readyState()) {
    case MediaStreamSource::ReadyStateLive:
        dispatchEvent(Event::create(EventTypeNames::unmute));
        break;
    case MediaStreamSource::ReadyStateMuted:
        dispatchEvent(Event::create(EventTypeNames::mute));
        break;
    case MediaStreamSource::ReadyStateEnded:
        dispatchEvent(Event::create(EventTypeNames::ended));
        didEndTrack();
        break;
    }
}

void MediaStreamTrack::didEndTrack()
{
    MediaStreamDescriptor* stream = m_component->stream();
    if (!stream)
        return;

    MediaStreamDescriptorClient* client = stream->client();
    if (!client)
        return;

    client->trackEnded();
}

MediaStreamComponent* MediaStreamTrack::component()
{
    return m_component.get();
}

void MediaStreamTrack::stop()
{
    m_stopped = true;
}

const AtomicString& MediaStreamTrack::interfaceName() const
{
    return EventTargetNames::MediaStreamTrack;
}

ExecutionContext* MediaStreamTrack::executionContext() const
{
    return ActiveDOMObject::executionContext();
}

} // namespace WebCore
