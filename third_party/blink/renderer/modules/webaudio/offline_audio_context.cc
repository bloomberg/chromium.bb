/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
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

#include "third_party/blink/renderer/bindings/core/v8/exception_messages.h"
#include "third_party/blink/renderer/bindings/core/v8/exception_state.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/dom/exception_code.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/webaudio/audio_listener.h"
#include "third_party/blink/renderer/modules/webaudio/deferred_task_handler.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_completion_event.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_context_options.h"
#include "third_party/blink/renderer/modules/webaudio/offline_audio_destination_node.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"

#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/histogram.h"

namespace blink {

OfflineAudioContext* OfflineAudioContext::Create(
    ExecutionContext* context,
    unsigned number_of_channels,
    unsigned number_of_frames,
    float sample_rate,
    ExceptionState& exception_state) {
  // FIXME: add support for workers.
  if (!context || !context->IsDocument()) {
    exception_state.ThrowDOMException(kNotSupportedError,
                                      "Workers are not supported.");
    return nullptr;
  }

  Document* document = ToDocument(context);

  if (!number_of_frames) {
    exception_state.ThrowDOMException(
        kNotSupportedError,
        ExceptionMessages::IndexExceedsMinimumBound<unsigned>(
            "number of frames", number_of_frames, 1));
    return nullptr;
  }

  if (number_of_channels == 0 ||
      number_of_channels > BaseAudioContext::MaxNumberOfChannels()) {
    exception_state.ThrowDOMException(
        kNotSupportedError, ExceptionMessages::IndexOutsideRange<unsigned>(
                                "number of channels", number_of_channels, 1,
                                ExceptionMessages::kInclusiveBound,
                                BaseAudioContext::MaxNumberOfChannels(),
                                ExceptionMessages::kInclusiveBound));
    return nullptr;
  }

  if (!AudioUtilities::IsValidAudioBufferSampleRate(sample_rate)) {
    exception_state.ThrowDOMException(
        kNotSupportedError, ExceptionMessages::IndexOutsideRange(
                                "sampleRate", sample_rate,
                                AudioUtilities::MinAudioBufferSampleRate(),
                                ExceptionMessages::kInclusiveBound,
                                AudioUtilities::MaxAudioBufferSampleRate(),
                                ExceptionMessages::kInclusiveBound));
    return nullptr;
  }

  OfflineAudioContext* audio_context =
      new OfflineAudioContext(document, number_of_channels, number_of_frames,
                              sample_rate, exception_state);
  audio_context->PauseIfNeeded();

#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: OfflineAudioContext::OfflineAudioContext()\n",
          audio_context);
#endif
  DEFINE_STATIC_LOCAL(SparseHistogram, offline_context_channel_count_histogram,
                      ("WebAudio.OfflineAudioContext.ChannelCount"));
  // Arbitrarly limit the maximum length to 1 million frames (about 20 sec
  // at 48kHz).  The number of buckets is fairly arbitrary.
  DEFINE_STATIC_LOCAL(CustomCountHistogram, offline_context_length_histogram,
                      ("WebAudio.OfflineAudioContext.Length", 1, 1000000, 50));
  // The limits are the min and max AudioBuffer sample rates currently
  // supported.  We use explicit values here instead of
  // AudioUtilities::minAudioBufferSampleRate() and
  // AudioUtilities::maxAudioBufferSampleRate().  The number of buckets is
  // fairly arbitrary.
  DEFINE_STATIC_LOCAL(
      CustomCountHistogram, offline_context_sample_rate_histogram,
      ("WebAudio.OfflineAudioContext.SampleRate384kHz", 3000, 384000, 50));

  offline_context_channel_count_histogram.Sample(number_of_channels);
  offline_context_length_histogram.Count(number_of_frames);
  offline_context_sample_rate_histogram.Count(sample_rate);

  return audio_context;
}

OfflineAudioContext* OfflineAudioContext::Create(
    ExecutionContext* context,
    const OfflineAudioContextOptions& options,
    ExceptionState& exception_state) {
  OfflineAudioContext* offline_context =
      Create(context, options.numberOfChannels(), options.length(),
             options.sampleRate(), exception_state);

  return offline_context;
}

OfflineAudioContext::OfflineAudioContext(Document* document,
                                         unsigned number_of_channels,
                                         size_t number_of_frames,
                                         float sample_rate,
                                         ExceptionState& exception_state)
    : BaseAudioContext(document, kOfflineContext),
      is_rendering_started_(false),
      total_render_frames_(number_of_frames) {
  destination_node_ = OfflineAudioDestinationNode::Create(
      this, number_of_channels, number_of_frames, sample_rate);
  Initialize();
}

OfflineAudioContext::~OfflineAudioContext() {
#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: OfflineAudioContext::~OfflineAudioContext()\n",
          this);
#endif
}

void OfflineAudioContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(complete_resolver_);
  visitor->Trace(scheduled_suspends_);
  BaseAudioContext::Trace(visitor);
}

ScriptPromise OfflineAudioContext::startOfflineRendering(
    ScriptState* script_state) {
  DCHECK(IsMainThread());

  // Calling close() on an OfflineAudioContext is not supported/allowed,
  // but it might well have been stopped by its execution context.
  //
  // See: crbug.com/435867
  if (IsContextClosed()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidStateError,
                             "cannot call startRendering on an "
                             "OfflineAudioContext in a stopped state."));
  }

  // If the context is not in the suspended state (i.e. running), reject the
  // promise.
  if (ContextState() != AudioContextState::kSuspended) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(
            kInvalidStateError,
            "cannot startRendering when an OfflineAudioContext is " + state()));
  }

  // Can't call startRendering more than once.  Return a rejected promise now.
  if (is_rendering_started_) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidStateError,
                             "cannot call startRendering more than once"));
  }

  DCHECK(!is_rendering_started_);

  complete_resolver_ = ScriptPromiseResolver::Create(script_state);

  // Allocate the AudioBuffer to hold the rendered result.
  float sample_rate = DestinationHandler().SampleRate();
  unsigned number_of_channels = DestinationHandler().NumberOfChannels();

  AudioBuffer* render_target = AudioBuffer::CreateUninitialized(
      number_of_channels, total_render_frames_, sample_rate);

  if (!render_target) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kNotSupportedError,
                             "startRendering failed to create AudioBuffer(" +
                                 String::Number(number_of_channels) + ", " +
                                 String::Number(total_render_frames_) + ", " +
                                 String::Number(sample_rate) + ")"));
  }

  // Start rendering and return the promise.
  is_rendering_started_ = true;
  SetContextState(kRunning);
  DestinationHandler().InitializeOfflineRenderThread(render_target);
  DestinationHandler().StartRendering();

  return complete_resolver_->Promise();
}

ScriptPromise OfflineAudioContext::suspendContext(ScriptState* script_state) {
  LOG(FATAL) << "This CANNOT be called on OfflineAudioContext; this is only to "
                "implement the pure virtual interface from BaseAudioContext.";
  return ScriptPromise();
}

ScriptPromise OfflineAudioContext::suspendContext(ScriptState* script_state,
                                                  double when) {
  DCHECK(IsMainThread());

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // If the rendering is finished, reject the promise.
  if (ContextState() == AudioContextState::kClosed) {
    resolver->Reject(DOMException::Create(kInvalidStateError,
                                          "the rendering is already finished"));
    return promise;
  }

  // The specified suspend time is negative; reject the promise.
  if (when < 0) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError,
        "negative suspend time (" + String::Number(when) + ") is not allowed"));
    return promise;
  }

  // The suspend time should be earlier than the total render frame. If the
  // requested suspension time is equal to the total render frame, the promise
  // will be rejected.
  double total_render_duration = total_render_frames_ / sampleRate();
  if (total_render_duration <= when) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError,
        "cannot schedule a suspend at " +
            String::NumberToStringECMAScript(when) +
            " seconds because it is greater than "
            "or equal to the total "
            "render duration of " +
            String::Number(total_render_frames_) + " frames (" +
            String::NumberToStringECMAScript(total_render_duration) +
            " seconds)"));
    return promise;
  }

  // Quantize (to the lower boundary) the suspend time by the render quantum.
  size_t frame = when * sampleRate();
  frame -= frame % DestinationHandler().RenderQuantumFrames();

  // The specified suspend time is in the past; reject the promise.
  if (frame < CurrentSampleFrame()) {
    size_t current_frame_clamped = std::min(CurrentSampleFrame(), length());
    double current_time_clamped =
        std::min(currentTime(), length() / static_cast<double>(sampleRate()));
    resolver->Reject(DOMException::Create(
        kInvalidStateError,
        "suspend(" + String::Number(when) + ") failed to suspend at frame " +
            String::Number(frame) + " because it is earlier than the current " +
            "frame of " + String::Number(current_frame_clamped) + " (" +
            String::Number(current_time_clamped) + " seconds)"));
    return promise;
  }

  // Wait until the suspend map is available for the insertion. Here we should
  // use GraphAutoLocker because it locks the graph from the main thread.
  GraphAutoLocker locker(this);

  // If there is a duplicate suspension at the same quantized frame,
  // reject the promise.
  if (scheduled_suspends_.Contains(frame)) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError, "cannot schedule more than one suspend at frame " +
                                String::Number(frame) + " (" +
                                String::Number(when) + " seconds)"));
    return promise;
  }

  scheduled_suspends_.insert(frame, resolver);

  return promise;
}

ScriptPromise OfflineAudioContext::resumeContext(ScriptState* script_state) {
  DCHECK(IsMainThread());

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // If the rendering has not started, reject the promise.
  if (!is_rendering_started_) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError,
        "cannot resume an offline context that has not started"));
    return promise;
  }

  // If the context is in a closed state, reject the promise.
  if (ContextState() == AudioContextState::kClosed) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError, "cannot resume a closed offline context"));
    return promise;
  }

  // If the context is already running, resolve the promise without altering
  // the current state or starting the rendering loop.
  if (ContextState() == AudioContextState::kRunning) {
    resolver->Resolve();
    return promise;
  }

  DCHECK_EQ(ContextState(), AudioContextState::kSuspended);

  // If the context is suspended, resume rendering by setting the state to
  // "Running". and calling startRendering(). Note that resuming is possible
  // only after the rendering started.
  SetContextState(kRunning);
  DestinationHandler().StartRendering();

  // Resolve the promise immediately.
  resolver->Resolve();

  return promise;
}

void OfflineAudioContext::FireCompletionEvent() {
  DCHECK(IsMainThread());

  // Context is finished, so remove any tail processing nodes; there's nowhere
  // for the output to go.
  GetDeferredTaskHandler().FinishTailProcessing();

  // We set the state to closed here so that the oncomplete event handler sees
  // that the context has been closed.
  SetContextState(kClosed);

  // Avoid firing the event if the document has already gone away.
  if (GetExecutionContext()) {
    AudioBuffer* rendered_buffer = DestinationHandler().RenderTarget();

    DCHECK(rendered_buffer);
    if (!rendered_buffer)
      return;

    // Call the offline rendering completion event listener and resolve the
    // promise too.
    DispatchEvent(OfflineAudioCompletionEvent::Create(rendered_buffer));
    complete_resolver_->Resolve(rendered_buffer);
  } else {
    // The resolver should be rejected when the execution context is gone.
    complete_resolver_->Reject(DOMException::Create(
        kInvalidStateError, "the execution context does not exist"));
  }

  is_rendering_started_ = false;
}

bool OfflineAudioContext::HandlePreOfflineRenderTasks() {
  DCHECK(IsAudioThread());

  // OfflineGraphAutoLocker here locks the audio graph for this scope. Note
  // that this locker does not use tryLock() inside because the timing of
  // suspension MUST NOT be delayed.
  OfflineGraphAutoLocker locker(this);

  // Update the dirty state of the listener.
  listener()->UpdateState();

  GetDeferredTaskHandler().HandleDeferredTasks();
  HandleStoppableSourceNodes();

  return ShouldSuspend();
}

void OfflineAudioContext::HandlePostOfflineRenderTasks() {
  DCHECK(IsAudioThread());

  // OfflineGraphAutoLocker here locks the audio graph for the same reason
  // above in |handlePreOfflineRenderTasks|.
  {
    OfflineGraphAutoLocker locker(this);

    GetDeferredTaskHandler().BreakConnections();
    GetDeferredTaskHandler().HandleDeferredTasks();
    GetDeferredTaskHandler().RequestToDeleteHandlersOnMainThread();
  }
}

OfflineAudioDestinationHandler& OfflineAudioContext::DestinationHandler() {
  return static_cast<OfflineAudioDestinationHandler&>(
      destination()->GetAudioDestinationHandler());
}

void OfflineAudioContext::ResolveSuspendOnMainThread(size_t frame) {
  DCHECK(IsMainThread());

  // Suspend the context first. This will fire onstatechange event.
  SetContextState(kSuspended);

  // Wait until the suspend map is available for the removal.
  GraphAutoLocker locker(this);

  // If the context is going away, m_scheduledSuspends could have had all its
  // entries removed.  Check for that here.
  if (scheduled_suspends_.size()) {
    // |frame| must exist in the map.
    DCHECK(scheduled_suspends_.Contains(frame));

    SuspendMap::iterator it = scheduled_suspends_.find(frame);
    it->value->Resolve();

    scheduled_suspends_.erase(it);
  }
}

void OfflineAudioContext::RejectPendingResolvers() {
  DCHECK(IsMainThread());

  // Wait until the suspend map is available for removal.
  GraphAutoLocker locker(this);

  // Offline context is going away so reject any promises that are still
  // pending.

  for (auto& pending_suspend_resolver : scheduled_suspends_) {
    pending_suspend_resolver.value->Reject(DOMException::Create(
        kInvalidStateError, "Audio context is going away"));
  }

  scheduled_suspends_.clear();
  DCHECK_EQ(resume_resolvers_.size(), 0u);

  RejectPendingDecodeAudioDataResolvers();
}

bool OfflineAudioContext::ShouldSuspend() {
  DCHECK(IsAudioThread());

  // Note that the GraphLock is required before this check. Since this needs
  // to run on the audio thread, OfflineGraphAutoLocker must be used.
  if (scheduled_suspends_.Contains(CurrentSampleFrame()))
    return true;

  return false;
}

bool OfflineAudioContext::HasPendingActivity() const {
  return is_rendering_started_;
}

}  // namespace blink
