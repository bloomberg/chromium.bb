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

#include "modules/webaudio/OfflineAudioDestinationNode.h"

#include <algorithm>
#include "core/dom/TaskRunnerHelper.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/BaseAudioContext.h"
#include "modules/webaudio/OfflineAudioContext.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/audio/AudioBus.h"
#include "platform/audio/AudioUtilities.h"
#include "platform/audio/DenormalDisabler.h"
#include "platform/audio/HRTFDatabaseLoader.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"

namespace blink {

OfflineAudioDestinationHandler::OfflineAudioDestinationHandler(
    AudioNode& node,
    AudioBuffer* render_target)
    : AudioDestinationHandler(node),
      render_target_(render_target),
      frames_processed_(0),
      frames_to_process_(0),
      is_rendering_started_(false),
      should_suspend_(false) {
  render_bus_ = AudioBus::Create(render_target->numberOfChannels(),
                                 AudioUtilities::kRenderQuantumFrames);
  frames_to_process_ = render_target_->length();

  // Node-specific defaults.
  channel_count_ = render_target_->numberOfChannels();
  SetInternalChannelCountMode(kExplicit);
  SetInternalChannelInterpretation(AudioBus::kSpeakers);
}

PassRefPtr<OfflineAudioDestinationHandler>
OfflineAudioDestinationHandler::Create(AudioNode& node,
                                       AudioBuffer* render_target) {
  return AdoptRef(new OfflineAudioDestinationHandler(node, render_target));
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

  if (render_thread_)
    render_thread_.reset();

  AudioHandler::Uninitialize();
}

OfflineAudioContext* OfflineAudioDestinationHandler::Context() const {
  return static_cast<OfflineAudioContext*>(AudioDestinationHandler::Context());
}

unsigned long OfflineAudioDestinationHandler::MaxChannelCount() const {
  return channel_count_;
}

void OfflineAudioDestinationHandler::StartRendering() {
  DCHECK(IsMainThread());
  DCHECK(render_thread_);
  DCHECK(render_target_);

  if (!render_target_)
    return;

  // Rendering was not started. Starting now.
  if (!is_rendering_started_) {
    is_rendering_started_ = true;
    render_thread_->GetWebTaskRunner()->PostTask(
        BLINK_FROM_HERE,
        CrossThreadBind(&OfflineAudioDestinationHandler::StartOfflineRendering,
                        WrapPassRefPtr(this)));
    return;
  }

  // Rendering is already started, which implicitly means we resume the
  // rendering by calling |doOfflineRendering| on the render thread.
  render_thread_->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(&OfflineAudioDestinationHandler::DoOfflineRendering,
                      WrapPassRefPtr(this)));
}

void OfflineAudioDestinationHandler::StopRendering() {
  // offline audio rendering CANNOT BE stopped by JavaScript.
  NOTREACHED();
}

size_t OfflineAudioDestinationHandler::CallbackBufferSize() const {
  // The callback buffer size has no meaning for an offline context.
  NOTREACHED();
  return 0;
}

void OfflineAudioDestinationHandler::InitializeOfflineRenderThread() {
  DCHECK(IsMainThread());
  render_thread_ = Platform::Current()->CreateThread("offline audio renderer");
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

  bool channels_match =
      render_bus_->NumberOfChannels() == render_target_->numberOfChannels();
  DCHECK(channels_match);
  if (!channels_match)
    return;

  bool is_render_bus_allocated =
      render_bus_->length() >= AudioUtilities::kRenderQuantumFrames;
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
    CrossThreadPersistentRegion::LockScope gc_lock(
        ProcessHeap::GetCrossThreadPersistentRegion(), true);
    if (!gc_lock.HasLock()) {
      // To ensure that the rendering step eventually happens, repost.
      render_thread_->GetWebTaskRunner()->PostTask(
          BLINK_FROM_HERE,
          Bind(&OfflineAudioDestinationHandler::DoOfflineRendering,
               WrapPassRefPtr(this)));
      return;
    }

    number_of_channels = render_target_->numberOfChannels();
    destinations.ReserveInitialCapacity(number_of_channels);
    for (unsigned i = 0; i < number_of_channels; ++i)
      destinations.push_back(render_target_->getChannelData(i).View()->Data());
  }

  // Reset the suspend flag.
  should_suspend_ = false;

  // If there is more to process and there is no suspension at the moment,
  // do continue to render quanta. Then calling OfflineAudioContext.resume()
  // will pick up the render loop again from where it was suspended.
  while (frames_to_process_ > 0 && !should_suspend_) {
    // Suspend the rendering and update m_shouldSuspend if a scheduled
    // suspend found at the current sample frame. Otherwise render one
    // quantum and return false.
    should_suspend_ = RenderIfNotSuspended(
        0, render_bus_.Get(), AudioUtilities::kRenderQuantumFrames);

    if (should_suspend_)
      return;

    size_t frames_available_to_copy =
        std::min(frames_to_process_,
                 static_cast<size_t>(AudioUtilities::kRenderQuantumFrames));

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

  // Finish up the rendering loop if there is no more to process.
  if (!frames_to_process_)
    FinishOfflineRendering();
}

void OfflineAudioDestinationHandler::SuspendOfflineRendering() {
  DCHECK(!IsMainThread());

  // The actual rendering has been suspended. Notify the context.
  if (Context()->GetExecutionContext()) {
    TaskRunnerHelper::Get(TaskType::kMediaElementEvent,
                          Context()->GetExecutionContext())
        ->PostTask(BLINK_FROM_HERE,
                   CrossThreadBind(
                       &OfflineAudioDestinationHandler::NotifySuspend,
                       WrapPassRefPtr(this), Context()->CurrentSampleFrame()));
  }
}

void OfflineAudioDestinationHandler::FinishOfflineRendering() {
  DCHECK(!IsMainThread());

  // The actual rendering has been completed. Notify the context.
  if (Context()->GetExecutionContext()) {
    TaskRunnerHelper::Get(TaskType::kMediaElementEvent,
                          Context()->GetExecutionContext())
        ->PostTask(
            BLINK_FROM_HERE,
            CrossThreadBind(&OfflineAudioDestinationHandler::NotifyComplete,
                            WrapPassRefPtr(this)));
  }
}

void OfflineAudioDestinationHandler::NotifySuspend(size_t frame) {
  DCHECK(IsMainThread());

  if (Context())
    Context()->ResolveSuspendOnMainThread(frame);
}

void OfflineAudioDestinationHandler::NotifyComplete() {
  DCHECK(IsMainThread());

  // Nullify the rendering thread to save the system resource.
  render_thread_.reset();

  // The OfflineAudioContext might be gone.
  if (Context())
    Context()->FireCompletionEvent();
}

bool OfflineAudioDestinationHandler::RenderIfNotSuspended(
    AudioBus* source_bus,
    AudioBus* destination_bus,
    size_t number_of_frames) {
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
  if (Context()->HandlePreOfflineRenderTasks()) {
    SuspendOfflineRendering();
    return true;
  }

  // Prepare the local audio input provider for this render quantum.
  if (source_bus)
    local_audio_input_provider_.Set(source_bus);

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
  Context()->HandlePostOfflineRenderTasks();

  // Advance current sample-frame.
  size_t new_sample_frame = current_sample_frame_ + number_of_frames;
  ReleaseStore(&current_sample_frame_, new_sample_frame);

  return false;
}

// ----------------------------------------------------------------

OfflineAudioDestinationNode::OfflineAudioDestinationNode(
    BaseAudioContext& context,
    AudioBuffer* render_target)
    : AudioDestinationNode(context) {
  SetHandler(OfflineAudioDestinationHandler::Create(*this, render_target));
}

OfflineAudioDestinationNode* OfflineAudioDestinationNode::Create(
    BaseAudioContext* context,
    AudioBuffer* render_target) {
  return new OfflineAudioDestinationNode(*context, render_target);
}

}  // namespace blink
