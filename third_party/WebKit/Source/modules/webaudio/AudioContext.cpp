// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/webaudio/AudioContext.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/UseCounter.h"
#include "core/timing/DOMWindowPerformance.h"
#include "core/timing/Performance.h"
#include "modules/webaudio/AudioContextOptions.h"
#include "modules/webaudio/AudioTimestamp.h"
#include "modules/webaudio/DefaultAudioDestinationNode.h"
#include "platform/Histogram.h"
#include "platform/audio/AudioUtilities.h"
#include "public/platform/WebAudioLatencyHint.h"

#if DEBUG_AUDIONODE_REFERENCES
#include <stdio.h>
#endif

namespace blink {

// Don't allow more than this number of simultaneous AudioContexts
// talking to hardware.
const unsigned kMaxHardwareContexts = 6;
static unsigned g_hardware_context_count = 0;
static unsigned g_context_id = 0;

AudioContext* AudioContext::Create(Document& document,
                                   const AudioContextOptions& context_options,
                                   ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  UseCounter::CountCrossOriginIframe(
      document, WebFeature::kAudioContextCrossOriginIframe);

  if (g_hardware_context_count >= kMaxHardwareContexts) {
    exception_state.ThrowDOMException(
        kNotSupportedError,
        ExceptionMessages::IndexExceedsMaximumBound(
            "number of hardware contexts", g_hardware_context_count,
            kMaxHardwareContexts));
    return nullptr;
  }

  WebAudioLatencyHint latency_hint(WebAudioLatencyHint::kCategoryInteractive);
  if (context_options.latencyHint().isAudioContextLatencyCategory()) {
    latency_hint = WebAudioLatencyHint(
        context_options.latencyHint().getAsAudioContextLatencyCategory());
  } else if (context_options.latencyHint().isDouble()) {
    // This should be the requested output latency in seconds, without taking
    // into account double buffering (same as baseLatency).
    latency_hint =
        WebAudioLatencyHint(context_options.latencyHint().getAsDouble());
  }

  AudioContext* audio_context = new AudioContext(document, latency_hint);
  audio_context->SuspendIfNeeded();

  if (!AudioUtilities::IsValidAudioBufferSampleRate(
          audio_context->sampleRate())) {
    exception_state.ThrowDOMException(
        kNotSupportedError,
        ExceptionMessages::IndexOutsideRange(
            "hardware sample rate", audio_context->sampleRate(),
            AudioUtilities::MinAudioBufferSampleRate(),
            ExceptionMessages::kInclusiveBound,
            AudioUtilities::MaxAudioBufferSampleRate(),
            ExceptionMessages::kInclusiveBound));
    return audio_context;
  }
  // This starts the audio thread. The destination node's
  // provideInput() method will now be called repeatedly to render
  // audio.  Each time provideInput() is called, a portion of the
  // audio stream is rendered. Let's call this time period a "render
  // quantum". NOTE: for now AudioContext does not need an explicit
  // startRendering() call from JavaScript.  We may want to consider
  // requiring it for symmetry with OfflineAudioContext.
  audio_context->MaybeUnlockUserGesture();
  if (audio_context->IsAllowedToStart()) {
    audio_context->StartRendering();
    audio_context->SetContextState(kRunning);
  }
  ++g_hardware_context_count;
#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: AudioContext::AudioContext(): %u #%u\n",
          audio_context, audio_context->context_id_, g_hardware_context_count);
#endif

  DEFINE_STATIC_LOCAL(SparseHistogram, max_channel_count_histogram,
                      ("WebAudio.AudioContext.MaxChannelsAvailable"));
  DEFINE_STATIC_LOCAL(SparseHistogram, sample_rate_histogram,
                      ("WebAudio.AudioContext.HardwareSampleRate"));
  max_channel_count_histogram.Sample(
      audio_context->destination()->maxChannelCount());
  sample_rate_histogram.Sample(audio_context->sampleRate());

  return audio_context;
}

AudioContext::AudioContext(Document& document,
                           const WebAudioLatencyHint& latency_hint)
    : BaseAudioContext(&document, kRealtimeContext),
      context_id_(g_context_id++) {
  destination_node_ = DefaultAudioDestinationNode::Create(this, latency_hint);
  Initialize();
}

AudioContext::~AudioContext() {
#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: AudioContext::~AudioContext(): %u\n", this,
          context_id_);
#endif
}

DEFINE_TRACE(AudioContext) {
  visitor->Trace(close_resolver_);
  BaseAudioContext::Trace(visitor);
}

ScriptPromise AudioContext::suspendContext(ScriptState* script_state) {
  DCHECK(IsMainThread());
  AutoLocker locker(this);

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (ContextState() == kClosed) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError, "Cannot suspend a context that has been closed"));
  } else {
    // Stop rendering now.
    if (destination())
      StopRendering();

    // Since we don't have any way of knowing when the hardware actually stops,
    // we'll just resolve the promise now.
    resolver->Resolve();
  }

  return promise;
}

ScriptPromise AudioContext::resumeContext(ScriptState* script_state) {
  DCHECK(IsMainThread());

  if (IsContextClosed()) {
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidAccessError,
                             "cannot resume a closed AudioContext"));
  }

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  // If we're already running, just resolve; nothing else needs to be
  // done.
  if (ContextState() == kRunning) {
    resolver->Resolve();
    return promise;
  }
  // Restart the destination node to pull on the audio graph.
  if (destination()) {
    MaybeUnlockUserGesture();
    if (IsAllowedToStart()) {
      // Do not set the state to running here.  We wait for the
      // destination to start to set the state.
      StartRendering();
    }
  }

  // Save the resolver which will get resolved when the destination node starts
  // pulling on the graph again.
  {
    AutoLocker locker(this);
    resume_resolvers_.push_back(resolver);
  }

  return promise;
}

void AudioContext::getOutputTimestamp(ScriptState* script_state,
                                      AudioTimestamp& result) {
  DCHECK(IsMainThread());
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!window)
    return;

  if (!destination()) {
    result.setContextTime(0.0);
    result.setPerformanceTime(0.0);
    return;
  }

  Performance* performance = DOMWindowPerformance::performance(*window);
  DCHECK(performance);

  AudioIOPosition position = OutputPosition();

  double performance_time =
      performance->MonotonicTimeToDOMHighResTimeStamp(position.timestamp);
  if (performance_time < 0.0)
    performance_time = 0.0;

  result.setContextTime(position.position);
  result.setPerformanceTime(performance_time);
}

ScriptPromise AudioContext::closeContext(ScriptState* script_state) {
  if (IsContextClosed()) {
    // We've already closed the context previously, but it hasn't yet been
    // resolved, so just create a new promise and reject it.
    return ScriptPromise::RejectWithDOMException(
        script_state,
        DOMException::Create(kInvalidStateError,
                             "Cannot close a context that is being closed or "
                             "has already been closed."));
  }

  // Save the current sample rate for any subsequent decodeAudioData calls.
  SetClosedContextSampleRate(sampleRate());

  close_resolver_ = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = close_resolver_->Promise();

  // Stop the audio context. This will stop the destination node from pulling
  // audio anymore. And since we have disconnected the destination from the
  // audio graph, and thus has no references, the destination node can GCed if
  // JS has no references. uninitialize() will also resolve the Promise created
  // here.
  Uninitialize();

  return promise;
}

void AudioContext::DidClose() {
  // This is specific to AudioContexts. OfflineAudioContexts
  // are closed in their completion event.
  SetContextState(kClosed);

  DCHECK(g_hardware_context_count);
  --g_hardware_context_count;

  if (close_resolver_)
    close_resolver_->Resolve();
}

bool AudioContext::IsContextClosed() const {
  return close_resolver_ || BaseAudioContext::IsContextClosed();
}

void AudioContext::StopRendering() {
  DCHECK(IsMainThread());
  DCHECK(destination());

  if (ContextState() == kRunning) {
    destination()->GetAudioDestinationHandler().StopRendering();
    SetContextState(kSuspended);
    GetDeferredTaskHandler().ClearHandlersToBeDeleted();
  }
}

double AudioContext::baseLatency() const {
  return FramesPerBuffer() / static_cast<double>(sampleRate());
}

}  // namespace blink
