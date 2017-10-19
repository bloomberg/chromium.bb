/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webaudio/AudioListener.h"
#include "modules/webaudio/PannerNode.h"
#include "platform/audio/AudioBus.h"
#include "platform/audio/AudioUtilities.h"
#include "platform/audio/HRTFDatabaseLoader.h"

namespace blink {

AudioListener::AudioListener(BaseAudioContext& context)
    : position_x_(
          AudioParam::Create(context, kParamTypeAudioListenerPositionX, 0.0)),
      position_y_(
          AudioParam::Create(context, kParamTypeAudioListenerPositionY, 0.0)),
      position_z_(
          AudioParam::Create(context, kParamTypeAudioListenerPositionZ, 0.0)),
      forward_x_(
          AudioParam::Create(context, kParamTypeAudioListenerForwardX, 0.0)),
      forward_y_(
          AudioParam::Create(context, kParamTypeAudioListenerForwardY, 0.0)),
      forward_z_(
          AudioParam::Create(context, kParamTypeAudioListenerForwardZ, -1.0)),
      up_x_(AudioParam::Create(context, kParamTypeAudioListenerUpX, 0.0)),
      up_y_(AudioParam::Create(context, kParamTypeAudioListenerUpY, 1.0)),
      up_z_(AudioParam::Create(context, kParamTypeAudioListenerUpZ, 0.0)),
      last_update_time_(-1),
      is_listener_dirty_(false),
      position_x_values_(AudioUtilities::kRenderQuantumFrames),
      position_y_values_(AudioUtilities::kRenderQuantumFrames),
      position_z_values_(AudioUtilities::kRenderQuantumFrames),
      forward_x_values_(AudioUtilities::kRenderQuantumFrames),
      forward_y_values_(AudioUtilities::kRenderQuantumFrames),
      forward_z_values_(AudioUtilities::kRenderQuantumFrames),
      up_x_values_(AudioUtilities::kRenderQuantumFrames),
      up_y_values_(AudioUtilities::kRenderQuantumFrames),
      up_z_values_(AudioUtilities::kRenderQuantumFrames) {
  // Initialize the cached values with the current values.  Thus, we don't need
  // to notify any panners because we haved moved.
  last_position_ = GetPosition();
  last_forward_ = Orientation();
  last_up_ = UpVector();
}

AudioListener::~AudioListener() {}

void AudioListener::Trace(blink::Visitor* visitor) {
  visitor->Trace(position_x_);
  visitor->Trace(position_y_);
  visitor->Trace(position_z_);

  visitor->Trace(forward_x_);
  visitor->Trace(forward_y_);
  visitor->Trace(forward_z_);

  visitor->Trace(up_x_);
  visitor->Trace(up_y_);
  visitor->Trace(up_z_);
}

void AudioListener::AddPanner(PannerHandler& panner) {
  DCHECK(IsMainThread());
  panners_.insert(&panner);
}

void AudioListener::RemovePanner(PannerHandler& panner) {
  DCHECK(IsMainThread());
  DCHECK(panners_.Contains(&panner));
  panners_.erase(&panner);
}

bool AudioListener::HasSampleAccurateValues() const {
  return positionX()->Handler().HasSampleAccurateValues() ||
         positionY()->Handler().HasSampleAccurateValues() ||
         positionZ()->Handler().HasSampleAccurateValues() ||
         forwardX()->Handler().HasSampleAccurateValues() ||
         forwardY()->Handler().HasSampleAccurateValues() ||
         forwardZ()->Handler().HasSampleAccurateValues() ||
         upX()->Handler().HasSampleAccurateValues() ||
         upY()->Handler().HasSampleAccurateValues() ||
         upZ()->Handler().HasSampleAccurateValues();
}

void AudioListener::UpdateValuesIfNeeded(size_t frames_to_process) {
  double current_time =
      positionX()->Handler().DestinationHandler().CurrentTime();
  if (last_update_time_ != current_time) {
    // Time has changed. Update all of the automation values now.
    last_update_time_ = current_time;

    bool sizes_are_good = frames_to_process <= position_x_values_.size() &&
                          frames_to_process <= position_y_values_.size() &&
                          frames_to_process <= position_z_values_.size() &&
                          frames_to_process <= forward_x_values_.size() &&
                          frames_to_process <= forward_y_values_.size() &&
                          frames_to_process <= forward_z_values_.size() &&
                          frames_to_process <= up_x_values_.size() &&
                          frames_to_process <= up_y_values_.size() &&
                          frames_to_process <= up_z_values_.size();

    DCHECK(sizes_are_good);
    if (!sizes_are_good)
      return;

    positionX()->Handler().CalculateSampleAccurateValues(
        position_x_values_.Data(), frames_to_process);
    positionY()->Handler().CalculateSampleAccurateValues(
        position_y_values_.Data(), frames_to_process);
    positionZ()->Handler().CalculateSampleAccurateValues(
        position_z_values_.Data(), frames_to_process);

    forwardX()->Handler().CalculateSampleAccurateValues(
        forward_x_values_.Data(), frames_to_process);
    forwardY()->Handler().CalculateSampleAccurateValues(
        forward_y_values_.Data(), frames_to_process);
    forwardZ()->Handler().CalculateSampleAccurateValues(
        forward_z_values_.Data(), frames_to_process);

    upX()->Handler().CalculateSampleAccurateValues(up_x_values_.Data(),
                                                   frames_to_process);
    upY()->Handler().CalculateSampleAccurateValues(up_y_values_.Data(),
                                                   frames_to_process);
    upZ()->Handler().CalculateSampleAccurateValues(up_z_values_.Data(),
                                                   frames_to_process);
  }
}

const float* AudioListener::GetPositionXValues(size_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return position_x_values_.Data();
}

const float* AudioListener::GetPositionYValues(size_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return position_y_values_.Data();
}

const float* AudioListener::GetPositionZValues(size_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return position_z_values_.Data();
}

const float* AudioListener::GetForwardXValues(size_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return forward_x_values_.Data();
}

const float* AudioListener::GetForwardYValues(size_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return forward_y_values_.Data();
}

const float* AudioListener::GetForwardZValues(size_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return forward_z_values_.Data();
}

const float* AudioListener::GetUpXValues(size_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return up_x_values_.Data();
}

const float* AudioListener::GetUpYValues(size_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return up_y_values_.Data();
}

const float* AudioListener::GetUpZValues(size_t frames_to_process) {
  UpdateValuesIfNeeded(frames_to_process);
  return up_z_values_.Data();
}

void AudioListener::UpdateState() {
  // This must be called from the audio thread in pre or post render phase of
  // the graph processing.  (AudioListener doesn't have access to the context
  // to check for the audio thread.)
  DCHECK(!IsMainThread());

  MutexTryLocker try_locker(listener_lock_);
  if (try_locker.Locked()) {
    FloatPoint3D current_position = GetPosition();
    FloatPoint3D current_forward = Orientation();
    FloatPoint3D current_up = UpVector();

    is_listener_dirty_ = current_position != last_position_ ||
                         current_forward != last_forward_ ||
                         current_up != last_up_;

    if (is_listener_dirty_) {
      last_position_ = current_position;
      last_forward_ = current_forward;
      last_up_ = current_up;
    }
  } else {
    // Main thread must be updating the position, forward, or up vector;
    // just assume the listener is dirty.  At worst, we'll do a little more
    // work than necessary for one rendering quantum.
    is_listener_dirty_ = true;
  }
}

void AudioListener::CreateAndLoadHRTFDatabaseLoader(float sample_rate) {
  DCHECK(IsMainThread());

  if (!hrtf_database_loader_)
    hrtf_database_loader_ =
        HRTFDatabaseLoader::CreateAndLoadAsynchronouslyIfNecessary(sample_rate);
}

bool AudioListener::IsHRTFDatabaseLoaded() {
  return hrtf_database_loader_ && hrtf_database_loader_->IsLoaded();
}

void AudioListener::WaitForHRTFDatabaseLoaderThreadCompletion() {
  if (hrtf_database_loader_)
    hrtf_database_loader_->WaitForLoaderThreadCompletion();
}

void AudioListener::MarkPannersAsDirty(unsigned type) {
  DCHECK(IsMainThread());
  for (PannerHandler* panner : panners_)
    panner->MarkPannerAsDirty(type);
}

void AudioListener::setPosition(const FloatPoint3D& position) {
  DCHECK(IsMainThread());

  // This synchronizes with panner's process().
  MutexLocker listener_locker(listener_lock_);
  position_x_->setValue(position.X());
  position_y_->setValue(position.Y());
  position_z_->setValue(position.Z());
  MarkPannersAsDirty(PannerHandler::kAzimuthElevationDirty |
                     PannerHandler::kDistanceConeGainDirty);
}

void AudioListener::setOrientation(const FloatPoint3D& orientation) {
  DCHECK(IsMainThread());

  // This synchronizes with panner's process().
  MutexLocker listener_locker(listener_lock_);
  forward_x_->setValue(orientation.X());
  forward_y_->setValue(orientation.Y());
  forward_z_->setValue(orientation.Z());
  MarkPannersAsDirty(PannerHandler::kAzimuthElevationDirty);
}

void AudioListener::SetUpVector(const FloatPoint3D& up_vector) {
  DCHECK(IsMainThread());

  // This synchronizes with panner's process().
  MutexLocker listener_locker(listener_lock_);
  up_x_->setValue(up_vector.X());
  up_y_->setValue(up_vector.Y());
  up_z_->setValue(up_vector.Z());
  MarkPannersAsDirty(PannerHandler::kAzimuthElevationDirty);
}

}  // namespace blink
