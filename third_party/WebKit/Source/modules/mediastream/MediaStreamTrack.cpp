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
#include "modules/mediastream/MediaStreamTrackSourcesCallback.h"
#include "modules/mediastream/MediaStreamTrackSourcesRequestImpl.h"
#include "platform/mediastream/MediaStreamCenter.h"
#include "platform/mediastream/MediaStreamComponent.h"
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
    , m_isIteratingObservers(false)
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

    MediaStreamCenter::instance().didSetMediaStreamTrackEnabled(m_component.get());
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

void MediaStreamTrack::getSources(ExecutionContext* context, PassOwnPtr<MediaStreamTrackSourcesCallback> callback, ExceptionState& exceptionState)
{
    RefPtr<MediaStreamTrackSourcesRequest> request = MediaStreamTrackSourcesRequestImpl::create(context->securityOrigin()->toString(), callback);
    if (!MediaStreamCenter::instance().getMediaStreamTrackSources(request.release()))
        exceptionState.throwDOMException(NotSupportedError, ExceptionMessages::failedToExecute("getSources", "MediaStreamTrack", "Functionality not implemented yet"));
}

void MediaStreamTrack::stopTrack(ExceptionState& exceptionState)
{
    if (ended())
        return;

    MediaStreamCenter::instance().didStopMediaStreamTrack(component());
}

PassRefPtr<MediaStreamTrack> MediaStreamTrack::clone(ExecutionContext* context)
{
    RefPtr<MediaStreamComponent> clonedComponent = MediaStreamComponent::create(component()->source());
    RefPtr<MediaStreamTrack> clonedTrack = MediaStreamTrack::create(context, clonedComponent.get());
    MediaStreamCenter::instance().didCreateMediaStreamTrack(clonedComponent.get());
    return clonedTrack.release();
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
        propagateTrackEnded();
        break;
    }
}

void MediaStreamTrack::propagateTrackEnded()
{
    RELEASE_ASSERT(!m_isIteratingObservers);
    m_isIteratingObservers = true;
    for (Vector<Observer*>::iterator iter = m_observers.begin(); iter != m_observers.end(); ++iter)
        (*iter)->trackEnded();
    m_isIteratingObservers = false;
}

MediaStreamComponent* MediaStreamTrack::component()
{
    return m_component.get();
}

void MediaStreamTrack::stop()
{
    m_stopped = true;
}

void MediaStreamTrack::addObserver(MediaStreamTrack::Observer* observer)
{
    RELEASE_ASSERT(!m_isIteratingObservers);
    m_observers.append(observer);
}

void MediaStreamTrack::removeObserver(MediaStreamTrack::Observer* observer)
{
    RELEASE_ASSERT(!m_isIteratingObservers);
    size_t pos = m_observers.find(observer);
    RELEASE_ASSERT(pos != kNotFound);
    m_observers.remove(pos);
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
