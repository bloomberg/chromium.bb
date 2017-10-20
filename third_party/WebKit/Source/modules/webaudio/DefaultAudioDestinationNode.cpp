/*
 * Copyright (C) 2011, Google Inc. All rights reserved.
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

#include "modules/webaudio/DefaultAudioDestinationNode.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "modules/webaudio/BaseAudioContext.h"
#include "modules/webaudio/AudioWorkletMessagingProxy.h"

namespace blink {

DefaultAudioDestinationHandler::DefaultAudioDestinationHandler(
    AudioNode& node,
    const WebAudioLatencyHint& latency_hint)
    : AudioDestinationHandler(node),
      number_of_input_channels_(0),
      latency_hint_(latency_hint) {
  // Node-specific default mixing rules.
  channel_count_ = 2;
  SetInternalChannelCountMode(kExplicit);
  SetInternalChannelInterpretation(AudioBus::kSpeakers);
}

scoped_refptr<DefaultAudioDestinationHandler>
DefaultAudioDestinationHandler::Create(
    AudioNode& node,
    const WebAudioLatencyHint& latency_hint) {
  return WTF::AdoptRef(new DefaultAudioDestinationHandler(node, latency_hint));
}

DefaultAudioDestinationHandler::~DefaultAudioDestinationHandler() {
  DCHECK(!IsInitialized());
}

void DefaultAudioDestinationHandler::Dispose() {
  Uninitialize();
  AudioDestinationHandler::Dispose();
}

void DefaultAudioDestinationHandler::Initialize() {
  DCHECK(IsMainThread());
  if (IsInitialized())
    return;

  CreateDestination();
  AudioHandler::Initialize();
}

void DefaultAudioDestinationHandler::Uninitialize() {
  DCHECK(IsMainThread());
  if (!IsInitialized())
    return;

  StopDestination();
  number_of_input_channels_ = 0;
  AudioHandler::Uninitialize();
}

void DefaultAudioDestinationHandler::CreateDestination() {
  destination_ = AudioDestination::Create(*this,
      ChannelCount(), latency_hint_, Context()->GetSecurityOrigin());
}

void DefaultAudioDestinationHandler::StartDestination() {
  // Use Experimental AudioWorkletThread only when AudioWorklet is enabled and
  // there is an active AudioWorkletGlobalScope.
  if (RuntimeEnabledFeatures::AudioWorkletEnabled() &&
      Context()->HasWorkletMessagingProxy()) {
    DCHECK(Context()->WorkletMessagingProxy()->GetWorkletBackingThread());
    destination_->StartWithWorkletThread(
        Context()->WorkletMessagingProxy()->GetWorkletBackingThread());
  } else {
    destination_->Start();
  }
}

void DefaultAudioDestinationHandler::StopDestination() {
  destination_->Stop();
}

void DefaultAudioDestinationHandler::StartRendering() {
  DCHECK(IsInitialized());
  if (IsInitialized()) {
    DCHECK(!destination_->IsPlaying());
    StartDestination();
  }
}

void DefaultAudioDestinationHandler::StopRendering() {
  DCHECK(IsInitialized());
  if (IsInitialized()) {
    DCHECK(destination_->IsPlaying());
    StopDestination();
  }
}

void DefaultAudioDestinationHandler::RestartDestination() {
  StopDestination();
  StartDestination();
}

unsigned long DefaultAudioDestinationHandler::MaxChannelCount() const {
  return AudioDestination::MaxChannelCount();
}

size_t DefaultAudioDestinationHandler::CallbackBufferSize() const {
  return destination_->CallbackBufferSize();
}

void DefaultAudioDestinationHandler::SetChannelCount(
    unsigned long channel_count,
    ExceptionState& exception_state) {
  // The channelCount for the input to this node controls the actual number of
  // channels we send to the audio hardware. It can only be set depending on the
  // maximum number of channels supported by the hardware.

  DCHECK(IsMainThread());

  if (!MaxChannelCount() || channel_count > MaxChannelCount()) {
    exception_state.ThrowDOMException(
        kIndexSizeError,
        ExceptionMessages::IndexOutsideRange<unsigned>(
            "channel count", channel_count, 1,
            ExceptionMessages::kInclusiveBound, MaxChannelCount(),
            ExceptionMessages::kInclusiveBound));
    return;
  }

  unsigned long old_channel_count = this->ChannelCount();
  AudioHandler::SetChannelCount(channel_count, exception_state);

  if (!exception_state.HadException() &&
      this->ChannelCount() != old_channel_count && IsInitialized()) {
    // Recreate/restart destination.
    StopDestination();
    CreateDestination();
    StartDestination();
  }
}

double DefaultAudioDestinationHandler::SampleRate() const {
  return destination_ ? destination_->SampleRate() : 0;
}

int DefaultAudioDestinationHandler::FramesPerBuffer() const {
  return destination_ ? destination_->FramesPerBuffer() : 0;
}

// ----------------------------------------------------------------

DefaultAudioDestinationNode::DefaultAudioDestinationNode(
    BaseAudioContext& context,
    const WebAudioLatencyHint& latency_hint)
    : AudioDestinationNode(context) {
  SetHandler(DefaultAudioDestinationHandler::Create(*this, latency_hint));
}

DefaultAudioDestinationNode* DefaultAudioDestinationNode::Create(
    BaseAudioContext* context,
    const WebAudioLatencyHint& latency_hint) {
  return new DefaultAudioDestinationNode(*context, latency_hint);
}

}  // namespace blink
