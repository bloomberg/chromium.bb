// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webaudio/audio_context.h"

#include "build/build_config.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "third_party/blink/public/common/browser_interface_broker_proxy.h"
#include "third_party/blink/public/platform/web_audio_latency_hint.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/frame/local_dom_window.h"
#include "third_party/blink/renderer/core/frame/local_frame.h"
#include "third_party/blink/renderer/core/html/media/html_media_element.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/core/probe/core_probes.h"
#include "third_party/blink/renderer/core/timing/dom_window_performance.h"
#include "third_party/blink/renderer/core/timing/window_performance.h"
#include "third_party/blink/renderer/modules/mediastream/media_stream.h"
#include "third_party/blink/renderer/modules/webaudio/audio_context_options.h"
#include "third_party/blink/renderer/modules/webaudio/audio_listener.h"
#include "third_party/blink/renderer/modules/webaudio/audio_timestamp.h"
#include "third_party/blink/renderer/modules/webaudio/media_element_audio_source_node.h"
#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_destination_node.h"
#include "third_party/blink/renderer/modules/webaudio/media_stream_audio_source_node.h"
#include "third_party/blink/renderer/modules/webaudio/realtime_audio_destination_node.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/audio/vector_math.h"
#include "third_party/blink/renderer/platform/bindings/exception_messages.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/heap/heap.h"
#include "third_party/blink/renderer/platform/instrumentation/histogram.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

#if DEBUG_AUDIONODE_REFERENCES
#include <stdio.h>
#endif

namespace blink {

// Number of AudioContexts still alive.  It's incremented when an
// AudioContext is created and decremented when the context is closed.
static unsigned g_hardware_context_count = 0;

// A context ID that is incremented for each context that is created.
// This initializes the internal id for the context.
static unsigned g_context_id = 0;

AudioContext* AudioContext::Create(Document& document,
                                   const AudioContextOptions* context_options,
                                   ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  if (document.IsDetached()) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        "Cannot create AudioContext on a detached document.");
    return nullptr;
  }

  document.CountUseOnlyInCrossOriginIframe(
      WebFeature::kAudioContextCrossOriginIframe);

  WebAudioLatencyHint latency_hint(WebAudioLatencyHint::kCategoryInteractive);
  if (context_options->latencyHint().IsAudioContextLatencyCategory()) {
    latency_hint = WebAudioLatencyHint(
        context_options->latencyHint().GetAsAudioContextLatencyCategory());
  } else if (context_options->latencyHint().IsDouble()) {
    // This should be the requested output latency in seconds, without taking
    // into account double buffering (same as baseLatency).
    latency_hint =
        WebAudioLatencyHint(context_options->latencyHint().GetAsDouble());
  }

  base::Optional<float> sample_rate;
  if (context_options->hasSampleRate()) {
    sample_rate = context_options->sampleRate();
  }

  // Validate options before trying to construct the actual context.
  if (sample_rate.has_value() &&
      !audio_utilities::IsValidAudioBufferSampleRate(sample_rate.value())) {
    exception_state.ThrowDOMException(
        DOMExceptionCode::kNotSupportedError,
        ExceptionMessages::IndexOutsideRange(
            "hardware sample rate", sample_rate.value(),
            audio_utilities::MinAudioBufferSampleRate(),
            ExceptionMessages::kInclusiveBound,
            audio_utilities::MaxAudioBufferSampleRate(),
            ExceptionMessages::kInclusiveBound));
    return nullptr;
  }

  AudioContext* audio_context =
      MakeGarbageCollected<AudioContext>(document, latency_hint, sample_rate);
  ++g_hardware_context_count;
  audio_context->UpdateStateIfNeeded();

  // This starts the audio thread. The destination node's
  // provideInput() method will now be called repeatedly to render
  // audio.  Each time provideInput() is called, a portion of the
  // audio stream is rendered. Let's call this time period a "render
  // quantum". NOTE: for now AudioContext does not need an explicit
  // startRendering() call from JavaScript.  We may want to consider
  // requiring it for symmetry with OfflineAudioContext.
  audio_context->MaybeAllowAutoplayWithUnlockType(
      AutoplayUnlockType::kContextConstructor);
  if (audio_context->IsAllowedToStart()) {
    audio_context->StartRendering();
    audio_context->SetContextState(kRunning);
  }
#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: AudioContext::AudioContext(): %u #%u\n",
          audio_context, audio_context->context_id_, g_hardware_context_count);
#endif

  DEFINE_STATIC_LOCAL(SparseHistogram, max_channel_count_histogram,
                      ("WebAudio.AudioContext.MaxChannelsAvailable"));
  max_channel_count_histogram.Sample(
      audio_context->destination()->maxChannelCount());

  probe::DidCreateAudioContext(&document);

  return audio_context;
}

AudioContext::AudioContext(Document& document,
                           const WebAudioLatencyHint& latency_hint,
                           base::Optional<float> sample_rate)
    : BaseAudioContext(&document, kRealtimeContext),
      context_id_(g_context_id++) {
  destination_node_ =
      RealtimeAudioDestinationNode::Create(this, latency_hint, sample_rate);

  switch (GetAutoplayPolicy()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
      // kUserGestureRequire policy only applies to cross-origin iframes for Web
      // Audio.
      if (document.GetFrame() &&
          document.GetFrame()->IsCrossOriginSubframe()) {
        autoplay_status_ = AutoplayStatus::kAutoplayStatusFailed;
        user_gesture_required_ = true;
      }
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      autoplay_status_ = AutoplayStatus::kAutoplayStatusFailed;
      user_gesture_required_ = true;
      break;
  }

  Initialize();
}

void AudioContext::Uninitialize() {
  DCHECK(IsMainThread());
  DCHECK_NE(g_hardware_context_count, 0u);
  --g_hardware_context_count;

  DidClose();
  RecordAutoplayMetrics();
  BaseAudioContext::Uninitialize();
}

AudioContext::~AudioContext() {
  // TODO(crbug.com/945379) Disable this DCHECK for now.  It's not terrible if
  // the autoplay metrics aren't recorded in some odd situations.  haraken@ said
  // that we shouldn't get here without also calling |Uninitialize()|, but it
  // can happen.  Until that is fixed, disable this DCHECK.

  // DCHECK(!autoplay_status_.has_value());
#if DEBUG_AUDIONODE_REFERENCES
  fprintf(stderr, "[%16p]: AudioContext::~AudioContext(): %u\n", this,
          context_id_);
#endif
}

void AudioContext::Trace(blink::Visitor* visitor) {
  visitor->Trace(close_resolver_);
  BaseAudioContext::Trace(visitor);
}

ScriptPromise AudioContext::suspendContext(ScriptState* script_state) {
  DCHECK(IsMainThread());

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  if (ContextState() == kClosed) {
    resolver->Reject(MakeGarbageCollected<DOMException>(
        DOMExceptionCode::kInvalidStateError,
        "Cannot suspend a context that has been closed"));
  } else {
    suspended_by_user_ = true;

    // Stop rendering now.
    if (destination())
      StopRendering();

    // Since we don't have any way of knowing when the hardware actually stops,
    // we'll just resolve the promise now.
    resolver->Resolve();

    // Probe reports the suspension only when the promise is resolved.
    probe::DidSuspendAudioContext(GetDocument());
  }

  return promise;
}

ScriptPromise AudioContext::resumeContext(ScriptState* script_state) {
  DCHECK(IsMainThread());

  if (IsContextClosed()) {
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidAccessError,
                          "cannot resume a closed AudioContext"));
  }

  auto* resolver = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = resolver->Promise();

  // If we're already running, just resolve; nothing else needs to be done.
  if (ContextState() == kRunning) {
    resolver->Resolve();
    return promise;
  }

  suspended_by_user_ = false;

  // Restart the destination node to pull on the audio graph.
  if (destination()) {
    MaybeAllowAutoplayWithUnlockType(AutoplayUnlockType::kContextResume);
    if (IsAllowedToStart()) {
      // Do not set the state to running here.  We wait for the
      // destination to start to set the state.
      StartRendering();

      // Probe reports only when the user gesture allows the audio rendering.
      probe::DidResumeAudioContext(GetDocument());
    }
  }

  // Save the resolver which will get resolved when the destination node starts
  // pulling on the graph again.
  {
    GraphAutoLocker locker(this);
    resume_resolvers_.push_back(resolver);
  }

  return promise;
}

AudioTimestamp* AudioContext::getOutputTimestamp(
    ScriptState* script_state) const {
  AudioTimestamp* result = AudioTimestamp::Create();

  DCHECK(IsMainThread());
  LocalDOMWindow* window = LocalDOMWindow::From(script_state);
  if (!window)
    return result;

  if (!destination()) {
    result->setContextTime(0.0);
    result->setPerformanceTime(0.0);
    return result;
  }

  WindowPerformance* performance = DOMWindowPerformance::performance(*window);
  DCHECK(performance);

  AudioIOPosition position = OutputPosition();

  // The timestamp of what is currently being played (contextTime) cannot be
  // later than what is being rendered. (currentTime)
  if (position.position > currentTime()) {
    position.position = currentTime();
  }

  double performance_time = performance->MonotonicTimeToDOMHighResTimeStamp(
      base::TimeTicks() + base::TimeDelta::FromSecondsD(position.timestamp));
  if (performance_time < 0.0)
    performance_time = 0.0;

  result->setContextTime(position.position);
  result->setPerformanceTime(performance_time);
  return result;
}

ScriptPromise AudioContext::closeContext(ScriptState* script_state) {
  if (IsContextClosed()) {
    // We've already closed the context previously, but it hasn't yet been
    // resolved, so just create a new promise and reject it.
    return ScriptPromise::RejectWithDOMException(
        script_state, MakeGarbageCollected<DOMException>(
                          DOMExceptionCode::kInvalidStateError,
                          "Cannot close a context that is being closed or "
                          "has already been closed."));
  }

  close_resolver_ = MakeGarbageCollected<ScriptPromiseResolver>(script_state);
  ScriptPromise promise = close_resolver_->Promise();

  // Stops the rendering, but it doesn't release the resources here.
  StopRendering();

  // The promise from closing context resolves immediately after this function.
  DidClose();

  probe::DidCloseAudioContext(GetDocument());

  return promise;
}

void AudioContext::DidClose() {
  SetContextState(kClosed);

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
  DCHECK(IsMainThread());
  DCHECK(destination());

  // TODO(hongchan): Due to the incompatible constructor between
  // AudioDestinationNode and RealtimeAudioDestinationNode, casting directly
  // from |destination()| is impossible. This is a temporary workaround until
  // the refactoring is completed.
  RealtimeAudioDestinationHandler& destination_handler =
      static_cast<RealtimeAudioDestinationHandler&>(
          destination()->GetAudioDestinationHandler());
  return destination_handler.GetFramesPerBuffer() /
         static_cast<double>(sampleRate());
}

MediaElementAudioSourceNode* AudioContext::createMediaElementSource(
    HTMLMediaElement* media_element,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MediaElementAudioSourceNode::Create(*this, *media_element,
                                             exception_state);
}

MediaStreamAudioSourceNode* AudioContext::createMediaStreamSource(
    MediaStream* media_stream,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MediaStreamAudioSourceNode::Create(*this, *media_stream,
                                            exception_state);
}

MediaStreamAudioDestinationNode* AudioContext::createMediaStreamDestination(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // Set number of output channels to stereo by default.
  return MediaStreamAudioDestinationNode::Create(*this, 2, exception_state);
}

void AudioContext::NotifySourceNodeStart() {
  DCHECK(IsMainThread());

  source_node_started_ = true;
  if (!user_gesture_required_)
    return;

  MaybeAllowAutoplayWithUnlockType(AutoplayUnlockType::kSourceNodeStart);

  if (ContextState() == AudioContextState::kSuspended && !suspended_by_user_ &&
      IsAllowedToStart()) {
    StartRendering();
    SetContextState(kRunning);
  }
}

AutoplayPolicy::Type AudioContext::GetAutoplayPolicy() const {
  Document* document = GetDocument();
  DCHECK(document);

  auto autoplay_policy =
      AutoplayPolicy::GetAutoplayPolicyForDocument(*document);

  if (autoplay_policy ==
          AutoplayPolicy::Type::kDocumentUserActivationRequired &&
      RuntimeEnabledFeatures::AutoplayIgnoresWebAudioEnabled()) {
// When ignored, the policy is different on Android compared to Desktop.
#if defined(OS_ANDROID)
    return AutoplayPolicy::Type::kUserGestureRequired;
#else
    // Force no user gesture required on desktop.
    return AutoplayPolicy::Type::kNoUserGestureRequired;
#endif
  }

  return autoplay_policy;
}

bool AudioContext::AreAutoplayRequirementsFulfilled() const {
  DCHECK(GetDocument());

  switch (GetAutoplayPolicy()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      return true;
    case AutoplayPolicy::Type::kUserGestureRequired:
      return LocalFrame::HasTransientUserActivation(GetDocument()->GetFrame());
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      return AutoplayPolicy::IsDocumentAllowedToPlay(*GetDocument());
  }

  NOTREACHED();
  return false;
}

void AudioContext::MaybeAllowAutoplayWithUnlockType(AutoplayUnlockType type) {
  if (!user_gesture_required_ || !AreAutoplayRequirementsFulfilled())
    return;

  DCHECK(!autoplay_status_.has_value() ||
         autoplay_status_ != AutoplayStatus::kAutoplayStatusSucceeded);

  user_gesture_required_ = false;
  autoplay_status_ = AutoplayStatus::kAutoplayStatusSucceeded;

  DCHECK(!autoplay_unlock_type_.has_value());
  autoplay_unlock_type_ = type;
}

bool AudioContext::IsAllowedToStart() const {
  if (!user_gesture_required_)
    return true;

  Document* document = To<Document>(GetExecutionContext());
  DCHECK(document);

  switch (GetAutoplayPolicy()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      NOTREACHED();
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
      DCHECK(document->GetFrame());
      DCHECK(document->GetFrame()->IsCrossOriginSubframe());
      document->AddConsoleMessage(ConsoleMessage::Create(
          mojom::ConsoleMessageSource::kOther,
          mojom::ConsoleMessageLevel::kWarning,
          "The AudioContext was not allowed to start. It must be resumed (or "
          "created) from a user gesture event handler. https://goo.gl/7K7WLu"));
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      document->AddConsoleMessage(ConsoleMessage::Create(
          mojom::ConsoleMessageSource::kOther,
          mojom::ConsoleMessageLevel::kWarning,
          "The AudioContext was not allowed to start. It must be resumed (or "
          "created) after a user gesture on the page. https://goo.gl/7K7WLu"));
      break;
  }

  return false;
}

void AudioContext::RecordAutoplayMetrics() {
  if (!autoplay_status_.has_value() || !GetDocument())
    return;

  ukm::UkmRecorder* ukm_recorder = GetDocument()->UkmRecorder();
  DCHECK(ukm_recorder);
  ukm::builders::Media_Autoplay_AudioContext(GetDocument()->UkmSourceID())
      .SetStatus(autoplay_status_.value())
      .SetUnlockType(autoplay_unlock_type_
                         ? static_cast<int>(autoplay_unlock_type_.value())
                         : -1)
      .SetSourceNodeStarted(source_node_started_)
      .Record(ukm_recorder);

  // Record autoplay_status_ value.
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, autoplay_histogram,
      ("WebAudio.Autoplay", AutoplayStatus::kAutoplayStatusCount));
  DEFINE_STATIC_LOCAL(
      EnumerationHistogram, cross_origin_autoplay_histogram,
      ("WebAudio.Autoplay.CrossOrigin", AutoplayStatus::kAutoplayStatusCount));

  autoplay_histogram.Count(autoplay_status_.value());

  if (GetDocument()->GetFrame() &&
      GetDocument()->GetFrame()->IsCrossOriginSubframe()) {
    cross_origin_autoplay_histogram.Count(autoplay_status_.value());
  }

  autoplay_status_.reset();

  // Record autoplay_unlock_type_ value.
  if (autoplay_unlock_type_.has_value()) {
    DEFINE_STATIC_LOCAL(EnumerationHistogram, autoplay_unlock_type_histogram,
                        ("WebAudio.Autoplay.UnlockType",
                         static_cast<int>(AutoplayUnlockType::kCount)));

    autoplay_unlock_type_histogram.Count(
        static_cast<int>(autoplay_unlock_type_.value()));

    autoplay_unlock_type_.reset();
  }
}

void AudioContext::ContextDestroyed(ExecutionContext*) {
  Uninitialize();
}

bool AudioContext::HasPendingActivity() const {
  // There's activity only if the context is running.  Suspended contexts are
  // basically idle with nothing going on.
  return (ContextState() == kRunning) && BaseAudioContext::HasPendingActivity();
}

bool AudioContext::HandlePreRenderTasks(const AudioIOPosition* output_position,
                                        const AudioCallbackMetric* metric) {
  DCHECK(IsAudioThread());

  // At the beginning of every render quantum, try to update the internal
  // rendering graph state (from main thread changes).  It's OK if the tryLock()
  // fails, we'll just take slightly longer to pick up the changes.
  if (TryLock()) {
    GetDeferredTaskHandler().HandleDeferredTasks();

    ResolvePromisesForUnpause();

    // Check to see if source nodes can be stopped because the end time has
    // passed.
    HandleStoppableSourceNodes();

    // Update the dirty state of the listener.
    listener()->UpdateState();

    // Update output timestamp and metric.
    output_position_ = *output_position;
    callback_metric_ = *metric;

    unlock();
  }

  // Realtime context ignores the return result, but return true, just in case.
  return true;
}

void AudioContext::NotifyAudibleAudioStarted() {
  DCHECK(IsMainThread());

  EnsureAudioContextManagerService();
  if (audio_context_manager_)
    audio_context_manager_->AudioContextAudiblePlaybackStarted(context_id_);
}

void AudioContext::HandlePostRenderTasks() {
  DCHECK(IsAudioThread());

  // Must use a tryLock() here too.  Don't worry, the lock will very rarely be
  // contended and this method is called frequently.  The worst that can happen
  // is that there will be some nodes which will take slightly longer than usual
  // to be deleted or removed from the render graph (in which case they'll
  // render silence).
  if (TryLock()) {
    // Take care of AudioNode tasks where the tryLock() failed previously.
    GetDeferredTaskHandler().BreakConnections();

    GetDeferredTaskHandler().HandleDeferredTasks();
    GetDeferredTaskHandler().RequestToDeleteHandlersOnMainThread();

    unlock();
  }
}

static bool IsAudible(const AudioBus* rendered_data) {
  // Compute the energy in each channel and sum up the energy in each channel
  // for the total energy.
  float energy = 0;

  uint32_t data_size = rendered_data->length();
  for (uint32_t k = 0; k < rendered_data->NumberOfChannels(); ++k) {
    const float* data = rendered_data->Channel(k)->Data();
    float channel_energy;
    vector_math::Vsvesq(data, 1, &channel_energy, data_size);
    energy += channel_energy;
  }

  return energy > 0;
}

void AudioContext::HandleAudibility(AudioBus* destination_bus) {
  DCHECK(IsAudioThread());

  // Detect silence (or not) for MEI
  bool is_audible = IsAudible(destination_bus);

  if (is_audible) {
    ++total_audible_renders_;
  }

  if (was_audible_ != is_audible) {
    // Audibility changed in this render, so report the change.
    was_audible_ = is_audible;
    if (is_audible) {
      PostCrossThreadTask(
          *task_runner_, FROM_HERE,
          CrossThreadBindOnce(&AudioContext::NotifyAudibleAudioStarted,
                              WrapCrossThreadPersistent(this)));
    } else {
      PostCrossThreadTask(
          *task_runner_, FROM_HERE,
          CrossThreadBindOnce(&AudioContext::NotifyAudibleAudioStopped,
                              WrapCrossThreadPersistent(this)));
    }
  }
}

void AudioContext::ResolvePromisesForUnpause() {
  // This runs inside the BaseAudioContext's lock when handling pre-render
  // tasks.
  DCHECK(IsAudioThread());
  AssertGraphOwner();

  // Resolve any pending promises created by resume(). Only do this if we
  // haven't already started resolving these promises. This gets called very
  // often and it takes some time to resolve the promises in the main thread.
  if (!is_resolving_resume_promises_ && resume_resolvers_.size() > 0) {
    is_resolving_resume_promises_ = true;
    ScheduleMainThreadCleanup();
  }
}

AudioIOPosition AudioContext::OutputPosition() const {
  DCHECK(IsMainThread());
  GraphAutoLocker locker(this);
  return output_position_;
}

void AudioContext::NotifyAudibleAudioStopped() {
  DCHECK(IsMainThread());

  EnsureAudioContextManagerService();
  if (audio_context_manager_)
    audio_context_manager_->AudioContextAudiblePlaybackStopped(context_id_);
}

void AudioContext::EnsureAudioContextManagerService() {
  if (audio_context_manager_ || !GetDocument())
    return;

  GetDocument()->GetFrame()->GetBrowserInterfaceBroker().GetInterface(
      mojo::GenericPendingReceiver(
          audio_context_manager_.BindNewPipeAndPassReceiver()));

  audio_context_manager_.set_disconnect_handler(
      WTF::Bind(&AudioContext::OnAudioContextManagerServiceConnectionError,
                WrapWeakPersistent(this)));
}

void AudioContext::OnAudioContextManagerServiceConnectionError() {
  audio_context_manager_.reset();
}

AudioCallbackMetric AudioContext::GetCallbackMetric() const {
  // Return a copy under the graph lock because returning a reference would
  // allow seeing the audio thread changing the struct values. This method
  // gets called once per second and the size of the struct is small, so
  // creating a copy is acceptable here.
  GraphAutoLocker locker(this);
  return callback_metric_;
}

}  // namespace blink
