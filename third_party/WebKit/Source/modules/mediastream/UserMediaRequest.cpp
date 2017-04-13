/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#include "modules/mediastream/UserMediaRequest.h"

#include <type_traits>

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/SpaceSplitString.h"
#include "core/frame/Deprecation.h"
#include "core/frame/HostsUsingFeatures.h"
#include "modules/mediastream/MediaConstraintsImpl.h"
#include "modules/mediastream/MediaStream.h"
#include "modules/mediastream/MediaStreamConstraints.h"
#include "modules/mediastream/MediaTrackConstraints.h"
#include "modules/mediastream/UserMediaController.h"
#include "platform/mediastream/MediaStreamCenter.h"
#include "platform/mediastream/MediaStreamDescriptor.h"

namespace blink {

namespace {

template <typename NumericConstraint>
bool SetUsesNumericConstraint(
    const WebMediaTrackConstraintSet& set,
    NumericConstraint WebMediaTrackConstraintSet::*field) {
  return (set.*field).HasExact() || (set.*field).HasIdeal() ||
         (set.*field).HasMin() || (set.*field).HasMax();
}

template <typename DiscreteConstraint>
bool SetUsesDiscreteConstraint(
    const WebMediaTrackConstraintSet& set,
    DiscreteConstraint WebMediaTrackConstraintSet::*field) {
  return (set.*field).HasExact() || (set.*field).HasIdeal();
}

template <typename NumericConstraint>
bool RequestUsesNumericConstraint(
    const WebMediaConstraints& constraints,
    NumericConstraint WebMediaTrackConstraintSet::*field) {
  if (SetUsesNumericConstraint(constraints.Basic(), field))
    return true;
  for (const auto& advanced_set : constraints.Advanced()) {
    if (SetUsesNumericConstraint(advanced_set, field))
      return true;
  }
  return false;
}

template <typename DiscreteConstraint>
bool RequestUsesDiscreteConstraint(
    const WebMediaConstraints& constraints,
    DiscreteConstraint WebMediaTrackConstraintSet::*field) {
  static_assert(
      std::is_same<decltype(field),
                   StringConstraint WebMediaTrackConstraintSet::*>::value ||
          std::is_same<decltype(field),
                       BooleanConstraint WebMediaTrackConstraintSet::*>::value,
      "Must use StringConstraint or BooleanConstraint");
  if (SetUsesDiscreteConstraint(constraints.Basic(), field))
    return true;
  for (const auto& advanced_set : constraints.Advanced()) {
    if (SetUsesDiscreteConstraint(advanced_set, field))
      return true;
  }
  return false;
}

class FeatureCounter {
  WTF_MAKE_NONCOPYABLE(FeatureCounter);

 public:
  FeatureCounter(ExecutionContext* context)
      : context_(context), is_unconstrained_(true) {}
  void Count(UseCounter::Feature feature) {
    UseCounter::Count(context_, feature);
    is_unconstrained_ = false;
  }
  bool IsUnconstrained() { return is_unconstrained_; }

 private:
  Persistent<ExecutionContext> context_;
  bool is_unconstrained_;
};

void CountAudioConstraintUses(ExecutionContext* context,
                              const WebMediaConstraints& constraints) {
  FeatureCounter counter(context);
  if (RequestUsesNumericConstraint(constraints,
                                   &WebMediaTrackConstraintSet::sample_rate)) {
    counter.Count(UseCounter::kMediaStreamConstraintsSampleRate);
  }
  if (RequestUsesNumericConstraint(constraints,
                                   &WebMediaTrackConstraintSet::sample_size)) {
    counter.Count(UseCounter::kMediaStreamConstraintsSampleSize);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &WebMediaTrackConstraintSet::echo_cancellation)) {
    counter.Count(UseCounter::kMediaStreamConstraintsEchoCancellation);
  }
  if (RequestUsesNumericConstraint(constraints,
                                   &WebMediaTrackConstraintSet::latency)) {
    counter.Count(UseCounter::kMediaStreamConstraintsLatency);
  }
  if (RequestUsesNumericConstraint(
          constraints, &WebMediaTrackConstraintSet::channel_count)) {
    counter.Count(UseCounter::kMediaStreamConstraintsChannelCount);
  }
  if (RequestUsesDiscreteConstraint(constraints,
                                    &WebMediaTrackConstraintSet::device_id)) {
    counter.Count(UseCounter::kMediaStreamConstraintsDeviceIdAudio);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &WebMediaTrackConstraintSet::disable_local_echo)) {
    counter.Count(UseCounter::kMediaStreamConstraintsDisableLocalEcho);
  }
  if (RequestUsesDiscreteConstraint(constraints,
                                    &WebMediaTrackConstraintSet::group_id)) {
    counter.Count(UseCounter::kMediaStreamConstraintsGroupIdAudio);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &WebMediaTrackConstraintSet::media_stream_source)) {
    counter.Count(UseCounter::kMediaStreamConstraintsMediaStreamSourceAudio);
  }
  if (RequestUsesDiscreteConstraint(
          constraints,
          &WebMediaTrackConstraintSet::render_to_associated_sink)) {
    counter.Count(UseCounter::kMediaStreamConstraintsRenderToAssociatedSink);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &WebMediaTrackConstraintSet::hotword_enabled)) {
    counter.Count(UseCounter::kMediaStreamConstraintsHotwordEnabled);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &WebMediaTrackConstraintSet::goog_echo_cancellation)) {
    counter.Count(UseCounter::kMediaStreamConstraintsGoogEchoCancellation);
  }
  if (RequestUsesDiscreteConstraint(
          constraints,
          &WebMediaTrackConstraintSet::goog_experimental_echo_cancellation)) {
    counter.Count(
        UseCounter::kMediaStreamConstraintsGoogExperimentalEchoCancellation);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &WebMediaTrackConstraintSet::goog_auto_gain_control)) {
    counter.Count(UseCounter::kMediaStreamConstraintsGoogAutoGainControl);
  }
  if (RequestUsesDiscreteConstraint(
          constraints,
          &WebMediaTrackConstraintSet::goog_experimental_auto_gain_control)) {
    counter.Count(
        UseCounter::kMediaStreamConstraintsGoogExperimentalAutoGainControl);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &WebMediaTrackConstraintSet::goog_noise_suppression)) {
    counter.Count(UseCounter::kMediaStreamConstraintsGoogNoiseSuppression);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &WebMediaTrackConstraintSet::goog_highpass_filter)) {
    counter.Count(UseCounter::kMediaStreamConstraintsGoogHighpassFilter);
  }
  if (RequestUsesDiscreteConstraint(
          constraints,
          &WebMediaTrackConstraintSet::goog_typing_noise_detection)) {
    counter.Count(UseCounter::kMediaStreamConstraintsGoogTypingNoiseDetection);
  }
  if (RequestUsesDiscreteConstraint(
          constraints,
          &WebMediaTrackConstraintSet::goog_experimental_noise_suppression)) {
    counter.Count(
        UseCounter::kMediaStreamConstraintsGoogExperimentalNoiseSuppression);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &WebMediaTrackConstraintSet::goog_beamforming)) {
    counter.Count(UseCounter::kMediaStreamConstraintsGoogBeamforming);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &WebMediaTrackConstraintSet::goog_array_geometry)) {
    counter.Count(UseCounter::kMediaStreamConstraintsGoogArrayGeometry);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &WebMediaTrackConstraintSet::goog_audio_mirroring)) {
    counter.Count(UseCounter::kMediaStreamConstraintsGoogAudioMirroring);
  }
  if (RequestUsesDiscreteConstraint(
          constraints,
          &WebMediaTrackConstraintSet::goog_da_echo_cancellation)) {
    counter.Count(UseCounter::kMediaStreamConstraintsGoogDAEchoCancellation);
  }

  UseCounter::Count(context, UseCounter::kMediaStreamConstraintsAudio);
  if (counter.IsUnconstrained()) {
    UseCounter::Count(context,
                      UseCounter::kMediaStreamConstraintsAudioUnconstrained);
  }
}

void CountVideoConstraintUses(ExecutionContext* context,
                              const WebMediaConstraints& constraints) {
  FeatureCounter counter(context);
  if (RequestUsesNumericConstraint(constraints,
                                   &WebMediaTrackConstraintSet::width)) {
    counter.Count(UseCounter::kMediaStreamConstraintsWidth);
  }
  if (RequestUsesNumericConstraint(constraints,
                                   &WebMediaTrackConstraintSet::height)) {
    counter.Count(UseCounter::kMediaStreamConstraintsHeight);
  }
  if (RequestUsesNumericConstraint(constraints,
                                   &WebMediaTrackConstraintSet::aspect_ratio)) {
    counter.Count(UseCounter::kMediaStreamConstraintsAspectRatio);
  }
  if (RequestUsesNumericConstraint(constraints,
                                   &WebMediaTrackConstraintSet::frame_rate)) {
    counter.Count(UseCounter::kMediaStreamConstraintsFrameRate);
  }
  if (RequestUsesDiscreteConstraint(constraints,
                                    &WebMediaTrackConstraintSet::facing_mode)) {
    counter.Count(UseCounter::kMediaStreamConstraintsFacingMode);
  }
  if (RequestUsesDiscreteConstraint(constraints,
                                    &WebMediaTrackConstraintSet::device_id)) {
    counter.Count(UseCounter::kMediaStreamConstraintsDeviceIdVideo);
  }
  if (RequestUsesDiscreteConstraint(constraints,
                                    &WebMediaTrackConstraintSet::group_id)) {
    counter.Count(UseCounter::kMediaStreamConstraintsGroupIdVideo);
  }
  if (RequestUsesDiscreteConstraint(constraints,
                                    &WebMediaTrackConstraintSet::video_kind)) {
    counter.Count(UseCounter::kMediaStreamConstraintsVideoKind);
  }
  if (RequestUsesNumericConstraint(constraints,
                                   &WebMediaTrackConstraintSet::depth_near)) {
    counter.Count(UseCounter::kMediaStreamConstraintsDepthNear);
  }
  if (RequestUsesNumericConstraint(constraints,
                                   &WebMediaTrackConstraintSet::depth_far)) {
    counter.Count(UseCounter::kMediaStreamConstraintsDepthFar);
  }
  if (RequestUsesNumericConstraint(
          constraints, &WebMediaTrackConstraintSet::focal_length_x)) {
    counter.Count(UseCounter::kMediaStreamConstraintsFocalLengthX);
  }
  if (RequestUsesNumericConstraint(
          constraints, &WebMediaTrackConstraintSet::focal_length_y)) {
    counter.Count(UseCounter::kMediaStreamConstraintsFocalLengthY);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &WebMediaTrackConstraintSet::media_stream_source)) {
    counter.Count(UseCounter::kMediaStreamConstraintsMediaStreamSourceVideo);
  }
  if (RequestUsesDiscreteConstraint(
          constraints, &WebMediaTrackConstraintSet::goog_noise_reduction)) {
    counter.Count(UseCounter::kMediaStreamConstraintsGoogNoiseReduction);
  }
  if (RequestUsesNumericConstraint(
          constraints,
          &WebMediaTrackConstraintSet::goog_power_line_frequency)) {
    counter.Count(UseCounter::kMediaStreamConstraintsGoogPowerLineFrequency);
  }

  UseCounter::Count(context, UseCounter::kMediaStreamConstraintsVideo);
  if (counter.IsUnconstrained()) {
    UseCounter::Count(context,
                      UseCounter::kMediaStreamConstraintsVideoUnconstrained);
  }
}

WebMediaConstraints ParseOptions(ExecutionContext* context,
                                 const BooleanOrMediaTrackConstraints& options,
                                 MediaErrorState& error_state) {
  WebMediaConstraints constraints;

  Dictionary constraints_dictionary;
  if (options.isNull()) {
    // Do nothing.
  } else if (options.isMediaTrackConstraints()) {
    constraints = MediaConstraintsImpl::Create(
        context, options.getAsMediaTrackConstraints(), error_state);
  } else {
    DCHECK(options.isBoolean());
    if (options.getAsBoolean()) {
      constraints = MediaConstraintsImpl::Create();
    }
  }

  return constraints;
}

}  // namespace

UserMediaRequest* UserMediaRequest::Create(
    ExecutionContext* context,
    UserMediaController* controller,
    const MediaStreamConstraints& options,
    NavigatorUserMediaSuccessCallback* success_callback,
    NavigatorUserMediaErrorCallback* error_callback,
    MediaErrorState& error_state) {
  WebMediaConstraints audio =
      ParseOptions(context, options.audio(), error_state);
  if (error_state.HadException())
    return nullptr;

  WebMediaConstraints video =
      ParseOptions(context, options.video(), error_state);
  if (error_state.HadException())
    return nullptr;

  if (audio.IsNull() && video.IsNull()) {
    error_state.ThrowTypeError(
        "At least one of audio and video must be requested");
    return nullptr;
  }

  if (!audio.IsNull())
    CountAudioConstraintUses(context, audio);
  if (!video.IsNull())
    CountVideoConstraintUses(context, video);

  return new UserMediaRequest(context, controller, audio, video,
                              success_callback, error_callback);
}

UserMediaRequest* UserMediaRequest::CreateForTesting(
    const WebMediaConstraints& audio,
    const WebMediaConstraints& video) {
  return new UserMediaRequest(nullptr, nullptr, audio, video, nullptr, nullptr);
}

UserMediaRequest::UserMediaRequest(
    ExecutionContext* context,
    UserMediaController* controller,
    WebMediaConstraints audio,
    WebMediaConstraints video,
    NavigatorUserMediaSuccessCallback* success_callback,
    NavigatorUserMediaErrorCallback* error_callback)
    : ContextLifecycleObserver(context),
      audio_(audio),
      video_(video),
      controller_(controller),
      success_callback_(success_callback),
      error_callback_(error_callback) {}

UserMediaRequest::~UserMediaRequest() {}

bool UserMediaRequest::Audio() const {
  return !audio_.IsNull();
}

bool UserMediaRequest::Video() const {
  return !video_.IsNull();
}

WebMediaConstraints UserMediaRequest::AudioConstraints() const {
  return audio_;
}

WebMediaConstraints UserMediaRequest::VideoConstraints() const {
  return video_;
}

bool UserMediaRequest::IsSecureContextUse(String& error_message) {
  Document* document = OwnerDocument();

  if (document->IsSecureContext(error_message)) {
    UseCounter::Count(document->GetFrame(),
                      UseCounter::kGetUserMediaSecureOrigin);
    UseCounter::CountCrossOriginIframe(
        *document, UseCounter::kGetUserMediaSecureOriginIframe);
    HostsUsingFeatures::CountAnyWorld(
        *document, HostsUsingFeatures::Feature::kGetUserMediaSecureHost);
    return true;
  }

  // While getUserMedia is blocked on insecure origins, we still want to
  // count attempts to use it.
  Deprecation::CountDeprecation(document->GetFrame(),
                                UseCounter::kGetUserMediaInsecureOrigin);
  Deprecation::CountDeprecationCrossOriginIframe(
      *document, UseCounter::kGetUserMediaInsecureOriginIframe);
  HostsUsingFeatures::CountAnyWorld(
      *document, HostsUsingFeatures::Feature::kGetUserMediaInsecureHost);
  return false;
}

Document* UserMediaRequest::OwnerDocument() {
  if (ExecutionContext* context = GetExecutionContext()) {
    return ToDocument(context);
  }

  return 0;
}

void UserMediaRequest::Start() {
  if (controller_)
    controller_->RequestUserMedia(this);
}

void UserMediaRequest::Succeed(MediaStreamDescriptor* stream_descriptor) {
  if (!GetExecutionContext())
    return;

  MediaStream* stream =
      MediaStream::Create(GetExecutionContext(), stream_descriptor);

  MediaStreamTrackVector audio_tracks = stream->getAudioTracks();
  for (MediaStreamTrackVector::iterator iter = audio_tracks.begin();
       iter != audio_tracks.end(); ++iter) {
    (*iter)->Component()->Source()->SetConstraints(audio_);
    (*iter)->SetConstraints(audio_);
  }

  MediaStreamTrackVector video_tracks = stream->getVideoTracks();
  for (MediaStreamTrackVector::iterator iter = video_tracks.begin();
       iter != video_tracks.end(); ++iter) {
    (*iter)->Component()->Source()->SetConstraints(video_);
    (*iter)->SetConstraints(video_);
  }

  success_callback_->handleEvent(stream);
}

void UserMediaRequest::FailPermissionDenied(const String& message) {
  if (!GetExecutionContext())
    return;
  error_callback_->handleEvent(NavigatorUserMediaError::Create(
      NavigatorUserMediaError::kNamePermissionDenied, message, String()));
}

void UserMediaRequest::FailConstraint(const String& constraint_name,
                                      const String& message) {
  DCHECK(!constraint_name.IsEmpty());
  if (!GetExecutionContext())
    return;
  error_callback_->handleEvent(NavigatorUserMediaError::Create(
      NavigatorUserMediaError::kNameConstraintNotSatisfied, message,
      constraint_name));
}

void UserMediaRequest::FailUASpecific(const String& name,
                                      const String& message,
                                      const String& constraint_name) {
  DCHECK(!name.IsEmpty());
  if (!GetExecutionContext())
    return;
  error_callback_->handleEvent(
      NavigatorUserMediaError::Create(name, message, constraint_name));
}

void UserMediaRequest::ContextDestroyed(ExecutionContext*) {
  if (controller_) {
    controller_->CancelUserMediaRequest(this);
    controller_ = nullptr;
  }
}

DEFINE_TRACE(UserMediaRequest) {
  visitor->Trace(controller_);
  visitor->Trace(success_callback_);
  visitor->Trace(error_callback_);
  ContextLifecycleObserver::Trace(visitor);
}

}  // namespace blink
