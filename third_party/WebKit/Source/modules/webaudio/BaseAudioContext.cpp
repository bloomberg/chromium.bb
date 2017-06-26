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

#include "modules/webaudio/BaseAudioContext.h"

#include "bindings/core/v8/Dictionary.h"
#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/modules/v8/DecodeErrorCallback.h"
#include "bindings/modules/v8/DecodeSuccessCallback.h"
#include "core/dom/DOMException.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/TaskRunnerHelper.h"
#include "core/dom/UserGestureIndicator.h"
#include "core/frame/Settings.h"
#include "core/html/HTMLMediaElement.h"
#include "core/html/media/AutoplayPolicy.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ConsoleTypes.h"
#include "modules/mediastream/MediaStream.h"
#include "modules/webaudio/AnalyserNode.h"
#include "modules/webaudio/AudioBuffer.h"
#include "modules/webaudio/AudioBufferSourceNode.h"
#include "modules/webaudio/AudioContext.h"
#include "modules/webaudio/AudioListener.h"
#include "modules/webaudio/AudioNodeInput.h"
#include "modules/webaudio/AudioNodeOutput.h"
#include "modules/webaudio/BiquadFilterNode.h"
#include "modules/webaudio/ChannelMergerNode.h"
#include "modules/webaudio/ChannelSplitterNode.h"
#include "modules/webaudio/ConstantSourceNode.h"
#include "modules/webaudio/ConvolverNode.h"
#include "modules/webaudio/DelayNode.h"
#include "modules/webaudio/DynamicsCompressorNode.h"
#include "modules/webaudio/GainNode.h"
#include "modules/webaudio/IIRFilterNode.h"
#include "modules/webaudio/MediaElementAudioSourceNode.h"
#include "modules/webaudio/MediaStreamAudioDestinationNode.h"
#include "modules/webaudio/MediaStreamAudioSourceNode.h"
#include "modules/webaudio/OfflineAudioCompletionEvent.h"
#include "modules/webaudio/OfflineAudioContext.h"
#include "modules/webaudio/OfflineAudioDestinationNode.h"
#include "modules/webaudio/OscillatorNode.h"
#include "modules/webaudio/PannerNode.h"
#include "modules/webaudio/PeriodicWave.h"
#include "modules/webaudio/PeriodicWaveConstraints.h"
#include "modules/webaudio/ScriptProcessorNode.h"
#include "modules/webaudio/StereoPannerNode.h"
#include "modules/webaudio/WaveShaperNode.h"
#include "platform/CrossThreadFunctional.h"
#include "platform/Histogram.h"
#include "platform/audio/IIRFilter.h"
#include "platform/bindings/ScriptState.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/Platform.h"

namespace blink {

BaseAudioContext* BaseAudioContext::Create(
    Document& document,
    const AudioContextOptions& context_options,
    ExceptionState& exception_state) {
  return AudioContext::Create(document, context_options, exception_state);
}

// FIXME(dominicc): Devolve these constructors to AudioContext
// and OfflineAudioContext respectively.

// Constructor for rendering to the audio hardware.
BaseAudioContext::BaseAudioContext(Document* document)
    : SuspendableObject(document),
      destination_node_(nullptr),
      is_cleared_(false),
      is_resolving_resume_promises_(false),
      has_posted_cleanup_task_(false),
      user_gesture_required_(false),
      connection_count_(0),
      deferred_task_handler_(DeferredTaskHandler::Create()),
      context_state_(kSuspended),
      closed_context_sample_rate_(-1),
      periodic_wave_sine_(nullptr),
      periodic_wave_square_(nullptr),
      periodic_wave_sawtooth_(nullptr),
      periodic_wave_triangle_(nullptr),
      output_position_() {
  switch (GetAutoplayPolicy()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      break;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      if (document->GetFrame() &&
          document->GetFrame()->IsCrossOriginSubframe()) {
        autoplay_status_ = AutoplayStatus::kAutoplayStatusFailed;
        user_gesture_required_ = true;
      }
      break;
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      autoplay_status_ = AutoplayStatus::kAutoplayStatusFailed;
      user_gesture_required_ = true;
      break;
  }
}

// Constructor for offline (non-realtime) rendering.
BaseAudioContext::BaseAudioContext(Document* document,
                                   unsigned number_of_channels,
                                   size_t number_of_frames,
                                   float sample_rate)
    : SuspendableObject(document),
      destination_node_(nullptr),
      is_cleared_(false),
      is_resolving_resume_promises_(false),
      user_gesture_required_(false),
      connection_count_(0),
      deferred_task_handler_(DeferredTaskHandler::Create()),
      context_state_(kSuspended),
      closed_context_sample_rate_(-1),
      periodic_wave_sine_(nullptr),
      periodic_wave_square_(nullptr),
      periodic_wave_sawtooth_(nullptr),
      periodic_wave_triangle_(nullptr),
      output_position_() {}

BaseAudioContext::~BaseAudioContext() {
  GetDeferredTaskHandler().ContextWillBeDestroyed();
  // AudioNodes keep a reference to their context, so there should be no way to
  // be in the destructor if there are still AudioNodes around.
  DCHECK(!IsDestinationInitialized());
  DCHECK(!active_source_nodes_.size());
  DCHECK(!is_resolving_resume_promises_);
  DCHECK(!resume_resolvers_.size());
  DCHECK(!autoplay_status_.has_value());
}

void BaseAudioContext::Initialize() {
  if (IsDestinationInitialized())
    return;

  FFTFrame::Initialize();

  if (destination_node_) {
    destination_node_->Handler().Initialize();
    // The AudioParams in the listener need access to the destination node, so
    // only create the listener if the destination node exists.
    listener_ = AudioListener::Create(*this);
  }
}

void BaseAudioContext::Clear() {
  destination_node_.Clear();
  // The audio rendering thread is dead.  Nobody will schedule AudioHandler
  // deletion.  Let's do it ourselves.
  GetDeferredTaskHandler().ClearHandlersToBeDeleted();
  is_cleared_ = true;
}

void BaseAudioContext::Uninitialize() {
  DCHECK(IsMainThread());

  if (!IsDestinationInitialized())
    return;

  // This stops the audio thread and all audio rendering.
  if (destination_node_)
    destination_node_->Handler().Uninitialize();

  // Get rid of the sources which may still be playing.
  ReleaseActiveSourceNodes();

  // Reject any pending resolvers before we go away.
  RejectPendingResolvers();
  DidClose();

  DCHECK(listener_);
  listener_->WaitForHRTFDatabaseLoaderThreadCompletion();

  RecordAutoplayStatus();

  Clear();
}

void BaseAudioContext::ContextDestroyed(ExecutionContext*) {
  Uninitialize();
}

bool BaseAudioContext::HasPendingActivity() const {
  // There's no pending activity if the audio context has been cleared.
  return !is_cleared_;
}

AudioDestinationNode* BaseAudioContext::destination() const {
  // Cannot be called from the audio thread because this method touches objects
  // managed by Oilpan, and the audio thread is not managed by Oilpan.
  DCHECK(!IsAudioThread());
  return destination_node_;
}

void BaseAudioContext::ThrowExceptionForClosedState(
    ExceptionState& exception_state) {
  exception_state.ThrowDOMException(kInvalidStateError,
                                    "AudioContext has been closed.");
}

AudioBuffer* BaseAudioContext::createBuffer(unsigned number_of_channels,
                                            size_t number_of_frames,
                                            float sample_rate,
                                            ExceptionState& exception_state) {
  // It's ok to call createBuffer, even if the context is closed because the
  // AudioBuffer doesn't really "belong" to any particular context.

  AudioBuffer* buffer = AudioBuffer::Create(
      number_of_channels, number_of_frames, sample_rate, exception_state);

  if (buffer) {
    // Only record the data if the creation succeeded.
    DEFINE_STATIC_LOCAL(SparseHistogram, audio_buffer_channels_histogram,
                        ("WebAudio.AudioBuffer.NumberOfChannels"));

    // Arbitrarly limit the maximum length to 1 million frames (about 20 sec
    // at 48kHz).  The number of buckets is fairly arbitrary.
    DEFINE_STATIC_LOCAL(CustomCountHistogram, audio_buffer_length_histogram,
                        ("WebAudio.AudioBuffer.Length", 1, 1000000, 50));
    // The limits are the min and max AudioBuffer sample rates currently
    // supported.  We use explicit values here instead of
    // AudioUtilities::minAudioBufferSampleRate() and
    // AudioUtilities::maxAudioBufferSampleRate().  The number of buckets is
    // fairly arbitrary.
    DEFINE_STATIC_LOCAL(
        CustomCountHistogram, audio_buffer_sample_rate_histogram,
        ("WebAudio.AudioBuffer.SampleRate384kHz", 3000, 384000, 60));

    audio_buffer_channels_histogram.Sample(number_of_channels);
    audio_buffer_length_histogram.Count(number_of_frames);
    audio_buffer_sample_rate_histogram.Count(sample_rate);

    // Compute the ratio of the buffer rate and the context rate so we know
    // how often the buffer needs to be resampled to match the context.  For
    // the histogram, we multiply the ratio by 100 and round to the nearest
    // integer.  If the context is closed, don't record this because we
    // don't have a sample rate for closed context.
    if (!IsContextClosed()) {
      // The limits are choosen from 100*(3000/384000) = 0.78125 and
      // 100*(384000/3000) = 12800, where 3000 and 384000 are the current
      // min and max sample rates possible for an AudioBuffer.  The number
      // of buckets is fairly arbitrary.
      DEFINE_STATIC_LOCAL(
          CustomCountHistogram, audio_buffer_sample_rate_ratio_histogram,
          ("WebAudio.AudioBuffer.SampleRateRatio384kHz", 1, 12800, 50));
      float ratio = 100 * sample_rate / this->sampleRate();
      audio_buffer_sample_rate_ratio_histogram.Count(
          static_cast<int>(0.5 + ratio));
    }
  }

  return buffer;
}

ScriptPromise BaseAudioContext::decodeAudioData(
    ScriptState* script_state,
    DOMArrayBuffer* audio_data,
    ExceptionState& exception_state) {
  return decodeAudioData(script_state, audio_data, nullptr, nullptr,
                         exception_state);
}

ScriptPromise BaseAudioContext::decodeAudioData(
    ScriptState* script_state,
    DOMArrayBuffer* audio_data,
    DecodeSuccessCallback* success_callback,
    ExceptionState& exception_state) {
  return decodeAudioData(script_state, audio_data, success_callback, nullptr,
                         exception_state);
}

ScriptPromise BaseAudioContext::decodeAudioData(
    ScriptState* script_state,
    DOMArrayBuffer* audio_data,
    DecodeSuccessCallback* success_callback,
    DecodeErrorCallback* error_callback,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());
  DCHECK(audio_data);

  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  float rate = IsContextClosed() ? ClosedContextSampleRate() : sampleRate();

  DCHECK_GT(rate, 0);

  v8::Isolate* isolate = script_state->GetIsolate();
  WTF::ArrayBufferContents buffer_contents;
  // Detach the audio array buffer from the main thread and start
  // async decoding of the data.
  if (audio_data->IsNeuterable(isolate) &&
      audio_data->Transfer(isolate, buffer_contents)) {
    DOMArrayBuffer* audio = DOMArrayBuffer::Create(buffer_contents);

    decode_audio_resolvers_.insert(resolver);

    // Add a reference to success_callback and error_callback so that
    // they don't get collected prematurely before decodeAudioData
    // calls them.
    if (success_callback) {
      success_callbacks_.emplace_back(this, success_callback);
    }
    if (error_callback) {
      error_callbacks_.emplace_back(this, error_callback);
    }

    audio_decoder_.DecodeAsync(audio, rate, success_callback, error_callback,
                               resolver, this);
  } else {
    // If audioData is already detached (neutered) we need to reject the
    // promise with an error.
    DOMException* error = DOMException::Create(
        kDataCloneError, "Cannot decode detached ArrayBuffer");
    resolver->Reject(error);
    if (error_callback) {
      error_callback->call(this, error);
    }
  }

  return promise;
}

void BaseAudioContext::HandleDecodeAudioData(
    AudioBuffer* audio_buffer,
    ScriptPromiseResolver* resolver,
    DecodeSuccessCallback* success_callback,
    DecodeErrorCallback* error_callback) {
  DCHECK(IsMainThread());

  if (audio_buffer) {
    // Resolve promise successfully and run the success callback
    resolver->Resolve(audio_buffer);
    if (success_callback)
      success_callback->call(this, audio_buffer);
  } else {
    // Reject the promise and run the error callback
    DOMException* error =
        DOMException::Create(kEncodingError, "Unable to decode audio data");
    resolver->Reject(error);
    if (error_callback)
      error_callback->call(this, error);
  }

  // We've resolved the promise.  Remove it now.
  DCHECK(decode_audio_resolvers_.Contains(resolver));
  decode_audio_resolvers_.erase(resolver);

  // Find the success_callback and error_callback and remove it from
  // our list so that it can be collected.  Remove any matching entry
  // found (callback methods could be duplicated) which should be ok
  // because we're only using this to hold a reference to the
  // callback.

  if (success_callback) {
    size_t index = success_callbacks_.Find(success_callback);
    DCHECK_NE(index, kNotFound);
    success_callbacks_.erase(index);
  }

  if (error_callback) {
    size_t index = error_callbacks_.Find(error_callback);
    DCHECK_NE(index, kNotFound);
    error_callbacks_.erase(index);
  }
}

AudioBufferSourceNode* BaseAudioContext::createBufferSource(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  AudioBufferSourceNode* node =
      AudioBufferSourceNode::Create(*this, exception_state);

  // Do not add a reference to this source node now. The reference will be added
  // when start() is called.

  return node;
}

ConstantSourceNode* BaseAudioContext::createConstantSource(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ConstantSourceNode::Create(*this, exception_state);
}

MediaElementAudioSourceNode* BaseAudioContext::createMediaElementSource(
    HTMLMediaElement* media_element,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MediaElementAudioSourceNode::Create(*this, *media_element,
                                             exception_state);
}

MediaStreamAudioSourceNode* BaseAudioContext::createMediaStreamSource(
    MediaStream* media_stream,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return MediaStreamAudioSourceNode::Create(*this, *media_stream,
                                            exception_state);
}

MediaStreamAudioDestinationNode* BaseAudioContext::createMediaStreamDestination(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  // Set number of output channels to stereo by default.
  return MediaStreamAudioDestinationNode::Create(*this, 2, exception_state);
}

ScriptProcessorNode* BaseAudioContext::createScriptProcessor(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ScriptProcessorNode::Create(*this, exception_state);
}

ScriptProcessorNode* BaseAudioContext::createScriptProcessor(
    size_t buffer_size,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ScriptProcessorNode::Create(*this, buffer_size, exception_state);
}

ScriptProcessorNode* BaseAudioContext::createScriptProcessor(
    size_t buffer_size,
    size_t number_of_input_channels,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ScriptProcessorNode::Create(*this, buffer_size,
                                     number_of_input_channels, exception_state);
}

ScriptProcessorNode* BaseAudioContext::createScriptProcessor(
    size_t buffer_size,
    size_t number_of_input_channels,
    size_t number_of_output_channels,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ScriptProcessorNode::Create(
      *this, buffer_size, number_of_input_channels, number_of_output_channels,
      exception_state);
}

StereoPannerNode* BaseAudioContext::createStereoPanner(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return StereoPannerNode::Create(*this, exception_state);
}

BiquadFilterNode* BaseAudioContext::createBiquadFilter(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return BiquadFilterNode::Create(*this, exception_state);
}

WaveShaperNode* BaseAudioContext::createWaveShaper(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return WaveShaperNode::Create(*this, exception_state);
}

PannerNode* BaseAudioContext::createPanner(ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return PannerNode::Create(*this, exception_state);
}

ConvolverNode* BaseAudioContext::createConvolver(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ConvolverNode::Create(*this, exception_state);
}

DynamicsCompressorNode* BaseAudioContext::createDynamicsCompressor(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return DynamicsCompressorNode::Create(*this, exception_state);
}

AnalyserNode* BaseAudioContext::createAnalyser(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return AnalyserNode::Create(*this, exception_state);
}

GainNode* BaseAudioContext::createGain(ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return GainNode::Create(*this, exception_state);
}

DelayNode* BaseAudioContext::createDelay(ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return DelayNode::Create(*this, exception_state);
}

DelayNode* BaseAudioContext::createDelay(double max_delay_time,
                                         ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return DelayNode::Create(*this, max_delay_time, exception_state);
}

ChannelSplitterNode* BaseAudioContext::createChannelSplitter(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ChannelSplitterNode::Create(*this, exception_state);
}

ChannelSplitterNode* BaseAudioContext::createChannelSplitter(
    size_t number_of_outputs,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ChannelSplitterNode::Create(*this, number_of_outputs, exception_state);
}

ChannelMergerNode* BaseAudioContext::createChannelMerger(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ChannelMergerNode::Create(*this, exception_state);
}

ChannelMergerNode* BaseAudioContext::createChannelMerger(
    size_t number_of_inputs,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return ChannelMergerNode::Create(*this, number_of_inputs, exception_state);
}

OscillatorNode* BaseAudioContext::createOscillator(
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return OscillatorNode::Create(*this, "sine", nullptr, exception_state);
}

PeriodicWave* BaseAudioContext::createPeriodicWave(
    const Vector<float>& real,
    const Vector<float>& imag,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return PeriodicWave::Create(*this, real, imag, false, exception_state);
}

PeriodicWave* BaseAudioContext::createPeriodicWave(
    const Vector<float>& real,
    const Vector<float>& imag,
    const PeriodicWaveConstraints& options,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  bool disable = options.disableNormalization();

  return PeriodicWave::Create(*this, real, imag, disable, exception_state);
}

IIRFilterNode* BaseAudioContext::createIIRFilter(
    Vector<double> feedforward_coef,
    Vector<double> feedback_coef,
    ExceptionState& exception_state) {
  DCHECK(IsMainThread());

  return IIRFilterNode::Create(*this, feedforward_coef, feedback_coef,
                               exception_state);
}

PeriodicWave* BaseAudioContext::GetPeriodicWave(int type) {
  switch (type) {
    case OscillatorHandler::SINE:
      // Initialize the table if necessary
      if (!periodic_wave_sine_)
        periodic_wave_sine_ = PeriodicWave::CreateSine(sampleRate());
      return periodic_wave_sine_;
    case OscillatorHandler::SQUARE:
      // Initialize the table if necessary
      if (!periodic_wave_square_)
        periodic_wave_square_ = PeriodicWave::CreateSquare(sampleRate());
      return periodic_wave_square_;
    case OscillatorHandler::SAWTOOTH:
      // Initialize the table if necessary
      if (!periodic_wave_sawtooth_)
        periodic_wave_sawtooth_ = PeriodicWave::CreateSawtooth(sampleRate());
      return periodic_wave_sawtooth_;
    case OscillatorHandler::TRIANGLE:
      // Initialize the table if necessary
      if (!periodic_wave_triangle_)
        periodic_wave_triangle_ = PeriodicWave::CreateTriangle(sampleRate());
      return periodic_wave_triangle_;
    default:
      NOTREACHED();
      return nullptr;
  }
}

void BaseAudioContext::MaybeRecordStartAttempt() {
  if (!user_gesture_required_ || !AreAutoplayRequirementsFulfilled())
    return;

  DCHECK(!autoplay_status_.has_value() ||
         autoplay_status_ != AutoplayStatus::kAutoplayStatusSucceeded);
  autoplay_status_ = AutoplayStatus::kAutoplayStatusFailedWithStart;
}

String BaseAudioContext::state() const {
  // These strings had better match the strings for AudioContextState in
  // AudioContext.idl.
  switch (context_state_) {
    case kSuspended:
      return "suspended";
    case kRunning:
      return "running";
    case kClosed:
      return "closed";
  }
  NOTREACHED();
  return "";
}

void BaseAudioContext::SetContextState(AudioContextState new_state) {
  DCHECK(IsMainThread());

  // Validate the transitions.  The valid transitions are Suspended->Running,
  // Running->Suspended, and anything->Closed.
  switch (new_state) {
    case kSuspended:
      DCHECK_EQ(context_state_, kRunning);
      break;
    case kRunning:
      DCHECK_EQ(context_state_, kSuspended);
      break;
    case kClosed:
      DCHECK_NE(context_state_, kClosed);
      break;
  }

  if (new_state == context_state_) {
    // DCHECKs above failed; just return.
    return;
  }

  context_state_ = new_state;

  // Notify context that state changed
  if (GetExecutionContext())
    TaskRunnerHelper::Get(TaskType::kMediaElementEvent, GetExecutionContext())
        ->PostTask(BLINK_FROM_HERE,
                   WTF::Bind(&BaseAudioContext::NotifyStateChange,
                             WrapPersistent(this)));
}

void BaseAudioContext::NotifyStateChange() {
  DispatchEvent(Event::Create(EventTypeNames::statechange));
}

void BaseAudioContext::NotifySourceNodeFinishedProcessing(
    AudioHandler* handler) {
  DCHECK(IsAudioThread());
  MutexLocker lock(finished_source_handlers_mutex_);
  finished_source_handlers_.push_back(handler);
}

Document* BaseAudioContext::GetDocument() const {
  return ToDocument(GetExecutionContext());
}

AutoplayPolicy::Type BaseAudioContext::GetAutoplayPolicy() const {
  Document* document = GetDocument();
  DCHECK(document);
  return AutoplayPolicy::GetAutoplayPolicyForDocument(*document);
}

bool BaseAudioContext::AreAutoplayRequirementsFulfilled() const {
  switch (GetAutoplayPolicy()) {
    case AutoplayPolicy::Type::kNoUserGestureRequired:
      return true;
    case AutoplayPolicy::Type::kUserGestureRequired:
    case AutoplayPolicy::Type::kUserGestureRequiredForCrossOrigin:
      return UserGestureIndicator::ProcessingUserGesture();
    case AutoplayPolicy::Type::kDocumentUserActivationRequired:
      return AutoplayPolicy::IsDocumentAllowedToPlay(*GetDocument());
  }

  NOTREACHED();
  return false;
}

void BaseAudioContext::NotifySourceNodeStartedProcessing(AudioNode* node) {
  DCHECK(IsMainThread());
  AutoLocker locker(this);

  active_source_nodes_.push_back(node);
  node->Handler().MakeConnection();
}

void BaseAudioContext::ReleaseActiveSourceNodes() {
  DCHECK(IsMainThread());
  for (auto& source_node : active_source_nodes_)
    source_node->Handler().BreakConnection();

  active_source_nodes_.clear();
}

void BaseAudioContext::HandleStoppableSourceNodes() {
  DCHECK(IsAudioThread());
  DCHECK(IsGraphOwner());

  if (active_source_nodes_.size())
    ScheduleMainThreadCleanup();
}

void BaseAudioContext::HandlePreRenderTasks(
    const AudioIOPosition& output_position) {
  DCHECK(IsAudioThread());

  // At the beginning of every render quantum, try to update the internal
  // rendering graph state (from main thread changes).  It's OK if the tryLock()
  // fails, we'll just take slightly longer to pick up the changes.
  if (TryLock()) {
    GetDeferredTaskHandler().HandleDeferredTasks();

    ResolvePromisesForResume();

    // Check to see if source nodes can be stopped because the end time has
    // passed.
    HandleStoppableSourceNodes();

    // Update the dirty state of the listener.
    listener()->UpdateState();

    // Update output timestamp.
    output_position_ = output_position;

    unlock();
  }
}

void BaseAudioContext::HandlePostRenderTasks() {
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

void BaseAudioContext::PerformCleanupOnMainThread() {
  DCHECK(IsMainThread());
  AutoLocker locker(this);

  if (is_resolving_resume_promises_) {
    for (auto& resolver : resume_resolvers_) {
      if (context_state_ == kClosed) {
        resolver->Reject(DOMException::Create(
            kInvalidStateError,
            "Cannot resume a context that has been closed"));
      } else {
        SetContextState(kRunning);
        resolver->Resolve();
      }
    }
    resume_resolvers_.clear();
    is_resolving_resume_promises_ = false;
  }

  if (active_source_nodes_.size()) {
    // Find AudioBufferSourceNodes to see if we can stop playing them.
    for (AudioNode* node : active_source_nodes_) {
      if (node->Handler().GetNodeType() ==
          AudioHandler::kNodeTypeAudioBufferSource) {
        AudioBufferSourceNode* source_node =
            static_cast<AudioBufferSourceNode*>(node);
        source_node->GetAudioBufferSourceHandler().HandleStoppableSourceNode();
      }
    }

    Vector<AudioHandler*> finished_handlers;
    {
      MutexLocker lock(finished_source_handlers_mutex_);
      finished_source_handlers_.swap(finished_handlers);
    }
    // Break the connection and release active nodes that have finished
    // playing.
    unsigned remove_count = 0;
    Vector<bool> removables;
    removables.resize(active_source_nodes_.size());
    for (AudioHandler* handler : finished_handlers) {
      for (unsigned i = 0; i < active_source_nodes_.size(); ++i) {
        if (handler == &active_source_nodes_[i]->Handler()) {
          handler->BreakConnection();
          removables[i] = true;
          remove_count++;
          break;
        }
      }
    }

    // Copy over the surviving active nodes.
    HeapVector<Member<AudioNode>> actives;
    actives.ReserveInitialCapacity(active_source_nodes_.size() - remove_count);
    for (unsigned i = 0; i < removables.size(); ++i) {
      if (!removables[i])
        actives.push_back(active_source_nodes_[i]);
    }
    active_source_nodes_.swap(actives);
  }
  has_posted_cleanup_task_ = false;
}

void BaseAudioContext::ScheduleMainThreadCleanup() {
  if (has_posted_cleanup_task_)
    return;
  Platform::Current()->MainThread()->GetWebTaskRunner()->PostTask(
      BLINK_FROM_HERE,
      CrossThreadBind(&BaseAudioContext::PerformCleanupOnMainThread,
                      WrapCrossThreadPersistent(this)));
  has_posted_cleanup_task_ = true;
}

void BaseAudioContext::ResolvePromisesForResume() {
  // This runs inside the BaseAudioContext's lock when handling pre-render
  // tasks.
  DCHECK(IsAudioThread());
  DCHECK(IsGraphOwner());

  // Resolve any pending promises created by resume(). Only do this if we
  // haven't already started resolving these promises. This gets called very
  // often and it takes some time to resolve the promises in the main thread.
  if (!is_resolving_resume_promises_ && resume_resolvers_.size() > 0) {
    is_resolving_resume_promises_ = true;
    ScheduleMainThreadCleanup();
  }
}

void BaseAudioContext::RejectPendingDecodeAudioDataResolvers() {
  // Now reject any pending decodeAudioData resolvers
  for (auto& resolver : decode_audio_resolvers_)
    resolver->Reject(DOMException::Create(kInvalidStateError,
                                          "Audio context is going away"));
  decode_audio_resolvers_.clear();
}

void BaseAudioContext::MaybeUnlockUserGesture() {
  if (!user_gesture_required_ || !AreAutoplayRequirementsFulfilled())
    return;

  DCHECK(!autoplay_status_.has_value() ||
         autoplay_status_ != AutoplayStatus::kAutoplayStatusSucceeded);

  user_gesture_required_ = false;
  autoplay_status_ = AutoplayStatus::kAutoplayStatusSucceeded;
}

bool BaseAudioContext::IsAllowedToStart() const {
  if (!user_gesture_required_)
    return true;

  ToDocument(GetExecutionContext())
      ->AddConsoleMessage(ConsoleMessage::Create(
          kJSMessageSource, kWarningMessageLevel,
          "An AudioContext in a cross origin iframe must be created or resumed "
          "from a user gesture to enable audio output."));
  return false;
}

AudioIOPosition BaseAudioContext::OutputPosition() {
  DCHECK(IsMainThread());
  AutoLocker locker(this);
  return output_position_;
}

void BaseAudioContext::RejectPendingResolvers() {
  DCHECK(IsMainThread());

  // Audio context is closing down so reject any resume promises that are still
  // pending.

  for (auto& resolver : resume_resolvers_) {
    resolver->Reject(DOMException::Create(kInvalidStateError,
                                          "Audio context is going away"));
  }
  resume_resolvers_.clear();
  is_resolving_resume_promises_ = false;

  RejectPendingDecodeAudioDataResolvers();
}

void BaseAudioContext::RecordAutoplayStatus() {
  if (!autoplay_status_.has_value())
    return;

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
}

const AtomicString& BaseAudioContext::InterfaceName() const {
  return EventTargetNames::AudioContext;
}

ExecutionContext* BaseAudioContext::GetExecutionContext() const {
  return SuspendableObject::GetExecutionContext();
}

void BaseAudioContext::StartRendering() {
  // This is called for both online and offline contexts.  The caller
  // must set the context state appropriately. In particular, resuming
  // a context should wait until the context has actually resumed to
  // set the state.
  DCHECK(IsMainThread());
  DCHECK(destination_node_);
  DCHECK(IsAllowedToStart());

  if (context_state_ == kSuspended) {
    destination()->GetAudioDestinationHandler().StartRendering();
  }
}

DEFINE_TRACE(BaseAudioContext) {
  visitor->Trace(destination_node_);
  visitor->Trace(listener_);
  visitor->Trace(active_source_nodes_);
  visitor->Trace(resume_resolvers_);
  visitor->Trace(success_callbacks_);
  visitor->Trace(error_callbacks_);
  visitor->Trace(decode_audio_resolvers_);

  visitor->Trace(periodic_wave_sine_);
  visitor->Trace(periodic_wave_square_);
  visitor->Trace(periodic_wave_sawtooth_);
  visitor->Trace(periodic_wave_triangle_);
  EventTargetWithInlineData::Trace(visitor);
  SuspendableObject::Trace(visitor);
}

DEFINE_TRACE_WRAPPERS(BaseAudioContext) {
  // Inform V8's GC that we have references to these objects so they
  // don't get collected until we're done with them.
  for (auto callback : success_callbacks_) {
    visitor->TraceWrappers(callback);
  }
  for (auto callback : error_callbacks_) {
    visitor->TraceWrappers(callback);
  }
}

SecurityOrigin* BaseAudioContext::GetSecurityOrigin() const {
  if (GetExecutionContext())
    return GetExecutionContext()->GetSecurityOrigin();

  return nullptr;
}

}  // namespace blink
