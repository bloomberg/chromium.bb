// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/encoder_base.h"

#include <string>

#include "base/atomic_sequence_num.h"
#include "base/bind.h"
#include "base/callback.h"
#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/core/v8/script_function.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_dom_exception.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_encoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_encode_options.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_encoder_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_encoder.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_state_helper.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_video_chunk.h"
#include "third_party/blink/renderer/modules/webcodecs/video_encoder.h"
#include "third_party/blink/renderer/platform/bindings/enumeration_base.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {
constexpr const char kCategory[] = "media";

base::AtomicSequenceNumber g_sequence_num_for_counters;
}  // namespace

// static
template <typename Traits>
const CodecTraceNames* EncoderBase<Traits>::GetTraceNames() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CodecTraceNames, trace_names,
                                  (Traits::GetName()));
  return &trace_names;
}

template <typename Traits>
EncoderBase<Traits>::EncoderBase(ScriptState* script_state,
                                 const InitType* init,
                                 ExceptionState& exception_state)
    : ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)),
      state_(V8CodecState::Enum::kUnconfigured),
      script_state_(script_state),
      trace_counter_id_(g_sequence_num_for_counters.GetNext()) {
  logger_ = std::make_unique<CodecLogger>(GetExecutionContext(),
                                          Thread::Current()->GetTaskRunner());

  media::MediaLog* log = logger_->log();

  log->SetProperty<media::MediaLogProperty::kFrameTitle>(
      std::string(Traits::GetNameForDevTools()));
  log->SetProperty<media::MediaLogProperty::kFrameUrl>(
      GetExecutionContext()->Url().GetString().Ascii());

  output_callback_ = init->output();
  if (init->hasError())
    error_callback_ = init->error();
}

template <typename Traits>
EncoderBase<Traits>::~EncoderBase() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::UmaHistogramSparse(
      String::Format("Blink.WebCodecs.%s.FinalStatus", Traits::GetName())
          .Ascii()
          .c_str(),
      static_cast<int>(logger_->status_code()));
}

template <typename Traits>
void EncoderBase<Traits>::configure(const ConfigType* config,
                                    ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ThrowIfCodecStateClosed(state_, "configure", exception_state))
    return;

  InternalConfigType* parsed_config = ParseConfig(config, exception_state);
  if (!parsed_config) {
    DCHECK(exception_state.HadException());
    return;
  }

  if (!VerifyCodecSupport(parsed_config, exception_state)) {
    DCHECK(exception_state.HadException());
    return;
  }

  MarkCodecActive();

  Request* request = MakeGarbageCollected<Request>();
  request->reset_count = reset_count_;
  if (media_encoder_ && active_config_ &&
      state_.AsEnum() == V8CodecState::Enum::kConfigured &&
      CanReconfigure(*active_config_, *parsed_config)) {
    request->type = Request::Type::kReconfigure;
  } else {
    state_ = V8CodecState(V8CodecState::Enum::kConfigured);
    request->type = Request::Type::kConfigure;
  }
  active_config_ = parsed_config;
  EnqueueRequest(request);
}

template <typename Traits>
void EncoderBase<Traits>::encode(InputType* input,
                                 const EncodeOptionsType* opts,
                                 ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ThrowIfCodecStateClosed(state_, "encode", exception_state))
    return;

  if (ThrowIfCodecStateUnconfigured(state_, "encode", exception_state))
    return;

  DCHECK(active_config_);

  // This will fail if |input| is already closed.
  auto* internal_input = input->clone(exception_state);

  if (!internal_input) {
    // Remove exceptions relating to cloning closed input.
    exception_state.ClearException();

    exception_state.ThrowDOMException(DOMExceptionCode::kOperationError,
                                      "Cannot encode closed input.");
    return;
  }

  MarkCodecActive();

  Request* request = MakeGarbageCollected<Request>();
  request->reset_count = reset_count_;
  request->type = Request::Type::kEncode;
  request->input = internal_input;
  request->encodeOpts = opts;
  ++requested_encodes_;
  EnqueueRequest(request);
}

template <typename Traits>
void EncoderBase<Traits>::close(ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ThrowIfCodecStateClosed(state_, "close", exception_state))
    return;

  state_ = V8CodecState(V8CodecState::Enum::kClosed);

  ResetInternal();
  media_encoder_.reset();
  output_callback_.Clear();
  error_callback_.Clear();
}

template <typename Traits>
ScriptPromise EncoderBase<Traits>::flush(ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ThrowIfCodecStateClosed(state_, "flush", exception_state))
    return ScriptPromise();

  if (ThrowIfCodecStateUnconfigured(state_, "flush", exception_state))
    return ScriptPromise();

  MarkCodecActive();

  Request* request = MakeGarbageCollected<Request>();
  request->resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  request->reset_count = reset_count_;
  request->type = Request::Type::kFlush;
  EnqueueRequest(request);
  return request->resolver->Promise();
}

template <typename Traits>
void EncoderBase<Traits>::reset(ExceptionState& exception_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (ThrowIfCodecStateClosed(state_, "reset", exception_state))
    return;

  TRACE_EVENT0(kCategory, GetTraceNames()->reset.c_str());

  MarkCodecActive();

  state_ = V8CodecState(V8CodecState::Enum::kUnconfigured);
  ResetInternal();
  media_encoder_.reset();
}

template <typename Traits>
void EncoderBase<Traits>::ResetInternal() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  reset_count_++;
  while (!requests_.empty()) {
    Request* pending_req = requests_.TakeFirst();
    DCHECK(pending_req);
    if (pending_req->resolver)
      pending_req->resolver.Release()->Resolve();
    if (pending_req->input)
      pending_req->input.Release()->close();
  }
  requested_encodes_ = 0;
  stall_request_processing_ = false;
}

template <typename Traits>
void EncoderBase<Traits>::HandleError(DOMException* ex) {
  if (state_.AsEnum() == V8CodecState::Enum::kClosed)
    return;

  TRACE_EVENT0(kCategory, GetTraceNames()->handle_error.c_str());

  // Save a temp before we clear the callback.
  V8WebCodecsErrorCallback* error_callback = error_callback_.Get();

  state_ = V8CodecState(V8CodecState::Enum::kClosed);

  ResetInternal();

  // Errors are permanent. Shut everything down.
  error_callback_.Clear();
  media_encoder_.reset();
  output_callback_.Clear();

  // Prevent further logging.
  logger_->Neuter();

  if (!script_state_->ContextIsValid() || !error_callback)
    return;

  ScriptState::Scope scope(script_state_);
  error_callback->InvokeAndReportException(nullptr, ex);
}

template <typename Traits>
void EncoderBase<Traits>::EnqueueRequest(Request* request) {
  requests_.push_back(request);
  ProcessRequests();
}

template <typename Traits>
void EncoderBase<Traits>::ProcessRequests() {
  while (!requests_.empty() && !stall_request_processing_) {
    TraceQueueSizes();

    Request* request = requests_.TakeFirst();
    DCHECK(request);
    switch (request->type) {
      case Request::Type::kConfigure:
        ProcessConfigure(request);
        break;
      case Request::Type::kReconfigure:
        ProcessReconfigure(request);
        break;
      case Request::Type::kEncode:
        ProcessEncode(request);
        break;
      case Request::Type::kFlush:
        ProcessFlush(request);
        break;
      default:
        NOTREACHED();
    }
  }

  TraceQueueSizes();
}

template <typename Traits>
void EncoderBase<Traits>::ProcessFlush(Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, V8CodecState::Enum::kConfigured);
  DCHECK(media_encoder_);
  DCHECK_EQ(request->type, Request::Type::kFlush);

  auto done_callback = [](EncoderBase<Traits>* self, Request* req,
                          media::Status status) {
    DCHECK(req);
    DCHECK(req->resolver);

    if (!self) {
      req->resolver.Release()->Reject();
      req->EndTracing(/*aborted=*/true);
      return;
    }

    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (self->reset_count_ != req->reset_count) {
      req->resolver.Release()->Reject();
      req->EndTracing(/*aborted=*/true);
      return;
    }
    if (status.is_ok()) {
      req->resolver.Release()->Resolve();
    } else {
      self->HandleError(
          self->logger_->MakeException("Flushing error.", status));
      req->resolver.Release()->Reject();
    }
    req->EndTracing();

    self->stall_request_processing_ = false;
    self->ProcessRequests();
  };

  request->StartTracing();

  stall_request_processing_ = true;
  media_encoder_->Flush(ConvertToBaseOnceCallback(
      CrossThreadBindOnce(done_callback, WrapCrossThreadWeakPersistent(this),
                          WrapCrossThreadPersistent(request))));
}

template <typename Traits>
void EncoderBase<Traits>::OnCodecReclaimed(DOMException* exception) {
  TRACE_EVENT0(kCategory, GetTraceNames()->reclaimed.c_str());
  HandleError(exception);
}

template <typename Traits>
void EncoderBase<Traits>::ContextDestroyed() {
  state_ = V8CodecState(V8CodecState::Enum::kClosed);
  logger_->Neuter();
  media_encoder_.reset();
}

template <typename Traits>
bool EncoderBase<Traits>::HasPendingActivity() const {
  return stall_request_processing_ || !requests_.empty();
}

template <typename Traits>
void EncoderBase<Traits>::TraceQueueSizes() const {
  TRACE_COUNTER_ID2(kCategory, GetTraceNames()->requests_counter.c_str(),
                    trace_counter_id_, "encodes", requested_encodes_, "other",
                    requests_.size() - requested_encodes_);
}

template <typename Traits>
void EncoderBase<Traits>::Trace(Visitor* visitor) const {
  visitor->Trace(active_config_);
  visitor->Trace(script_state_);
  visitor->Trace(output_callback_);
  visitor->Trace(error_callback_);
  visitor->Trace(requests_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  ReclaimableCodec::Trace(visitor);
}

template <typename Traits>
void EncoderBase<Traits>::Request::Trace(Visitor* visitor) const {
  visitor->Trace(input);
  visitor->Trace(encodeOpts);
  visitor->Trace(resolver);
}

template <typename Traits>
const char* EncoderBase<Traits>::Request::TraceNameFromType() {
  using RequestType = typename EncoderBase<Traits>::Request::Type;

  const CodecTraceNames* trace_names = EncoderBase<Traits>::GetTraceNames();

  switch (type) {
    case RequestType::kConfigure:
      return trace_names->configure.c_str();
    case RequestType::kEncode:
      return trace_names->encode.c_str();
    case RequestType::kFlush:
      return trace_names->flush.c_str();
    case RequestType::kReconfigure:
      return trace_names->reconfigure.c_str();
  }
  return "InvalidCodecTraceName";
}

template <typename Traits>
void EncoderBase<Traits>::Request::StartTracingVideoEncode(bool is_keyframe) {
#if DCHECK_IS_ON()
  DCHECK(!is_tracing);
  is_tracing = true;
#endif
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN1(kCategory, TraceNameFromType(), this,
                                    "key_frame", is_keyframe);
}

template <typename Traits>
void EncoderBase<Traits>::Request::StartTracing() {
#if DCHECK_IS_ON()
  DCHECK(!is_tracing);
  is_tracing = true;
#endif
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(kCategory, TraceNameFromType(), this);
}

template <typename Traits>
void EncoderBase<Traits>::Request::EndTracing(bool aborted) {
#if DCHECK_IS_ON()
  DCHECK(is_tracing);
  is_tracing = false;
#endif
  TRACE_EVENT_NESTABLE_ASYNC_END1(kCategory, TraceNameFromType(), this,
                                  "aborted", aborted);
}

template class EncoderBase<VideoEncoderTraits>;
template class EncoderBase<AudioEncoderTraits>;

}  // namespace blink
