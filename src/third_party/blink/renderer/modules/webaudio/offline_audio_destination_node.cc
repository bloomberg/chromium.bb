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

#include "third_party/blink/renderer/modules/webaudio/offline_audio_destination_node.h"

#include <algorithm>
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_input.h"
#include "third_party/blink/renderer/modules/webaudio/audio_node_output.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet.h"
#include "third_party/blink/renderer/modules/webaudio/audio_worklet_messaging_proxy.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/platform/audio/audio_bus.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/denormal_disabler.h"
#include "third_party/blink/renderer/platform/audio/hrtf_database_loader.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

OfflineAudioDestinationHandler::OfflineAudioDestinationHandler(
    AudioNode& node,
    unsigned number_of_channels,
    uint32_t frames_to_process,
    float sample_rate)
    : AudioDestinationHandler(node),
      frames_processed_(0),
      frames_to_process_(frames_to_process),
      is_rendering_started_(false),
      number_of_channels_(number_of_channels),
      sample_rate_(sample_rate) {
  channel_count_ = number_of_channels;

  SetInternalChannelCountMode(kExplicit);
  SetInternalChannelInterpretation(AudioBus::kSpeakers);

  if (Context()->GetExecutionContext()) {
    main_thread_task_runner_ = Context()->GetExecutionContext()->GetTaskRunner(
        TaskType::kMiscPlatformAPI);
    DCHECK(main_thread_task_runner_->BelongsToCurrentThread());
  }
}

scoped_refptr<OfflineAudioDestinationHandler>
OfflineAudioDestinationHandler::Create(AudioNode& node,
                                       unsigned number_of_channels,
                                       uint32_t frames_to_process,
                                       float sample_rate) {
  return base::AdoptRef(new OfflineAudioDestinationHandler(
      node, number_of_channels, frames_to_process, sample_rate));
}

OfflineAudioDestinationHandler::~OfflineAudioDestinationHandler() {
  DCHECK(!IsInitialized());
}

void OfflineAudioDestinationHandler::Dispose() {
  Uninitialize();
  AudioDestinationHandler::Dispose();
}

void OfflineAudioDestinationHandler::Initialize() {
  if (IsInitialized())
    return;

  AudioHandler::Initialize();
}

void OfflineAudioDestinationHandler::Uninitialize() {
  if (!IsInitialized())
    return;

  render_thread_.reset();

  AudioHandler::Uninitialize();
}

OfflineAudioContext* OfflineAudioDestinationHandler::Context() const {
  return static_cast<OfflineAudioContext*>(AudioDestinationHandler::Context());
}

uint32_t OfflineAudioDestinationHandler::MaxChannelCount() const {
  return channel_count_;
}

void OfflineAudioDestinationHandler::StartRendering() {
  DCHECK(IsMainThread());
  DCHECK(shared_render_target_);
  DCHECK(render_thread_task_runner_);

  // Rendering was not started. Starting now.
  if (!is_rendering_started_) {
    is_rendering_started_ = true;
    PostCrossThreadTask(
        *render_thread_task_runner_, FROM_HERE,
        CrossThreadBindOnce(
            &OfflineAudioDestinationHandler::StartOfflineRendering,
            WrapRefCounted(this)));
    return;
  }

  // Rendering is already started, which implicitly means we resume the
  // rendering by calling |doOfflineRendering| on the render thread.
  PostCrossThreadTask(
      *render_thread_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&OfflineAudioDestinationHandler::DoOfflineRendering,
                          WrapRefCounted(this)));
}

void OfflineAudioDestinationHandler::StopRendering() {
  // offline audio rendering CANNOT BE stopped by JavaScript.
  NOTREACHED();
}

void OfflineAudioDestinationHandler::Pause() {
  NOTREACHED();
}

void OfflineAudioDestinationHandler::Resume() {
  NOTREACHED();
}

void OfflineAudioDestinationHandler::InitializeOfflineRenderThread(
    AudioBuffer* render_target) {
  DCHECK(IsMainThread());

  shared_render_target_ = render_target->CreateSharedAudioBuffer();
  render_bus_ = AudioBus::Create(render_target->numberOfChannels(),
                                 audio_utilities::kRenderQuantumFrames);
  DCHECK(render_bus_);

  PrepareTaskRunnerForRendering();
}

void OfflineAudioDestinationHandler::StartOfflineRendering() {
  DCHECK(!IsMainThread());

  DCHECK(render_bus_);
  if (!render_bus_)
    return;

  bool is_audio_context_initialized = Context()->IsDestinationInitialized();
  DCHECK(is_audio_context_initialized);
  if (!is_audio_context_initialized)
    return;

  bool channels_match = render_bus_->NumberOfChannels() ==
                        shared_render_target_->numberOfChannels();
  DCHECK(channels_match);
  if (!channels_match)
    return;

  bool is_render_bus_allocated =
      render_bus_->length() >= audio_utilities::kRenderQuantumFrames;
  DCHECK(is_render_bus_allocated);
  if (!is_render_bus_allocated)
    return;

  // Start rendering.
  DoOfflineRendering();
}

void OfflineAudioDestinationHandler::DoOfflineRendering() {
  DCHECK(!IsMainThread());

  unsigned number_of_channels;
  Vector<float*> destinations;
  {
    // Main thread GCs cannot happen while we're reading out channel
    // data. Detect that condition by trying to take the cross-thread
    // persistent lock which is held while a GC runs. If the lock is
    // already held, simply delay rendering until the next quantum.
    bool has_lock = ProcessHeap::CrossThreadPersistentMutex().TryLock();
    if (!has_lock) {
      // To ensure that the rendering step eventually happens, repost.
      render_thread_task_runner_->PostTask(
          FROM_HERE,
          WTF::Bind(&OfflineAudioDestinationHandler::DoOfflineRendering,
                    WrapRefCounted(this)));
      return;
    }

    number_of_channels = shared_render_target_->numberOfChannels();
    destinations.ReserveInitialCapacity(number_of_channels);
    for (unsigned i = 0; i < number_of_channels; ++i) {
      destinations.push_back(
          static_cast<float*>(shared_render_target_->channels()[i].Data()));
    }
    ProcessHeap::CrossThreadPersistentMutex().unlock();
  }

  // If there is more to process and there is no suspension at the moment,
  // do continue to render quanta. Then calling OfflineAudioContext.resume()
  // will pick up the render loop again from where it was suspended.
  while (frames_to_process_ > 0) {
    // Suspend the rendering if a scheduled suspend found at the current
    // sample frame. Otherwise render one quantum.
    if (RenderIfNotSuspended(nullptr, render_bus_.get(),
                             audio_utilities::kRenderQuantumFrames))
      return;

    uint32_t frames_available_to_copy =
        std::min(frames_to_process_, audio_utilities::kRenderQuantumFrames);

    for (unsigned channel_index = 0; channel_index < number_of_channels;
         ++channel_index) {
      const float* source = render_bus_->Channel(channel_index)->Data();
      memcpy(destinations[channel_index] + frames_processed_, source,
             sizeof(float) * frames_available_to_copy);
    }

    frames_processed_ += frames_available_to_copy;

    DCHECK_GE(frames_to_process_, frames_available_to_copy);
    frames_to_process_ -= frames_available_to_copy;
  }

  DCHECK_EQ(frames_to_process_, 0u);
  FinishOfflineRendering();
}

void OfflineAudioDestinationHandler::SuspendOfflineRendering() {
  DCHECK(!IsMainThread());

  // The actual rendering has been suspended. Notify the context.
  PostCrossThreadTask(
      *main_thread_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&OfflineAudioDestinationHandler::NotifySuspend,
                          WrapRefCounted(this),
                          Context()->CurrentSampleFrame()));
}

void OfflineAudioDestinationHandler::FinishOfflineRendering() {
  DCHECK(!IsMainThread());

  // The actual rendering has been completed. Notify the context.
  PostCrossThreadTask(
      *main_thread_task_runner_, FROM_HERE,
      CrossThreadBindOnce(&OfflineAudioDestinationHandler::NotifyComplete,
                          WrapRefCounted(this)));
}

void OfflineAudioDestinationHandler::NotifySuspend(size_t frame) {
  DCHECK(IsMainThread());

  if (!IsExecutionContextDestroyed() && Context())
    Context()->ResolveSuspendOnMainThread(frame);
}

void OfflineAudioDestinationHandler::NotifyComplete() {
  DCHECK(IsMainThread());

  render_thread_.reset();

  // If the execution context has been destroyed, there's no where to
  // send the notification, so just return.
  if (IsExecutionContextDestroyed()) {
    return;
  }

  // The OfflineAudioContext might be gone.
  if (Context() && Context()->GetExecutionContext())
    Context()->FireCompletionEvent();
}

bool OfflineAudioDestinationHandler::RenderIfNotSuspended(
    AudioBus* source_bus,
    AudioBus* destination_bus,
    uint32_t number_of_frames) {
  // We don't want denormals slowing down any of the audio processing
  // since they can very seriously hurt performance.
  // This will take care of all AudioNodes because they all process within this
  // scope.
  DenormalDisabler denormal_disabler;

  // Need to check if the context actually alive. Otherwise the subsequent
  // steps will fail. If the context is not alive somehow, return immediately
  // and do nothing.
  //
  // TODO(hongchan): because the context can go away while rendering, so this
  // check cannot guarantee the safe execution of the following steps.
  DCHECK(Context());
  if (!Context())
    return false;

  Context()->GetDeferredTaskHandler().SetAudioThreadToCurrentThread();

  // If the destination node is not initialized, pass the silence to the final
  // audio destination (one step before the FIFO). This check is for the case
  // where the destination is in the middle of tearing down process.
  if (!IsInitialized()) {
    destination_bus->Zero();
    return false;
  }

  // Take care pre-render tasks at the beginning of each render quantum. Then
  // it will stop the rendering loop if the context needs to be suspended
  // at the beginning of the next render quantum.
  if (Context()->HandlePreRenderTasks(nullptr, nullptr)) {
    SuspendOfflineRendering();
    return true;
  }

  DCHECK_GE(NumberOfInputs(), 1u);
  if (NumberOfInputs() < 1) {
    destination_bus->Zero();
    return false;
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
  AdvanceCurrentSampleFrame(number_of_frames);

  Context()->UpdateWorkletGlobalScopeOnRenderingThread();

  return false;
}

void OfflineAudioDestinationHandler::PrepareTaskRunnerForRendering() {
  DCHECK(IsMainThread());

  AudioWorklet* audio_worklet = Context()->audioWorklet();
  if (audio_worklet && audio_worklet->IsReady()) {
    if (!render_thread_) {
      // The context (re)started with the AudioWorklet mode. Assign the task
      // runner only when it is not set yet.
      if (!render_thread_task_runner_) {
        render_thread_task_runner_ =
            audio_worklet->GetMessagingProxy()->GetBackingWorkerThread()
                         ->GetTaskRunner(TaskType::kMiscPlatformAPI);
      }
    } else {
      // The AudioWorklet is activated and the render task runner should be
      // changed.
      render_thread_ = nullptr;
      render_thread_task_runner_ =
          audio_worklet->GetMessagingProxy()->GetBackingWorkerThread()
                       ->GetTaskRunner(TaskType::kMiscPlatformAPI);
    }
  } else {
    if (!render_thread_) {
      // The context started from the non-AudioWorklet mode.
      render_thread_ = Platform::Current()->CreateThread(
          ThreadCreationParams(WebThreadType::kOfflineAudioRenderThread));
      render_thread_task_runner_ = render_thread_->GetTaskRunner();
    }
  }

  // The task runner MUST be valid at this point.
  DCHECK(render_thread_task_runner_);
}

void OfflineAudioDestinationHandler::RestartRendering() {
  DCHECK(IsMainThread());

  // The rendering thread might have been changed, so we need to set up the
  // task runner again.
  PrepareTaskRunnerForRendering();
}

// ----------------------------------------------------------------

OfflineAudioDestinationNode::OfflineAudioDestinationNode(
    BaseAudioContext& context,
    unsigned number_of_channels,
    uint32_t frames_to_process,
    float sample_rate)
    : AudioDestinationNode(context) {
  SetHandler(OfflineAudioDestinationHandler::Create(
      *this, number_of_channels, frames_to_process, sample_rate));
}

OfflineAudioDestinationNode* OfflineAudioDestinationNode::Create(
    BaseAudioContext* context,
    unsigned number_of_channels,
    uint32_t frames_to_process,
    float sample_rate) {
  return MakeGarbageCollected<OfflineAudioDestinationNode>(
      *context, number_of_channels, frames_to_process, sample_rate);
}

void OfflineAudioDestinationNode::Trace(Visitor* visitor) {
  visitor->Trace(destination_buffer_);
  AudioDestinationNode::Trace(visitor);
}

}  // namespace blink
