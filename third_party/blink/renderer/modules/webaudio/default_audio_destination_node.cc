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

#include "third_party/blink/renderer/modules/webaudio/default_audio_destination_node.h"

#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/denormal_disabler.h"
#include "third_party/blink/renderer/platform/instrumentation/tracing/trace_event.h"
#include "third_party/blink/renderer/platform/wtf/atomics.h"

namespace blink {

DefaultAudioDestinationHandler::DefaultAudioDestinationHandler(
    AudioNode& node,
    const WebAudioLatencyHint& latency_hint)
    : AudioDestinationHandler(node),
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
  return base::AdoptRef(new DefaultAudioDestinationHandler(node, latency_hint));
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

  if (destination_->IsPlaying())
    StopDestination();

  AudioHandler::Uninitialize();
}

void DefaultAudioDestinationHandler::CreateDestination() {
  destination_ = AudioDestination::Create(*this, ChannelCount(), latency_hint_);
}

void DefaultAudioDestinationHandler::StartDestination() {
  DCHECK(!destination_->IsPlaying());

  AudioWorklet* audio_worklet = Context()->audioWorklet();
  if (audio_worklet && audio_worklet->IsReady()) {
    // This task runner is only used to fire the audio render callback, so it
    // MUST not be throttled to avoid potential audio glitch.
    destination_->StartWithWorkletTaskRunner(
        audio_worklet->GetMessagingProxy()
            ->GetBackingWorkerThread()
            ->GetTaskRunner(TaskType::kInternalMedia));
  } else {
    destination_->Start();
  }
}

void DefaultAudioDestinationHandler::StopDestination() {
  DCHECK(destination_->IsPlaying());
  destination_->Stop();
}

void DefaultAudioDestinationHandler::StartRendering() {
  DCHECK(IsInitialized());
  // Context might try to start rendering again while the destination is
  // running. Ignore it when that happens.
  if (IsInitialized() && !destination_->IsPlaying()) {
    StartDestination();
  }
}

void DefaultAudioDestinationHandler::StopRendering() {
  DCHECK(IsInitialized());
  // Context might try to stop rendering again while the destination is stopped.
  // Ignore it when that happens.
  if (IsInitialized() && destination_->IsPlaying()) {
    StopDestination();
  }
}

void DefaultAudioDestinationHandler::RestartRendering() {
  StopRendering();
  StartRendering();
}

unsigned long DefaultAudioDestinationHandler::MaxChannelCount() const {
  return AudioDestination::MaxChannelCount();
}

size_t DefaultAudioDestinationHandler::GetCallbackBufferSize() const {
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
        DOMExceptionCode::kIndexSizeError,
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

int DefaultAudioDestinationHandler::GetFramesPerBuffer() const {
  return destination_ ? destination_->FramesPerBuffer() : 0;
}

void DefaultAudioDestinationHandler::Render(
    AudioBus* destination_bus,
    size_t number_of_frames,
    const AudioIOPosition& output_position) {
  TRACE_EVENT0("webaudio", "DefaultAudioDestinationHandler::Render");

  // We don't want denormals slowing down any of the audio processing
  // since they can very seriously hurt performance.  This will take care of all
  // AudioNodes because they all process within this scope.
  DenormalDisabler denormal_disabler;

  // Need to check if the context actually alive. Otherwise the subsequent
  // steps will fail. If the context is not alive somehow, return immediately
  // and do nothing.
  //
  // TODO(hongchan): because the context can go away while rendering, so this
  // check cannot guarantee the safe execution of the following steps.
  DCHECK(Context());
  if (!Context())
    return;

  Context()->GetDeferredTaskHandler().SetAudioThreadToCurrentThread();

  // If the destination node is not initialized, pass the silence to the final
  // audio destination (one step before the FIFO). This check is for the case
  // where the destination is in the middle of tearing down process.
  if (!IsInitialized()) {
    destination_bus->Zero();
    return;
  }

  // Let the context take care of any business at the start of each render
  // quantum.
  Context()->HandlePreRenderTasks(output_position);

  DCHECK_GE(NumberOfInputs(), 1u);
  if (NumberOfInputs() < 1) {
    destination_bus->Zero();
    return;
  }
  // This will cause the node(s) connected to us to process, which in turn will
  // pull on their input(s), all the way backwards through the rendering graph.
  AudioBus* rendered_bus = Input(0).Pull(destination_bus, number_of_frames);

  if (!rendered_bus) {
    destination_bus->Zero();
  } else if (rendered_bus != destination_bus) {
    // in-place processing was not possible - so copy
    destination_bus->CopyFrom(*rendered_bus);
  }

  // Process nodes which need a little extra help because they are not connected
  // to anything, but still need to process.
  Context()->GetDeferredTaskHandler().ProcessAutomaticPullNodes(
      number_of_frames);

  // Let the context take care of any business at the end of each render
  // quantum.
  Context()->HandlePostRenderTasks();

  // Advance current sample-frame.
  size_t new_sample_frame = current_sample_frame_ + number_of_frames;
  ReleaseStore(&current_sample_frame_, new_sample_frame);

  Context()->UpdateWorkletGlobalScopeOnRenderingThread();
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
