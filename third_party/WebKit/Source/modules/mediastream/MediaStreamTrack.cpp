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

#include <memory>
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/events/Event.h"
#include "core/frame/Deprecation.h"
#include "modules/imagecapture/ImageCapture.h"
#include "modules/mediastream/MediaConstraintsImpl.h"
#include "modules/mediastream/MediaStream.h"
#include "modules/mediastream/MediaTrackCapabilities.h"
#include "modules/mediastream/MediaTrackConstraints.h"
#include "modules/mediastream/MediaTrackSettings.h"
#include "modules/mediastream/UserMediaController.h"
#include "platform/mediastream/MediaStreamCenter.h"
#include "platform/mediastream/MediaStreamComponent.h"
#include "platform/wtf/Assertions.h"
#include "public/platform/WebMediaStreamTrack.h"

namespace blink {

namespace {
static const char kContentHintStringNone[] = "";
static const char kContentHintStringAudioSpeech[] = "speech";
static const char kContentHintStringAudioMusic[] = "music";
static const char kContentHintStringVideoMotion[] = "motion";
static const char kContentHintStringVideoDetail[] = "detail";
}  // namespace

MediaStreamTrack* MediaStreamTrack::Create(ExecutionContext* context,
                                           MediaStreamComponent* component) {
  return new MediaStreamTrack(context, component);
}

MediaStreamTrack::MediaStreamTrack(ExecutionContext* context,
                                   MediaStreamComponent* component)
    : ContextLifecycleObserver(context),
      ready_state_(component->Source()->GetReadyState()),
      is_iterating_registered_media_streams_(false),
      stopped_(false),
      component_(component),
      // The source's constraints aren't yet initialized at creation time.
      constraints_() {
  component_->Source()->AddObserver(this);

  // If the source is already non-live at this point, the observer won't have
  // been called. Update the muted state manually.
  component_->SetMuted(ready_state_ == MediaStreamSource::kReadyStateMuted);

  if (component_->Source() &&
      component_->Source()->GetType() == MediaStreamSource::kTypeVideo) {
    // ImageCapture::create() only throws if |this| track is not of video type.
    NonThrowableExceptionState exception_state;
    image_capture_ = ImageCapture::Create(context, this, exception_state);
  }
}

MediaStreamTrack::~MediaStreamTrack() {}

String MediaStreamTrack::kind() const {
  DEFINE_STATIC_LOCAL(String, audio_kind, ("audio"));
  DEFINE_STATIC_LOCAL(String, video_kind, ("video"));

  switch (component_->Source()->GetType()) {
    case MediaStreamSource::kTypeAudio:
      return audio_kind;
    case MediaStreamSource::kTypeVideo:
      return video_kind;
  }

  NOTREACHED();
  return audio_kind;
}

String MediaStreamTrack::id() const {
  return component_->Id();
}

String MediaStreamTrack::label() const {
  return component_->Source()->GetName();
}

bool MediaStreamTrack::enabled() const {
  return component_->Enabled();
}

void MediaStreamTrack::setEnabled(bool enabled) {
  if (enabled == component_->Enabled())
    return;

  component_->SetEnabled(enabled);

  if (!Ended())
    MediaStreamCenter::Instance().DidSetMediaStreamTrackEnabled(
        component_.Get());
}

bool MediaStreamTrack::muted() const {
  return component_->Muted();
}

String MediaStreamTrack::ContentHint() const {
  WebMediaStreamTrack::ContentHintType hint = component_->ContentHint();
  switch (hint) {
    case WebMediaStreamTrack::ContentHintType::kNone:
      return kContentHintStringNone;
    case WebMediaStreamTrack::ContentHintType::kAudioSpeech:
      return kContentHintStringAudioSpeech;
    case WebMediaStreamTrack::ContentHintType::kAudioMusic:
      return kContentHintStringAudioMusic;
    case WebMediaStreamTrack::ContentHintType::kVideoMotion:
      return kContentHintStringVideoMotion;
    case WebMediaStreamTrack::ContentHintType::kVideoDetail:
      return kContentHintStringVideoDetail;
  }

  NOTREACHED();
  return String();
}

void MediaStreamTrack::SetContentHint(const String& hint) {
  WebMediaStreamTrack::ContentHintType translated_hint =
      WebMediaStreamTrack::ContentHintType::kNone;
  switch (component_->Source()->GetType()) {
    case MediaStreamSource::kTypeAudio:
      if (hint == kContentHintStringNone) {
        translated_hint = WebMediaStreamTrack::ContentHintType::kNone;
      } else if (hint == kContentHintStringAudioSpeech) {
        translated_hint = WebMediaStreamTrack::ContentHintType::kAudioSpeech;
      } else if (hint == kContentHintStringAudioMusic) {
        translated_hint = WebMediaStreamTrack::ContentHintType::kAudioMusic;
      } else {
        // TODO(pbos): Log warning?
        // Invalid values for audio are to be ignored (similar to invalid enum
        // values).
        return;
      }
      break;
    case MediaStreamSource::kTypeVideo:
      if (hint == kContentHintStringNone) {
        translated_hint = WebMediaStreamTrack::ContentHintType::kNone;
      } else if (hint == kContentHintStringVideoMotion) {
        translated_hint = WebMediaStreamTrack::ContentHintType::kVideoMotion;
      } else if (hint == kContentHintStringVideoDetail) {
        translated_hint = WebMediaStreamTrack::ContentHintType::kVideoDetail;
      } else {
        // TODO(pbos): Log warning?
        // Invalid values for video are to be ignored (similar to invalid enum
        // values).
        return;
      }
  }

  component_->SetContentHint(translated_hint);
}

String MediaStreamTrack::readyState() const {
  if (Ended())
    return "ended";

  // Although muted is tracked as a ReadyState, only "live" and "ended" are
  // visible externally.
  switch (ready_state_) {
    case MediaStreamSource::kReadyStateLive:
    case MediaStreamSource::kReadyStateMuted:
      return "live";
    case MediaStreamSource::kReadyStateEnded:
      return "ended";
  }

  NOTREACHED();
  return String();
}

void MediaStreamTrack::stopTrack(ExceptionState& exception_state) {
  if (Ended())
    return;

  ready_state_ = MediaStreamSource::kReadyStateEnded;
  MediaStreamCenter::Instance().DidStopMediaStreamTrack(Component());
  PropagateTrackEnded();
}

MediaStreamTrack* MediaStreamTrack::clone(ScriptState* script_state) {
  // TODO(pbos): Make sure m_readyState and m_stopped carries over on cloned
  // tracks.
  MediaStreamComponent* cloned_component = Component()->Clone();
  MediaStreamTrack* cloned_track = MediaStreamTrack::Create(
      ExecutionContext::From(script_state), cloned_component);
  MediaStreamCenter::Instance().DidCloneMediaStreamTrack(Component(),
                                                         cloned_component);
  return cloned_track;
}

void MediaStreamTrack::SetConstraints(const WebMediaConstraints& constraints) {
  constraints_ = constraints;
}

void MediaStreamTrack::getCapabilities(MediaTrackCapabilities& capabilities) {
  if (image_capture_)
    capabilities = image_capture_->GetMediaTrackCapabilities();
}

void MediaStreamTrack::getConstraints(MediaTrackConstraints& constraints) {
  MediaConstraintsImpl::ConvertConstraints(constraints_, constraints);

  if (!image_capture_)
    return;
  HeapVector<MediaTrackConstraintSet> vector;
  if (constraints.hasAdvanced())
    vector = constraints.advanced();
  // TODO(mcasas): consider consolidating this code in MediaContraintsImpl.
  auto image_capture_constraints = image_capture_->GetMediaTrackConstraints();
  // TODO(mcasas): add |torch|, https://crbug.com/700607.
  if (image_capture_constraints.hasWhiteBalanceMode() ||
      image_capture_constraints.hasExposureMode() ||
      image_capture_constraints.hasFocusMode() ||
      image_capture_constraints.hasExposureCompensation() ||
      image_capture_constraints.hasColorTemperature() ||
      image_capture_constraints.hasIso() ||
      image_capture_constraints.hasBrightness() ||
      image_capture_constraints.hasContrast() ||
      image_capture_constraints.hasSaturation() ||
      image_capture_constraints.hasSharpness() ||
      image_capture_constraints.hasZoom()) {
    // Add image capture constraints, if any, as another entry to advanced().
    vector.emplace_back(image_capture_constraints);
    constraints.setAdvanced(vector);
  }
}

void MediaStreamTrack::getSettings(MediaTrackSettings& settings) {
  WebMediaStreamTrack::Settings platform_settings;
  component_->GetSettings(platform_settings);
  if (platform_settings.HasFrameRate())
    settings.setFrameRate(platform_settings.frame_rate);
  if (platform_settings.HasWidth())
    settings.setWidth(platform_settings.width);
  if (platform_settings.HasHeight())
    settings.setHeight(platform_settings.height);
  if (platform_settings.HasAspectRatio())
    settings.setAspectRatio(platform_settings.aspect_ratio);
  if (RuntimeEnabledFeatures::MediaCaptureDepthVideoKindEnabled() &&
      component_->Source()->GetType() == MediaStreamSource::kTypeVideo) {
    if (platform_settings.HasVideoKind())
      settings.setVideoKind(platform_settings.video_kind);
  }
  if (RuntimeEnabledFeatures::MediaCaptureDepthEnabled() &&
      component_->Source()->GetType() == MediaStreamSource::kTypeVideo) {
    if (platform_settings.HasDepthNear())
      settings.setDepthNear(platform_settings.depth_near);
    if (platform_settings.HasDepthFar())
      settings.setDepthFar(platform_settings.depth_far);
    if (platform_settings.HasFocalLengthX())
      settings.setFocalLengthX(platform_settings.focal_length_x);
    if (platform_settings.HasFocalLengthY())
      settings.setFocalLengthY(platform_settings.focal_length_y);
  }
  settings.setDeviceId(platform_settings.device_id);
  if (platform_settings.HasFacingMode()) {
    switch (platform_settings.facing_mode) {
      case WebMediaStreamTrack::FacingMode::kUser:
        settings.setFacingMode("user");
        break;
      case WebMediaStreamTrack::FacingMode::kEnvironment:
        settings.setFacingMode("environment");
        break;
      case WebMediaStreamTrack::FacingMode::kLeft:
        settings.setFacingMode("left");
        break;
      case WebMediaStreamTrack::FacingMode::kRight:
        settings.setFacingMode("right");
        break;
      default:
        // None, or unknown facing mode. Ignore.
        break;
    }
  }
  if (platform_settings.HasEchoCancellationValue()) {
    settings.setEchoCancellation(
        static_cast<bool>(platform_settings.echo_cancellation));
  }

  if (image_capture_)
    image_capture_->GetMediaTrackSettings(settings);
}

ScriptPromise MediaStreamTrack::applyConstraints(
    ScriptState* script_state,
    const MediaTrackConstraints& constraints) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // TODO(mcasas): Until https://crbug.com/338503 is landed, we only support
  // ImageCapture-related constraints.
  if (!image_capture_ ||
      image_capture_->HasNonImageCaptureConstraints(constraints)) {
    resolver->Reject(DOMException::Create(
        kNotSupportedError,
        "Only Image-Capture constraints supported (https://crbug.com/338503)"));
    return promise;
  }

  // |constraints| empty means "remove/clear all current constraints".
  if (!constraints.hasAdvanced() || constraints.advanced().IsEmpty())
    image_capture_->ClearMediaTrackConstraints(resolver);
  else
    image_capture_->SetMediaTrackConstraints(resolver, constraints.advanced());

  return promise;
}

bool MediaStreamTrack::Ended() const {
  return stopped_ || (ready_state_ == MediaStreamSource::kReadyStateEnded);
}

void MediaStreamTrack::SourceChangedState() {
  if (Ended())
    return;

  ready_state_ = component_->Source()->GetReadyState();
  switch (ready_state_) {
    case MediaStreamSource::kReadyStateLive:
      component_->SetMuted(false);
      DispatchEvent(Event::Create(EventTypeNames::unmute));
      break;
    case MediaStreamSource::kReadyStateMuted:
      component_->SetMuted(true);
      DispatchEvent(Event::Create(EventTypeNames::mute));
      break;
    case MediaStreamSource::kReadyStateEnded:
      DispatchEvent(Event::Create(EventTypeNames::ended));
      PropagateTrackEnded();
      break;
  }
}

void MediaStreamTrack::PropagateTrackEnded() {
  CHECK(!is_iterating_registered_media_streams_);
  is_iterating_registered_media_streams_ = true;
  for (HeapHashSet<Member<MediaStream>>::iterator iter =
           registered_media_streams_.begin();
       iter != registered_media_streams_.end(); ++iter)
    (*iter)->TrackEnded();
  is_iterating_registered_media_streams_ = false;
}

void MediaStreamTrack::ContextDestroyed(ExecutionContext*) {
  stopped_ = true;
}

bool MediaStreamTrack::HasPendingActivity() const {
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
  return !Ended() && HasEventListeners(EventTypeNames::ended);
}

std::unique_ptr<AudioSourceProvider> MediaStreamTrack::CreateWebAudioSource() {
  return MediaStreamCenter::Instance().CreateWebAudioSourceFromMediaStreamTrack(
      Component());
}

void MediaStreamTrack::RegisterMediaStream(MediaStream* media_stream) {
  CHECK(!is_iterating_registered_media_streams_);
  CHECK(!registered_media_streams_.Contains(media_stream));
  registered_media_streams_.insert(media_stream);
}

void MediaStreamTrack::UnregisterMediaStream(MediaStream* media_stream) {
  CHECK(!is_iterating_registered_media_streams_);
  HeapHashSet<Member<MediaStream>>::iterator iter =
      registered_media_streams_.find(media_stream);
  CHECK(iter != registered_media_streams_.end());
  registered_media_streams_.erase(iter);
}

const AtomicString& MediaStreamTrack::InterfaceName() const {
  return EventTargetNames::MediaStreamTrack;
}

ExecutionContext* MediaStreamTrack::GetExecutionContext() const {
  return ContextLifecycleObserver::GetExecutionContext();
}

DEFINE_TRACE(MediaStreamTrack) {
  visitor->Trace(registered_media_streams_);
  visitor->Trace(component_);
  visitor->Trace(image_capture_);
  EventTargetWithInlineData::Trace(visitor);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
