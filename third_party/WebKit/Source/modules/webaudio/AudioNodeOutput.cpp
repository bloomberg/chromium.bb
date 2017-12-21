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

#include "modules/webaudio/AudioNodeOutput.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/BaseAudioContext.h"
#include "platform/wtf/Threading.h"

namespace blink {

inline AudioNodeOutput::AudioNodeOutput(AudioHandler* handler,
                                        unsigned number_of_channels)
    : handler_(*handler),
      number_of_channels_(number_of_channels),
      desired_number_of_channels_(number_of_channels),
      is_in_place_(false),
      is_enabled_(true),
      did_call_dispose_(false),
      rendering_fan_out_count_(0),
      rendering_param_fan_out_count_(0) {
  DCHECK_LE(number_of_channels, BaseAudioContext::MaxNumberOfChannels());

  internal_bus_ = AudioBus::Create(number_of_channels,
                                   AudioUtilities::kRenderQuantumFrames);
}

std::unique_ptr<AudioNodeOutput> AudioNodeOutput::Create(
    AudioHandler* handler,
    unsigned number_of_channels) {
  return base::WrapUnique(new AudioNodeOutput(handler, number_of_channels));
}

void AudioNodeOutput::Dispose() {
  did_call_dispose_ = true;

  GetDeferredTaskHandler().RemoveMarkedAudioNodeOutput(this);
  DisconnectAll();
  DCHECK(inputs_.IsEmpty());
  DCHECK(params_.IsEmpty());
}

void AudioNodeOutput::SetNumberOfChannels(unsigned number_of_channels) {
  DCHECK_LE(number_of_channels, BaseAudioContext::MaxNumberOfChannels());
  DCHECK(GetDeferredTaskHandler().IsGraphOwner());

  desired_number_of_channels_ = number_of_channels;

  if (GetDeferredTaskHandler().IsAudioThread()) {
    // If we're in the audio thread then we can take care of it right away (we
    // should be at the very start or end of a rendering quantum).
    UpdateNumberOfChannels();
  } else {
    DCHECK(!did_call_dispose_);
    // Let the context take care of it in the audio thread in the pre and post
    // render tasks.
    GetDeferredTaskHandler().MarkAudioNodeOutputDirty(this);
  }
}

void AudioNodeOutput::UpdateInternalBus() {
  if (NumberOfChannels() == internal_bus_->NumberOfChannels())
    return;

  internal_bus_ = AudioBus::Create(NumberOfChannels(),
                                   AudioUtilities::kRenderQuantumFrames);
}

void AudioNodeOutput::UpdateRenderingState() {
  UpdateNumberOfChannels();
  rendering_fan_out_count_ = FanOutCount();
  rendering_param_fan_out_count_ = ParamFanOutCount();
}

void AudioNodeOutput::UpdateNumberOfChannels() {
  DCHECK(GetDeferredTaskHandler().IsAudioThread());
  DCHECK(GetDeferredTaskHandler().IsGraphOwner());

  if (number_of_channels_ != desired_number_of_channels_) {
    number_of_channels_ = desired_number_of_channels_;
    UpdateInternalBus();
    PropagateChannelCount();
  }
}

void AudioNodeOutput::PropagateChannelCount() {
  DCHECK(GetDeferredTaskHandler().IsAudioThread());
  DCHECK(GetDeferredTaskHandler().IsGraphOwner());

  if (IsChannelCountKnown()) {
    // Announce to any nodes we're connected to that we changed our channel
    // count for its input.
    for (AudioNodeInput* i : inputs_)
      i->Handler().CheckNumberOfChannelsForInput(i);
  }
}

AudioBus* AudioNodeOutput::Pull(AudioBus* in_place_bus,
                                size_t frames_to_process) {
  DCHECK(GetDeferredTaskHandler().IsAudioThread());
  DCHECK(rendering_fan_out_count_ > 0 || rendering_param_fan_out_count_ > 0);

  // Causes our AudioNode to process if it hasn't already for this render
  // quantum.  We try to do in-place processing (using inPlaceBus) if at all
  // possible, but we can't process in-place if we're connected to more than one
  // input (fan-out > 1).  In this case pull() is called multiple times per
  // rendering quantum, and the processIfNecessary() call below will cause our
  // node to process() only the first time, caching the output in
  // m_internalOutputBus for subsequent calls.

  is_in_place_ =
      in_place_bus && in_place_bus->NumberOfChannels() == NumberOfChannels() &&
      (rendering_fan_out_count_ + rendering_param_fan_out_count_) == 1;

  in_place_bus_ = is_in_place_ ? in_place_bus : nullptr;

  Handler().ProcessIfNecessary(frames_to_process);
  return Bus();
}

AudioBus* AudioNodeOutput::Bus() const {
  DCHECK(GetDeferredTaskHandler().IsAudioThread());
  return is_in_place_ ? in_place_bus_.get() : internal_bus_.get();
}

unsigned AudioNodeOutput::FanOutCount() {
  DCHECK(GetDeferredTaskHandler().IsGraphOwner());
  return inputs_.size();
}

unsigned AudioNodeOutput::ParamFanOutCount() {
  DCHECK(GetDeferredTaskHandler().IsGraphOwner());
  return params_.size();
}

unsigned AudioNodeOutput::RenderingFanOutCount() const {
  return rendering_fan_out_count_;
}

void AudioNodeOutput::AddInput(AudioNodeInput& input) {
  DCHECK(GetDeferredTaskHandler().IsGraphOwner());
  inputs_.insert(&input);
  input.Handler().MakeConnection();
}

void AudioNodeOutput::RemoveInput(AudioNodeInput& input) {
  DCHECK(GetDeferredTaskHandler().IsGraphOwner());
  input.Handler().BreakConnection();
  inputs_.erase(&input);
}

void AudioNodeOutput::DisconnectAllInputs() {
  DCHECK(GetDeferredTaskHandler().IsGraphOwner());

  // AudioNodeInput::disconnect() changes m_inputs by calling removeInput().
  while (!inputs_.IsEmpty())
    (*inputs_.begin())->Disconnect(*this);
}

void AudioNodeOutput::DisconnectInput(AudioNodeInput& input) {
  DCHECK(GetDeferredTaskHandler().IsGraphOwner());
  DCHECK(IsConnectedToInput(input));
  input.Disconnect(*this);
}

void AudioNodeOutput::DisconnectAudioParam(AudioParamHandler& param) {
  DCHECK(GetDeferredTaskHandler().IsGraphOwner());
  DCHECK(IsConnectedToAudioParam(param));
  param.Disconnect(*this);
}

void AudioNodeOutput::AddParam(AudioParamHandler& param) {
  DCHECK(GetDeferredTaskHandler().IsGraphOwner());
  params_.insert(&param);
}

void AudioNodeOutput::RemoveParam(AudioParamHandler& param) {
  DCHECK(GetDeferredTaskHandler().IsGraphOwner());
  params_.erase(&param);
}

void AudioNodeOutput::DisconnectAllParams() {
  DCHECK(GetDeferredTaskHandler().IsGraphOwner());

  // AudioParam::disconnect() changes m_params by calling removeParam().
  while (!params_.IsEmpty())
    (*params_.begin())->Disconnect(*this);
}

void AudioNodeOutput::DisconnectAll() {
  DisconnectAllInputs();
  DisconnectAllParams();
}

bool AudioNodeOutput::IsConnectedToInput(AudioNodeInput& input) {
  DCHECK(GetDeferredTaskHandler().IsGraphOwner());
  return inputs_.Contains(&input);
}

bool AudioNodeOutput::IsConnectedToAudioParam(AudioParamHandler& param) {
  DCHECK(GetDeferredTaskHandler().IsGraphOwner());
  return params_.Contains(&param);
}

void AudioNodeOutput::Disable() {
  DCHECK(GetDeferredTaskHandler().IsGraphOwner());

  if (is_enabled_) {
    is_enabled_ = false;
    for (AudioNodeInput* i : inputs_)
      i->Disable(*this);
  }
}

void AudioNodeOutput::Enable() {
  DCHECK(GetDeferredTaskHandler().IsGraphOwner());

  if (!is_enabled_) {
    is_enabled_ = true;
    for (AudioNodeInput* i : inputs_)
      i->Enable(*this);
  }
}

}  // namespace blink
