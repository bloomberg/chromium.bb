/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#include "third_party/blink/renderer/modules/webaudio/panner_node.h"

#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer_source_node.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/panner_options.h"
#include "third_party/blink/renderer/platform/audio/hrtf_panner.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/histogram.h"
#include "third_party/blink/renderer/platform/wtf/math_extras.h"

namespace blink {

static void FixNANs(double& x) {
  if (std::isnan(x) || std::isinf(x))
    x = 0.0;
}

PannerHandler::PannerHandler(AudioNode& node,
                             float sample_rate,
                             AudioParamHandler& position_x,
                             AudioParamHandler& position_y,
                             AudioParamHandler& position_z,
                             AudioParamHandler& orientation_x,
                             AudioParamHandler& orientation_y,
                             AudioParamHandler& orientation_z)
    : AudioHandler(kNodeTypePanner, node, sample_rate),
      listener_(node.context()->listener()),
      distance_model_(DistanceEffect::kModelInverse),
      is_azimuth_elevation_dirty_(true),
      is_distance_cone_gain_dirty_(true),
      cached_azimuth_(0),
      cached_elevation_(0),
      cached_distance_cone_gain_(1.0f),
      position_x_(&position_x),
      position_y_(&position_y),
      position_z_(&position_z),
      orientation_x_(&orientation_x),
      orientation_y_(&orientation_y),
      orientation_z_(&orientation_z) {
  AddInput();
  AddOutput(2);

  // Node-specific default mixing rules.
  channel_count_ = 2;
  SetInternalChannelCountMode(kClampedMax);
  SetInternalChannelInterpretation(AudioBus::kSpeakers);

  // Explicitly set the default panning model here so that the histograms
  // include the default value.
  SetPanningModel("equalpower");

  Initialize();
}

scoped_refptr<PannerHandler> PannerHandler::Create(
    AudioNode& node,
    float sample_rate,
    AudioParamHandler& position_x,
    AudioParamHandler& position_y,
    AudioParamHandler& position_z,
    AudioParamHandler& orientation_x,
    AudioParamHandler& orientation_y,
    AudioParamHandler& orientation_z) {
  return base::AdoptRef(new PannerHandler(node, sample_rate, position_x,
                                          position_y, position_z, orientation_x,
                                          orientation_y, orientation_z));
}

PannerHandler::~PannerHandler() {
  Uninitialize();
}

void PannerHandler::Process(uint32_t frames_to_process) {
  AudioBus* destination = Output(0).Bus();

  if (!IsInitialized() || !panner_.get()) {
    destination->Zero();
    return;
  }

  AudioBus* source = Input(0).Bus();
  if (!source) {
    destination->Zero();
    return;
  }

  // The audio thread can't block on this lock, so we call tryLock() instead.
  MutexTryLocker try_locker(process_lock_);
  MutexTryLocker try_listener_locker(Listener()->ListenerLock());

  if (try_locker.Locked() && try_listener_locker.Locked()) {
    if (!Context()->HasRealtimeConstraint() &&
        panning_model_ == Panner::kPanningModelHRTF) {
      // For an OfflineAudioContext, we need to make sure the HRTFDatabase
      // is loaded before proceeding.  For realtime contexts, we don't
      // have to wait.  The HRTF panner handles that case itself.
      Listener()->WaitForHRTFDatabaseLoaderThreadCompletion();
    }

    if (HasSampleAccurateValues() || Listener()->HasSampleAccurateValues()) {
      // It's tempting to skip sample-accurate processing if
      // isAzimuthElevationDirty() and isDistanceConeGain() both return false.
      // But in general we can't because something may scheduled to start in the
      // middle of the rendering quantum.  On the other hand, the audible effect
      // may be small enough that we can afford to do this optimization.
      ProcessSampleAccurateValues(destination, source, frames_to_process);
    } else {
      // Apply the panning effect.
      double azimuth;
      double elevation;

      // Update dirty state in case something has moved; this can happen if the
      // AudioParam for the position or orientation component is set directly.
      UpdateDirtyState();

      AzimuthElevation(&azimuth, &elevation);

      panner_->Pan(azimuth, elevation, source, destination, frames_to_process,
                   InternalChannelInterpretation());

      // Get the distance and cone gain.
      float total_gain = DistanceConeGain();

      // Apply gain in-place.
      destination->CopyWithGainFrom(*destination, total_gain);
    }
  } else {
    // Too bad - The tryLock() failed.  We must be in the middle of changing the
    // properties of the panner or the listener.
    destination->Zero();
  }
}

void PannerHandler::ProcessSampleAccurateValues(AudioBus* destination,
                                                const AudioBus* source,
                                                uint32_t frames_to_process) {
  CHECK_LE(frames_to_process, audio_utilities::kRenderQuantumFrames);

  // Get the sample accurate values from all of the AudioParams, including the
  // values from the AudioListener.
  float panner_x[audio_utilities::kRenderQuantumFrames];
  float panner_y[audio_utilities::kRenderQuantumFrames];
  float panner_z[audio_utilities::kRenderQuantumFrames];

  float orientation_x[audio_utilities::kRenderQuantumFrames];
  float orientation_y[audio_utilities::kRenderQuantumFrames];
  float orientation_z[audio_utilities::kRenderQuantumFrames];

  position_x_->CalculateSampleAccurateValues(panner_x, frames_to_process);
  position_y_->CalculateSampleAccurateValues(panner_y, frames_to_process);
  position_z_->CalculateSampleAccurateValues(panner_z, frames_to_process);
  orientation_x_->CalculateSampleAccurateValues(orientation_x,
                                                frames_to_process);
  orientation_y_->CalculateSampleAccurateValues(orientation_y,
                                                frames_to_process);
  orientation_z_->CalculateSampleAccurateValues(orientation_z,
                                                frames_to_process);

  // Get the automation values from the listener.
  const float* listener_x =
      Listener()->GetPositionXValues(audio_utilities::kRenderQuantumFrames);
  const float* listener_y =
      Listener()->GetPositionYValues(audio_utilities::kRenderQuantumFrames);
  const float* listener_z =
      Listener()->GetPositionZValues(audio_utilities::kRenderQuantumFrames);

  const float* forward_x =
      Listener()->GetForwardXValues(audio_utilities::kRenderQuantumFrames);
  const float* forward_y =
      Listener()->GetForwardYValues(audio_utilities::kRenderQuantumFrames);
  const float* forward_z =
      Listener()->GetForwardZValues(audio_utilities::kRenderQuantumFrames);

  const float* up_x =
      Listener()->GetUpXValues(audio_utilities::kRenderQuantumFrames);
  const float* up_y =
      Listener()->GetUpYValues(audio_utilities::kRenderQuantumFrames);
  const float* up_z =
      Listener()->GetUpZValues(audio_utilities::kRenderQuantumFrames);

  // Compute the azimuth, elevation, and total gains for each position.
  double azimuth[audio_utilities::kRenderQuantumFrames];
  double elevation[audio_utilities::kRenderQuantumFrames];
  float total_gain[audio_utilities::kRenderQuantumFrames];

  for (unsigned k = 0; k < frames_to_process; ++k) {
    FloatPoint3D panner_position(panner_x[k], panner_y[k], panner_z[k]);
    FloatPoint3D orientation(orientation_x[k], orientation_y[k],
                             orientation_z[k]);
    FloatPoint3D listener_position(listener_x[k], listener_y[k], listener_z[k]);
    FloatPoint3D listener_forward(forward_x[k], forward_y[k], forward_z[k]);
    FloatPoint3D listener_up(up_x[k], up_y[k], up_z[k]);

    CalculateAzimuthElevation(&azimuth[k], &elevation[k], panner_position,
                              listener_position, listener_forward, listener_up);

    // Get distance and cone gain
    total_gain[k] = CalculateDistanceConeGain(panner_position, orientation,
                                              listener_position);
  }

  panner_->PanWithSampleAccurateValues(azimuth, elevation, source, destination,
                                       frames_to_process,
                                       InternalChannelInterpretation());
  destination->CopyWithSampleAccurateGainValuesFrom(*destination, total_gain,
                                                    frames_to_process);
}

void PannerHandler::ProcessOnlyAudioParams(uint32_t frames_to_process) {
  float values[audio_utilities::kRenderQuantumFrames];

  DCHECK_LE(frames_to_process, audio_utilities::kRenderQuantumFrames);

  position_x_->CalculateSampleAccurateValues(values, frames_to_process);
  position_y_->CalculateSampleAccurateValues(values, frames_to_process);
  position_z_->CalculateSampleAccurateValues(values, frames_to_process);

  orientation_x_->CalculateSampleAccurateValues(values, frames_to_process);
  orientation_y_->CalculateSampleAccurateValues(values, frames_to_process);
  orientation_z_->CalculateSampleAccurateValues(values, frames_to_process);
}

void PannerHandler::Initialize() {
  if (IsInitialized())
    return;

  panner_ = Panner::Create(panning_model_, Context()->sampleRate(),
                           Listener()->HrtfDatabaseLoader());
  Listener()->AddPanner(*this);

  // Set the cached values to the current values to start things off.  The
  // panner is already marked as dirty, so this won't matter.
  last_position_ = GetPosition();
  last_orientation_ = Orientation();

  AudioHandler::Initialize();
}

void PannerHandler::Uninitialize() {
  if (!IsInitialized())
    return;

  panner_.reset();
  Listener()->RemovePanner(*this);

  AudioHandler::Uninitialize();
}

AudioListener* PannerHandler::Listener() {
  return listener_;
}

String PannerHandler::PanningModel() const {
  switch (panning_model_) {
    case Panner::kPanningModelEqualPower:
      return "equalpower";
    case Panner::kPanningModelHRTF:
      return "HRTF";
    default:
      NOTREACHED();
      return "equalpower";
  }
}

void PannerHandler::SetPanningModel(const String& model) {
  // WebIDL should guarantee that we are never called with an invalid string
  // for the model.
  if (model == "equalpower")
    SetPanningModel(Panner::kPanningModelEqualPower);
  else if (model == "HRTF")
    SetPanningModel(Panner::kPanningModelHRTF);
  else
    NOTREACHED();
}

// This method should only be called from setPanningModel(const String&)!
bool PannerHandler::SetPanningModel(unsigned model) {
  DEFINE_STATIC_LOCAL(EnumerationHistogram, panning_model_histogram,
                      ("WebAudio.PannerNode.PanningModel", 2));
  panning_model_histogram.Count(model);

  if (model == Panner::kPanningModelHRTF) {
    // Load the HRTF database asynchronously so we don't block the
    // Javascript thread while creating the HRTF database. It's ok to call
    // this multiple times; we won't be constantly loading the database over
    // and over.
    Listener()->CreateAndLoadHRTFDatabaseLoader(Context()->sampleRate());
  }

  if (!panner_.get() || model != panning_model_) {
    // This synchronizes with process().
    MutexLocker process_locker(process_lock_);
    panner_ = Panner::Create(model, Context()->sampleRate(),
                             Listener()->HrtfDatabaseLoader());
    panning_model_ = model;
  }
  return true;
}

String PannerHandler::DistanceModel() const {
  switch (const_cast<PannerHandler*>(this)->distance_effect_.Model()) {
    case DistanceEffect::kModelLinear:
      return "linear";
    case DistanceEffect::kModelInverse:
      return "inverse";
    case DistanceEffect::kModelExponential:
      return "exponential";
    default:
      NOTREACHED();
      return "inverse";
  }
}

void PannerHandler::SetDistanceModel(const String& model) {
  if (model == "linear")
    SetDistanceModel(DistanceEffect::kModelLinear);
  else if (model == "inverse")
    SetDistanceModel(DistanceEffect::kModelInverse);
  else if (model == "exponential")
    SetDistanceModel(DistanceEffect::kModelExponential);
}

bool PannerHandler::SetDistanceModel(unsigned model) {
  switch (model) {
    case DistanceEffect::kModelLinear:
    case DistanceEffect::kModelInverse:
    case DistanceEffect::kModelExponential:
      if (model != distance_model_) {
        // This synchronizes with process().
        MutexLocker process_locker(process_lock_);
        distance_effect_.SetModel(
            static_cast<DistanceEffect::ModelType>(model));
        distance_model_ = model;
      }
      break;
    default:
      NOTREACHED();
      return false;
  }

  return true;
}

void PannerHandler::SetRefDistance(double distance) {
  if (RefDistance() == distance)
    return;

  // This synchronizes with process().
  MutexLocker process_locker(process_lock_);
  distance_effect_.SetRefDistance(distance);
  MarkPannerAsDirty(PannerHandler::kDistanceConeGainDirty);
}

void PannerHandler::SetMaxDistance(double distance) {
  if (MaxDistance() == distance)
    return;

  // This synchronizes with process().
  MutexLocker process_locker(process_lock_);
  distance_effect_.SetMaxDistance(distance);
  MarkPannerAsDirty(PannerHandler::kDistanceConeGainDirty);
}

void PannerHandler::SetRolloffFactor(double factor) {
  if (RolloffFactor() == factor)
    return;

  // This synchronizes with process().
  MutexLocker process_locker(process_lock_);
  distance_effect_.SetRolloffFactor(factor);
  MarkPannerAsDirty(PannerHandler::kDistanceConeGainDirty);
}

void PannerHandler::SetConeInnerAngle(double angle) {
  if (ConeInnerAngle() == angle)
    return;

  // This synchronizes with process().
  MutexLocker process_locker(process_lock_);
  cone_effect_.SetInnerAngle(angle);
  MarkPannerAsDirty(PannerHandler::kDistanceConeGainDirty);
}

void PannerHandler::SetConeOuterAngle(double angle) {
  if (ConeOuterAngle() == angle)
    return;

  // This synchronizes with process().
  MutexLocker process_locker(process_lock_);
  cone_effect_.SetOuterAngle(angle);
  MarkPannerAsDirty(PannerHandler::kDistanceConeGainDirty);
}

void PannerHandler::SetConeOuterGain(double angle) {
  if (ConeOuterGain() == angle)
    return;

  // This synchronizes with process().
  MutexLocker process_locker(process_lock_);
  cone_effect_.SetOuterGain(angle);
  MarkPannerAsDirty(PannerHandler::kDistanceConeGainDirty);
}

void PannerHandler::SetPosition(float x,
                                float y,
                                float z,
                                ExceptionState& exceptionState) {
  // This synchronizes with process().
  MutexLocker process_locker(process_lock_);

  double now = Context()->currentTime();

  position_x_->Timeline().SetValueAtTime(x, now, exceptionState);
  position_y_->Timeline().SetValueAtTime(y, now, exceptionState);
  position_z_->Timeline().SetValueAtTime(z, now, exceptionState);

  MarkPannerAsDirty(PannerHandler::kAzimuthElevationDirty |
                    PannerHandler::kDistanceConeGainDirty);
}

void PannerHandler::SetOrientation(float x,
                                   float y,
                                   float z,
                                   ExceptionState& exceptionState) {
  // This synchronizes with process().
  MutexLocker process_locker(process_lock_);

  double now = Context()->currentTime();

  orientation_x_->Timeline().SetValueAtTime(x, now, exceptionState);
  orientation_y_->Timeline().SetValueAtTime(y, now, exceptionState);
  orientation_z_->Timeline().SetValueAtTime(z, now, exceptionState);

  MarkPannerAsDirty(PannerHandler::kDistanceConeGainDirty);
}

void PannerHandler::CalculateAzimuthElevation(
    double* out_azimuth,
    double* out_elevation,
    const FloatPoint3D& position,
    const FloatPoint3D& listener_position,
    const FloatPoint3D& listener_forward,
    const FloatPoint3D& listener_up) {
  // Calculate the source-listener vector
  FloatPoint3D source_listener = position - listener_position;

  // Quick default return if the source and listener are at the same position.
  if (source_listener.IsZero()) {
    *out_azimuth = 0;
    *out_elevation = 0;
    return;
  }

  // normalize() does nothing if the length of |sourceListener| is zero.
  source_listener.Normalize();

  // Align axes
  FloatPoint3D listener_right = listener_forward.Cross(listener_up);
  listener_right.Normalize();

  FloatPoint3D listener_forward_norm = listener_forward;
  listener_forward_norm.Normalize();

  FloatPoint3D up = listener_right.Cross(listener_forward_norm);

  float up_projection = source_listener.Dot(up);

  FloatPoint3D projected_source = source_listener - up_projection * up;

  double azimuth = rad2deg(projected_source.AngleBetween(listener_right));
  FixNANs(azimuth);  // avoid illegal values

  // Source  in front or behind the listener
  double front_back = projected_source.Dot(listener_forward_norm);
  if (front_back < 0.0)
    azimuth = 360.0 - azimuth;

  // Make azimuth relative to "front" and not "right" listener vector
  if ((azimuth >= 0.0) && (azimuth <= 270.0))
    azimuth = 90.0 - azimuth;
  else
    azimuth = 450.0 - azimuth;

  // Elevation
  double elevation = 90 - rad2deg(source_listener.AngleBetween(up));
  FixNANs(elevation);  // avoid illegal values

  if (elevation > 90.0)
    elevation = 180.0 - elevation;
  else if (elevation < -90.0)
    elevation = -180.0 - elevation;

  if (out_azimuth)
    *out_azimuth = azimuth;
  if (out_elevation)
    *out_elevation = elevation;
}

float PannerHandler::CalculateDistanceConeGain(
    const FloatPoint3D& position,
    const FloatPoint3D& orientation,
    const FloatPoint3D& listener_position) {
  double listener_distance = position.DistanceTo(listener_position);
  double distance_gain = distance_effect_.Gain(listener_distance);
  double cone_gain =
      cone_effect_.Gain(position, orientation, listener_position);

  return float(distance_gain * cone_gain);
}

void PannerHandler::AzimuthElevation(double* out_azimuth,
                                     double* out_elevation) {
  DCHECK(Context()->IsAudioThread());

  // Calculate new azimuth and elevation if the panner or the listener changed
  // position or orientation in any way.
  if (IsAzimuthElevationDirty() || Listener()->IsListenerDirty()) {
    CalculateAzimuthElevation(&cached_azimuth_, &cached_elevation_,
                              GetPosition(), Listener()->GetPosition(),
                              Listener()->Orientation(),
                              Listener()->UpVector());
    is_azimuth_elevation_dirty_ = false;
  }

  *out_azimuth = cached_azimuth_;
  *out_elevation = cached_elevation_;
}

float PannerHandler::DistanceConeGain() {
  DCHECK(Context()->IsAudioThread());

  // Calculate new distance and cone gain if the panner or the listener
  // changed position or orientation in any way.
  if (IsDistanceConeGainDirty() || Listener()->IsListenerDirty()) {
    cached_distance_cone_gain_ = CalculateDistanceConeGain(
        GetPosition(), Orientation(), Listener()->GetPosition());
    is_distance_cone_gain_dirty_ = false;
  }

  return cached_distance_cone_gain_;
}

void PannerHandler::MarkPannerAsDirty(unsigned dirty) {
  if (dirty & PannerHandler::kAzimuthElevationDirty)
    is_azimuth_elevation_dirty_ = true;

  if (dirty & PannerHandler::kDistanceConeGainDirty)
    is_distance_cone_gain_dirty_ = true;
}

void PannerHandler::SetChannelCount(unsigned channel_count,
                                    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(Context());

  // A PannerNode only supports 1 or 2 channels
  if (channel_count > 0 && channel_count <= 2) {
    if (channel_count_ != channel_count) {
      channel_count_ = channel_count;
      if (InternalChannelCountMode() != kMax)
        UpdateChannelsForInputs();
    }
  } else {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange<unsigned long>(
            "channelCount", channel_count, 1,
            ExceptionMessages::kInclusiveBound, 2,
            ExceptionMessages::kInclusiveBound));
  }
}

void PannerHandler::SetChannelCountMode(const String& mode,
                                        ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  BaseAudioContext::GraphAutoLocker locker(Context());

  ChannelCountMode old_mode = InternalChannelCountMode();

  if (mode == "clamped-max") {
    new_channel_count_mode_ = kClampedMax;
  } else if (mode == "explicit") {
    new_channel_count_mode_ = kExplicit;
  } else if (mode == "max") {
    // This is not supported for a PannerNode, which can only handle 1 or 2
    // channels.
    exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                      "Panner: 'max' is not allowed");
    new_channel_count_mode_ = old_mode;
  } else {
    // Do nothing for other invalid values.
    new_channel_count_mode_ = old_mode;
  }

  if (new_channel_count_mode_ != old_mode)
    Context()->GetDeferredTaskHandler().AddChangedChannelCountMode(this);
}

bool PannerHandler::HasSampleAccurateValues() const {
  return position_x_->HasSampleAccurateValues() ||
         position_y_->HasSampleAccurateValues() ||
         position_z_->HasSampleAccurateValues() ||
         orientation_x_->HasSampleAccurateValues() ||
         orientation_y_->HasSampleAccurateValues() ||
         orientation_z_->HasSampleAccurateValues();
}

void PannerHandler::UpdateDirtyState() {
  DCHECK(Context()->IsAudioThread());

  FloatPoint3D current_position = GetPosition();
  FloatPoint3D current_orientation = Orientation();

  bool has_moved = current_position != last_position_ ||
                   current_orientation != last_orientation_;

  if (has_moved) {
    last_position_ = current_position;
    last_orientation_ = current_orientation;

    MarkPannerAsDirty(PannerHandler::kAzimuthElevationDirty |
                      PannerHandler::kDistanceConeGainDirty);
  }
}

bool PannerHandler::RequiresTailProcessing() const {
  // If there's no internal panner method set up yet, assume we require tail
  // processing in case the HRTF panner is set later, which does require tail
  // processing.
  return panner_ ? panner_->RequiresTailProcessing() : true;
}

// ----------------------------------------------------------------

PannerNode::PannerNode(BaseAudioContext& context)
    : AudioNode(context),
      position_x_(
          AudioParam::Create(context,
                             kParamTypePannerPositionX,
                             0.0,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      position_y_(
          AudioParam::Create(context,
                             kParamTypePannerPositionY,
                             0.0,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      position_z_(
          AudioParam::Create(context,
                             kParamTypePannerPositionZ,
                             0.0,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      orientation_x_(
          AudioParam::Create(context,
                             kParamTypePannerOrientationX,
                             1.0,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      orientation_y_(
          AudioParam::Create(context,
                             kParamTypePannerOrientationY,
                             0.0,
                             AudioParamHandler::AutomationRate::kAudio,
                             AudioParamHandler::AutomationRateMode::kVariable)),
      orientation_z_(AudioParam::Create(
          context,
          kParamTypePannerOrientationZ,
          0.0,
          AudioParamHandler::AutomationRate::kAudio,
          AudioParamHandler::AutomationRateMode::kVariable)) {
  SetHandler(PannerHandler::Create(
      *this, context.sampleRate(), position_x_->Handler(),
      position_y_->Handler(), position_z_->Handler(), orientation_x_->Handler(),
      orientation_y_->Handler(), orientation_z_->Handler()));
}

PannerNode* PannerNode::Create(BaseAudioContext& context,
                               ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (context.IsContextClosed()) {
    context.ThrowExceptionForClosedState(exception_state);
    return nullptr;
  }

  return MakeGarbageCollected<PannerNode>(context);
}

PannerNode* PannerNode::Create(BaseAudioContext* context,
                               const PannerOptions* options,
                               ExceptionState& exception_state) {
  PannerNode* node = Create(*context, exception_state);

  if (!node)
    return nullptr;

  node->HandleChannelOptions(options, exception_state);

  node->setPanningModel(options->panningModel());
  node->setDistanceModel(options->distanceModel());

  node->positionX()->setValue(options->positionX());
  node->positionY()->setValue(options->positionY());
  node->positionZ()->setValue(options->positionZ());

  node->orientationX()->setValue(options->orientationX());
  node->orientationY()->setValue(options->orientationY());
  node->orientationZ()->setValue(options->orientationZ());

  node->setRefDistance(options->refDistance(), exception_state);
  node->setMaxDistance(options->maxDistance(), exception_state);
  node->setRolloffFactor(options->rolloffFactor(), exception_state);
  node->setConeInnerAngle(options->coneInnerAngle());
  node->setConeOuterAngle(options->coneOuterAngle());
  node->setConeOuterGain(options->coneOuterGain(), exception_state);

  return node;
}

PannerHandler& PannerNode::GetPannerHandler() const {
  return static_cast<PannerHandler&>(Handler());
}

String PannerNode::panningModel() const {
  return GetPannerHandler().PanningModel();
}

void PannerNode::setPanningModel(const String& model) {
  GetPannerHandler().SetPanningModel(model);
}

void PannerNode::setPosition(float x,
                             float y,
                             float z,
                             ExceptionState& exceptionState) {
  GetPannerHandler().SetPosition(x, y, z, exceptionState);
}

void PannerNode::setOrientation(float x,
                                float y,
                                float z,
                                ExceptionState& exceptionState) {
  GetPannerHandler().SetOrientation(x, y, z, exceptionState);
}

String PannerNode::distanceModel() const {
  return GetPannerHandler().DistanceModel();
}

void PannerNode::setDistanceModel(const String& model) {
  GetPannerHandler().SetDistanceModel(model);
}

double PannerNode::refDistance() const {
  return GetPannerHandler().RefDistance();
}

void PannerNode::setRefDistance(double distance,
                                ExceptionState& exception_state) {
  if (distance < 0) {
    exception_state.ThrowRangeError(
        ExceptionMessages::IndexExceedsMinimumBound<double>("refDistance",
                                                            distance, 0));
    return;
  }

  GetPannerHandler().SetRefDistance(distance);
}

double PannerNode::maxDistance() const {
  return GetPannerHandler().MaxDistance();
}

void PannerNode::setMaxDistance(double distance,
                                ExceptionState& exception_state) {
  if (distance <= 0) {
    exception_state.ThrowRangeError(
        ExceptionMessages::IndexExceedsMinimumBound<double>("maxDistance",
                                                            distance, 0));
    return;
  }

  GetPannerHandler().SetMaxDistance(distance);
}

double PannerNode::rolloffFactor() const {
  return GetPannerHandler().RolloffFactor();
}

void PannerNode::setRolloffFactor(double factor,
                                  ExceptionState& exception_state) {
  if (factor < 0) {
    exception_state.ThrowRangeError(
        ExceptionMessages::IndexExceedsMinimumBound<double>("rolloffFactor",
                                                            factor, 0));
    return;
  }

  GetPannerHandler().SetRolloffFactor(factor);
}

double PannerNode::coneInnerAngle() const {
  return GetPannerHandler().ConeInnerAngle();
}

void PannerNode::setConeInnerAngle(double angle) {
  GetPannerHandler().SetConeInnerAngle(angle);
}

double PannerNode::coneOuterAngle() const {
  return GetPannerHandler().ConeOuterAngle();
}

void PannerNode::setConeOuterAngle(double angle) {
  GetPannerHandler().SetConeOuterAngle(angle);
}

double PannerNode::coneOuterGain() const {
  return GetPannerHandler().ConeOuterGain();
}

void PannerNode::setConeOuterGain(double gain,
                                  ExceptionState& exception_state) {
  if (gain < 0 || gain > 1) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kInvalidStateError,
        ExceptionMessages::IndexOutsideRange<double>(
            "coneOuterGain", gain, 0, ExceptionMessages::kInclusiveBound, 1,
            ExceptionMessages::kInclusiveBound));
    return;
  }

  GetPannerHandler().SetConeOuterGain(gain);
}

void PannerNode::Trace(blink::Visitor* visitor) {
  visitor->Trace(position_x_);
  visitor->Trace(position_y_);
  visitor->Trace(position_z_);

  visitor->Trace(orientation_x_);
  visitor->Trace(orientation_y_);
  visitor->Trace(orientation_z_);

  AudioNode::Trace(visitor);
}

}  // namespace blink
