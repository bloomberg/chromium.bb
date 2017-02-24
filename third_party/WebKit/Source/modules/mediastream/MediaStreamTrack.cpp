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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/mediastream/MediaStreamTrack.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/Event.h"
#include "core/frame/Deprecation.h"
#include "modules/mediastream/MediaConstraintsImpl.h"
#include "modules/mediastream/MediaStream.h"
#include "modules/mediastream/MediaTrackSettings.h"
#include "modules/mediastream/UserMediaController.h"
#include "platform/mediastream/MediaStreamCenter.h"
#include "platform/mediastream/MediaStreamComponent.h"
#include "public/platform/WebMediaStreamTrack.h"
#include "wtf/Assertions.h"
#include <memory>

namespace blink {

namespace {
static const char kContentHintStringNone[] = "";
static const char kContentHintStringAudioSpeech[] = "speech";
static const char kContentHintStringAudioMusic[] = "music";
static const char kContentHintStringVideoMotion[] = "motion";
static const char kContentHintStringVideoDetail[] = "detail";
}  // namespace

MediaStreamTrack* MediaStreamTrack::create(ExecutionContext* context,
                                           MediaStreamComponent* component) {
  return new MediaStreamTrack(context, component);
}

MediaStreamTrack::MediaStreamTrack(ExecutionContext* context,
                                   MediaStreamComponent* component)
    : ContextLifecycleObserver(context),
      m_readyState(MediaStreamSource::ReadyStateLive),
      m_isIteratingRegisteredMediaStreams(false),
      m_stopped(false),
      m_component(component),
      // The source's constraints aren't yet initialized at creation time.
      m_constraints() {
  m_component->source()->addObserver(this);
}

MediaStreamTrack::~MediaStreamTrack() {}

String MediaStreamTrack::kind() const {
  DEFINE_STATIC_LOCAL(String, audioKind, ("audio"));
  DEFINE_STATIC_LOCAL(String, videoKind, ("video"));

  switch (m_component->source()->type()) {
    case MediaStreamSource::TypeAudio:
      return audioKind;
    case MediaStreamSource::TypeVideo:
      return videoKind;
  }

  NOTREACHED();
  return audioKind;
}

String MediaStreamTrack::id() const {
  return m_component->id();
}

String MediaStreamTrack::label() const {
  return m_component->source()->name();
}

bool MediaStreamTrack::enabled() const {
  return m_component->enabled();
}

void MediaStreamTrack::setEnabled(bool enabled) {
  if (enabled == m_component->enabled())
    return;

  m_component->setEnabled(enabled);

  if (!ended())
    MediaStreamCenter::instance().didSetMediaStreamTrackEnabled(
        m_component.get());
}

bool MediaStreamTrack::muted() const {
  return m_component->muted();
}

String MediaStreamTrack::contentHint() const {
  WebMediaStreamTrack::ContentHintType hint = m_component->contentHint();
  switch (hint) {
    case WebMediaStreamTrack::ContentHintType::None:
      return kContentHintStringNone;
    case WebMediaStreamTrack::ContentHintType::AudioSpeech:
      return kContentHintStringAudioSpeech;
    case WebMediaStreamTrack::ContentHintType::AudioMusic:
      return kContentHintStringAudioMusic;
    case WebMediaStreamTrack::ContentHintType::VideoMotion:
      return kContentHintStringVideoMotion;
    case WebMediaStreamTrack::ContentHintType::VideoDetail:
      return kContentHintStringVideoDetail;
  }

  NOTREACHED();
  return String();
}

void MediaStreamTrack::setContentHint(const String& hint) {
  WebMediaStreamTrack::ContentHintType translatedHint =
      WebMediaStreamTrack::ContentHintType::None;
  switch (m_component->source()->type()) {
    case MediaStreamSource::TypeAudio:
      if (hint == kContentHintStringNone) {
        translatedHint = WebMediaStreamTrack::ContentHintType::None;
      } else if (hint == kContentHintStringAudioSpeech) {
        translatedHint = WebMediaStreamTrack::ContentHintType::AudioSpeech;
      } else if (hint == kContentHintStringAudioMusic) {
        translatedHint = WebMediaStreamTrack::ContentHintType::AudioMusic;
      } else {
        // TODO(pbos): Log warning?
        // Invalid values for audio are to be ignored (similar to invalid enum
        // values).
        return;
      }
      break;
    case MediaStreamSource::TypeVideo:
      if (hint == kContentHintStringNone) {
        translatedHint = WebMediaStreamTrack::ContentHintType::None;
      } else if (hint == kContentHintStringVideoMotion) {
        translatedHint = WebMediaStreamTrack::ContentHintType::VideoMotion;
      } else if (hint == kContentHintStringVideoDetail) {
        translatedHint = WebMediaStreamTrack::ContentHintType::VideoDetail;
      } else {
        // TODO(pbos): Log warning?
        // Invalid values for video are to be ignored (similar to invalid enum
        // values).
        return;
      }
  }

  m_component->setContentHint(translatedHint);
}

bool MediaStreamTrack::remote() const {
  return m_component->source()->remote();
}

String MediaStreamTrack::readyState() const {
  if (ended())
    return "ended";

  switch (m_readyState) {
    case MediaStreamSource::ReadyStateLive:
      return "live";
    case MediaStreamSource::ReadyStateMuted:
      return "muted";
    case MediaStreamSource::ReadyStateEnded:
      return "ended";
  }

  NOTREACHED();
  return String();
}

void MediaStreamTrack::stopTrack(ExceptionState& exceptionState) {
  if (ended())
    return;

  m_readyState = MediaStreamSource::ReadyStateEnded;
  MediaStreamCenter::instance().didStopMediaStreamTrack(component());
  dispatchEvent(Event::create(EventTypeNames::ended));
  propagateTrackEnded();
}

MediaStreamTrack* MediaStreamTrack::clone(ScriptState* scriptState) {
  // TODO(pbos): Make sure m_readyState and m_stopped carries over on cloned
  // tracks.
  MediaStreamComponent* clonedComponent = component()->clone();
  MediaStreamTrack* clonedTrack = MediaStreamTrack::create(
      scriptState->getExecutionContext(), clonedComponent);
  MediaStreamCenter::instance().didCreateMediaStreamTrack(clonedComponent);
  return clonedTrack;
}

void MediaStreamTrack::getConstraints(MediaTrackConstraints& constraints) {
  MediaConstraintsImpl::convertConstraints(m_constraints, constraints);
}

void MediaStreamTrack::setConstraints(const WebMediaConstraints& constraints) {
  m_constraints = constraints;
}

void MediaStreamTrack::getSettings(MediaTrackSettings& settings) {
  WebMediaStreamTrack::Settings platformSettings;
  m_component->getSettings(platformSettings);
  if (platformSettings.hasFrameRate()) {
    settings.setFrameRate(platformSettings.frameRate);
  }
  if (platformSettings.hasWidth()) {
    settings.setWidth(platformSettings.width);
  }
  if (platformSettings.hasHeight()) {
    settings.setHeight(platformSettings.height);
  }
  if (RuntimeEnabledFeatures::mediaCaptureDepthEnabled() &&
      m_component->source()->type() == MediaStreamSource::TypeVideo) {
    if (platformSettings.hasVideoKind())
      settings.setVideoKind(platformSettings.videoKind);
    if (platformSettings.hasDepthNear())
      settings.setDepthNear(platformSettings.depthNear);
    if (platformSettings.hasDepthFar())
      settings.setDepthFar(platformSettings.depthFar);
    if (platformSettings.hasFocalLengthX())
      settings.setFocalLengthX(platformSettings.focalLengthX);
    if (platformSettings.hasFocalLengthY())
      settings.setFocalLengthY(platformSettings.focalLengthY);
  }
  settings.setDeviceId(platformSettings.deviceId);
  if (platformSettings.hasFacingMode()) {
    switch (platformSettings.facingMode) {
      case WebMediaStreamTrack::FacingMode::User:
        settings.setFacingMode("user");
        break;
      case WebMediaStreamTrack::FacingMode::Environment:
        settings.setFacingMode("environment");
        break;
      case WebMediaStreamTrack::FacingMode::Left:
        settings.setFacingMode("left");
        break;
      case WebMediaStreamTrack::FacingMode::Right:
        settings.setFacingMode("right");
        break;
      default:
        // None, or unknown facing mode. Ignore.
        break;
    }
  }
}

bool MediaStreamTrack::ended() const {
  return m_stopped || (m_readyState == MediaStreamSource::ReadyStateEnded);
}

void MediaStreamTrack::sourceChangedState() {
  if (ended())
    return;

  m_readyState = m_component->source()->getReadyState();
  switch (m_readyState) {
    case MediaStreamSource::ReadyStateLive:
      m_component->setMuted(false);
      dispatchEvent(Event::create(EventTypeNames::unmute));
      break;
    case MediaStreamSource::ReadyStateMuted:
      m_component->setMuted(true);
      dispatchEvent(Event::create(EventTypeNames::mute));
      break;
    case MediaStreamSource::ReadyStateEnded:
      dispatchEvent(Event::create(EventTypeNames::ended));
      propagateTrackEnded();
      break;
  }
}

void MediaStreamTrack::propagateTrackEnded() {
  CHECK(!m_isIteratingRegisteredMediaStreams);
  m_isIteratingRegisteredMediaStreams = true;
  for (HeapHashSet<Member<MediaStream>>::iterator iter =
           m_registeredMediaStreams.begin();
       iter != m_registeredMediaStreams.end(); ++iter)
    (*iter)->trackEnded();
  m_isIteratingRegisteredMediaStreams = false;
}

void MediaStreamTrack::contextDestroyed(ExecutionContext*) {
  m_stopped = true;
}

bool MediaStreamTrack::hasPendingActivity() const {
  // If 'ended' listeners exist and the object hasn't yet reached
  // that state, keep the object alive.
  //
  // An otherwise unreachable MediaStreamTrack object in an non-ended
  // state will otherwise indirectly be transitioned to the 'ended' state
  // while finalizing m_component. Which dispatches an 'ended' event,
  // referring to this object as the target. If this object is then GCed
  // at the same time, v8 objects will retain (wrapper) references to
  // this dead MediaStreamTrack object. Bad.
  //
  // Hence insisting on keeping this object alive until the 'ended'
  // state has been reached & handled.
  return !ended() && hasEventListeners(EventTypeNames::ended);
}

std::unique_ptr<AudioSourceProvider> MediaStreamTrack::createWebAudioSource() {
  return MediaStreamCenter::instance().createWebAudioSourceFromMediaStreamTrack(
      component());
}

void MediaStreamTrack::registerMediaStream(MediaStream* mediaStream) {
  CHECK(!m_isIteratingRegisteredMediaStreams);
  CHECK(!m_registeredMediaStreams.contains(mediaStream));
  m_registeredMediaStreams.insert(mediaStream);
}

void MediaStreamTrack::unregisterMediaStream(MediaStream* mediaStream) {
  CHECK(!m_isIteratingRegisteredMediaStreams);
  HeapHashSet<Member<MediaStream>>::iterator iter =
      m_registeredMediaStreams.find(mediaStream);
  CHECK(iter != m_registeredMediaStreams.end());
  m_registeredMediaStreams.erase(iter);
}

const AtomicString& MediaStreamTrack::interfaceName() const {
  return EventTargetNames::MediaStreamTrack;
}

ExecutionContext* MediaStreamTrack::getExecutionContext() const {
  return ContextLifecycleObserver::getExecutionContext();
}

DEFINE_TRACE(MediaStreamTrack) {
  visitor->trace(m_registeredMediaStreams);
  visitor->trace(m_component);
  EventTargetWithInlineData::trace(visitor);
  ContextLifecycleObserver::trace(visitor);
}

}  // namespace blink
