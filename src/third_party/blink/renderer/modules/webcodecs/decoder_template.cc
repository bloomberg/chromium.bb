// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/decoder_template.h"

#include <limits>
#include <utility>
#include <vector>

#include "base/atomic_sequence_num.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "media/base/decode_status.h"
#include "media/media_buildflags.h"
#include "media/video/gpu_video_accelerator_factories.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_output_callback.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_chunk.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_video_chunk.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_video_decoder_init.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/modules/modules_export.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_data.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_decoder.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_config_eval.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_state_helper.h"
#include "third_party/blink/renderer/modules/webcodecs/gpu_factories_retriever.h"
#include "third_party/blink/renderer/modules/webcodecs/video_decoder.h"
#include "third_party/blink/renderer/modules/webcodecs/video_frame.h"
#include "third_party/blink/renderer/platform/bindings/exception_code.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/heap/persistent.h"
#include "third_party/blink/renderer/platform/scheduler/public/post_cross_thread_task.h"
#include "third_party/blink/renderer/platform/scheduler/public/thread.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"
#include "third_party/blink/renderer/platform/wtf/functional.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace blink {

namespace {
constexpr const char kCategory[] = "media";

base::AtomicSequenceNumber g_sequence_num_for_counters;
}  // namespace

// static
template <typename Traits>
const CodecTraceNames* DecoderTemplate<Traits>::GetTraceNames() {
  DEFINE_THREAD_SAFE_STATIC_LOCAL(CodecTraceNames, trace_names,
                                  (Traits::GetName()));
  return &trace_names;
}

template <typename Traits>
DecoderTemplate<Traits>::DecoderTemplate(ScriptState* script_state,
                                         const InitType* init,
                                         ExceptionState& exception_state)
    : ReclaimableCodec(ExecutionContext::From(script_state)),
      ExecutionContextLifecycleObserver(ExecutionContext::From(script_state)),
      script_state_(script_state),
      state_(V8CodecState::Enum::kUnconfigured),
      trace_counter_id_(g_sequence_num_for_counters.GetNext()) {
  DVLOG(1) << __func__;
  DCHECK(init->hasOutput());
  DCHECK(init->hasError());

  ExecutionContext* context = GetExecutionContext();
  DCHECK(context);

  main_thread_task_runner_ =
      context->GetTaskRunner(TaskType::kInternalMediaRealTime);

  logger_ = std::make_unique<CodecLogger>(context, main_thread_task_runner_);

  logger_->log()->SetProperty<media::MediaLogProperty::kFrameUrl>(
      context->Url().GetString().Ascii());

  output_cb_ = init->output();
  error_cb_ = init->error();
}

template <typename Traits>
DecoderTemplate<Traits>::~DecoderTemplate() {
  DVLOG(1) << __func__;
  base::UmaHistogramSparse(
      String::Format("Blink.WebCodecs.%s.FinalStatus", Traits::GetName())
          .Ascii()
          .c_str(),
      static_cast<int>(logger_->status_code()));
}

template <typename Traits>
int32_t DecoderTemplate<Traits>::decodeQueueSize() {
  return num_pending_decodes_;
}

template <typename Traits>
bool DecoderTemplate<Traits>::IsClosed() {
  return state_ == V8CodecState::Enum::kClosed;
}

template <typename Traits>
HardwarePreference DecoderTemplate<Traits>::GetHardwarePreference(
    const ConfigType&) {
  return HardwarePreference::kNoPreference;
}

template <typename Traits>
bool DecoderTemplate<Traits>::GetLowDelayPreference(const ConfigType&) {
  return false;
}

template <typename Traits>
void DecoderTemplate<Traits>::SetHardwarePreference(HardwarePreference) {}

template <typename Traits>
void DecoderTemplate<Traits>::configure(const ConfigType* config,
                                        ExceptionState& exception_state) {
  DVLOG(1) << __func__;
  if (ThrowIfCodecStateClosed(state_, "decode", exception_state))
    return;

  auto media_config = std::make_unique<MediaConfigType>();
  String console_message;

  CodecConfigEval eval =
      MakeMediaConfig(*config, media_config.get(), &console_message);
  switch (eval) {
    case CodecConfigEval::kInvalid:
      exception_state.ThrowTypeError(console_message);
      return;
    case CodecConfigEval::kUnsupported:
      exception_state.ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                        console_message);
      return;
    case CodecConfigEval::kSupported:
      // Good, lets proceed.
      break;
  }

  MarkCodecActive();

  state_ = V8CodecState(V8CodecState::Enum::kConfigured);
  require_key_frame_ = true;

  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kConfigure;
  request->media_config = std::move(media_config);
  request->reset_generation = reset_generation_;
  request->hw_pref = GetHardwarePreference(*config);
  request->low_delay = GetLowDelayPreference(*config);
  requests_.push_back(request);
  ProcessRequests();
}

template <typename Traits>
void DecoderTemplate<Traits>::decode(const InputType* chunk,
                                     ExceptionState& exception_state) {
  DVLOG(3) << __func__;
  if (ThrowIfCodecStateClosed(state_, "decode", exception_state))
    return;

  if (ThrowIfCodecStateUnconfigured(state_, "decode", exception_state))
    return;

  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kDecode;
  request->reset_generation = reset_generation_;
  auto status_or_buffer =
      MakeDecoderBuffer(*chunk, /*verify_key_frame=*/require_key_frame_);

  if (status_or_buffer.has_value()) {
    request->decoder_buffer = std::move(status_or_buffer).value();
    require_key_frame_ = false;
  } else {
    request->status = std::move(status_or_buffer).error();
    if (request->status.code() == media::StatusCode::kKeyFrameRequired) {
      exception_state.ThrowDOMException(
          DOMExceptionCode::kDataError,
          "A key frame is required after configure() or flush().");
      return;
    }
  }

  MarkCodecActive();

  requests_.push_back(request);
  ++num_pending_decodes_;
  ProcessRequests();
}

template <typename Traits>
ScriptPromise DecoderTemplate<Traits>::flush(ExceptionState& exception_state) {
  DVLOG(3) << __func__;
  if (ThrowIfCodecStateClosed(state_, "flush", exception_state))
    return ScriptPromise();

  if (ThrowIfCodecStateUnconfigured(state_, "flush", exception_state))
    return ScriptPromise();

  MarkCodecActive();

  require_key_frame_ = true;

  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kFlush;
  ScriptPromiseResolver* resolver =
      MakeGarbageCollected<ScriptPromiseResolver>(script_state_);
  request->resolver = resolver;
  request->reset_generation = reset_generation_;
  requests_.push_back(request);
  ProcessRequests();
  return resolver->Promise();
}

template <typename Traits>
void DecoderTemplate<Traits>::reset(ExceptionState& exception_state) {
  DVLOG(3) << __func__;
  if (ThrowIfCodecStateClosed(state_, "reset", exception_state))
    return;

  MarkCodecActive();

  ResetAlgorithm();
}

template <typename Traits>
void DecoderTemplate<Traits>::close(ExceptionState& exception_state) {
  DVLOG(3) << __func__;
  if (ThrowIfCodecStateClosed(state_, "close", exception_state))
    return;

  Shutdown();
}

template <typename Traits>
void DecoderTemplate<Traits>::ProcessRequests() {
  DVLOG(3) << __func__;
  DCHECK(!IsClosed());
  while (!pending_request_ && !requests_.IsEmpty()) {
    Request* request = requests_.front();

    // Skip processing for requests that are canceled by a recent reset().
    if (request->reset_generation != reset_generation_) {
      if (request->resolver) {
        // TODO(crbug.com/1229313): We might be in a Shutdown(), in which case
        // this may actually be due to an error or close().
        request->resolver.Release()->Reject(MakeGarbageCollected<DOMException>(
            DOMExceptionCode::kAbortError, "Aborted due to reset()"));
      }
      requests_.pop_front();
      continue;
    }

    TraceQueueSizes();

    DCHECK_EQ(request->reset_generation, reset_generation_);
    switch (request->type) {
      case Request::Type::kConfigure:
        if (!ProcessConfigureRequest(request))
          return;
        break;
      case Request::Type::kDecode:
        if (!ProcessDecodeRequest(request))
          return;
        break;
      case Request::Type::kFlush:
        if (!ProcessFlushRequest(request))
          return;
        break;
      case Request::Type::kReset:
        if (!ProcessResetRequest(request))
          return;
        break;
    }
    requests_.pop_front();
  }

  TraceQueueSizes();
}

template <typename Traits>
bool DecoderTemplate<Traits>::ProcessConfigureRequest(Request* request) {
  DVLOG(3) << __func__;
  DCHECK(!IsClosed());
  DCHECK(!pending_request_);
  DCHECK_EQ(request->type, Request::Type::kConfigure);
  DCHECK(request->media_config);

  if (decoder_ &&
      pending_decodes_.size() + 1 >
          static_cast<size_t>(Traits::GetMaxDecodeRequests(*decoder_))) {
    // Try again after OnDecodeDone().
    return false;
  }

  // TODO(sandersd): Record this configuration as pending but don't apply it
  // until there is a decode request.
  pending_request_ = request;
  pending_request_->StartTracing();

  if (gpu_factories_.has_value()) {
    ContinueConfigureWithGpuFactories(request, gpu_factories_.value());
  } else if (Traits::kNeedsGpuFactories) {
    RetrieveGpuFactoriesWithKnownDecoderSupport(CrossThreadBindOnce(
        &DecoderTemplate<Traits>::ContinueConfigureWithGpuFactories,
        WrapCrossThreadPersistent(this), WrapCrossThreadPersistent(request)));
  } else {
    ContinueConfigureWithGpuFactories(request, nullptr);
  }
  return true;
}

template <typename Traits>
void DecoderTemplate<Traits>::ContinueConfigureWithGpuFactories(
    Request* request,
    media::GpuVideoAcceleratorFactories* gpu_factories) {
  DCHECK(request);
  DCHECK_EQ(request->type, Request::Type::kConfigure);

  gpu_factories_ = gpu_factories;

  if (request->reset_generation != reset_generation_)
    return;
  if (!decoder_) {
    decoder_ = Traits::CreateDecoder(*ExecutionContext::From(script_state_),
                                     gpu_factories_.value(), logger_->log());
    if (!decoder_) {
      Shutdown(
          logger_->MakeException("Internal error: Could not create decoder.",
                                 media::StatusCode::kDecoderCreationFailed));
      return;
    }

    SetHardwarePreference(request->hw_pref.value());
    // Processing continues in OnInitializeDone().
    // Note: OnInitializeDone() must not call ProcessRequests() reentrantly,
    // which can happen if InitializeDecoder() calls it synchronously.
    initializing_sync_ = true;
    Traits::InitializeDecoder(
        *decoder_, request->low_delay.value(), *request->media_config,
        WTF::Bind(&DecoderTemplate::OnInitializeDone, WrapWeakPersistent(this)),
        WTF::BindRepeating(&DecoderTemplate::OnOutput, WrapWeakPersistent(this),
                           reset_generation_));
    initializing_sync_ = false;
    return;
  }

  // Processing continues in OnFlushDone().
  decoder_->Decode(
      media::DecoderBuffer::CreateEOSBuffer(),
      WTF::Bind(&DecoderTemplate::OnFlushDone, WrapWeakPersistent(this)));
}

template <typename Traits>
bool DecoderTemplate<Traits>::ProcessDecodeRequest(Request* request) {
  DVLOG(3) << __func__;
  DCHECK_EQ(state_, V8CodecState::Enum::kConfigured);
  DCHECK(!pending_request_);
  DCHECK_EQ(request->type, Request::Type::kDecode);
  DCHECK_GT(num_pending_decodes_, 0);

  if (!decoder_) {
    Shutdown(logger_->MakeException(
        "Decoding error: no decoder found.",
        media::StatusCode::kDecoderInitializeNeverCompleted));
    return false;
  }

  if (pending_decodes_.size() + 1 >
      static_cast<size_t>(Traits::GetMaxDecodeRequests(*decoder_))) {
    // Try again after OnDecodeDone().
    return false;
  }

  // The request may be invalid, if so report that now.
  if (!request->decoder_buffer || request->decoder_buffer->data_size() == 0) {
    if (request->status.is_ok()) {
      Shutdown(logger_->MakeException("Null or empty decoder buffer.",
                                      media::StatusCode::kDecoderFailedDecode));
    } else {
      Shutdown(logger_->MakeException("Decoder error.", request->status));
    }

    return false;
  }

  // Submit for decoding.
  //
  // |pending_decode_id_| must not be 0 nor max because it HashMap reserves
  // these values for "emtpy" and "deleted".
  while (++pending_decode_id_ == 0 ||
         pending_decode_id_ == std::numeric_limits<uint32_t>::max() ||
         pending_decodes_.Contains(pending_decode_id_))
    ;
  pending_decodes_.Set(pending_decode_id_, request);
  --num_pending_decodes_;

  if (media::ScopedDecodeTrace::IsEnabled()) {
    request->decode_trace = std::make_unique<media::ScopedDecodeTrace>(
        GetTraceNames()->decode.c_str(), *request->decoder_buffer);
  }

  decoder_->Decode(std::move(request->decoder_buffer),
                   WTF::Bind(&DecoderTemplate::OnDecodeDone,
                             WrapWeakPersistent(this), pending_decode_id_));
  return true;
}

template <typename Traits>
bool DecoderTemplate<Traits>::ProcessFlushRequest(Request* request) {
  DVLOG(3) << __func__;
  DCHECK(!IsClosed());
  DCHECK(!pending_request_);
  DCHECK_EQ(request->type, Request::Type::kFlush);
  DCHECK_EQ(state_, V8CodecState::Enum::kConfigured);

  // flush() can only be called when state = "configured", in which case we
  // should always have a decoder.
  DCHECK(decoder_);

  if (pending_decodes_.size() + 1 >
      static_cast<size_t>(Traits::GetMaxDecodeRequests(*decoder_))) {
    // Try again after OnDecodeDone().
    return false;
  }

  // Processing continues in OnFlushDone().
  pending_request_ = request;
  pending_request_->StartTracing();

  decoder_->Decode(
      media::DecoderBuffer::CreateEOSBuffer(),
      WTF::Bind(&DecoderTemplate::OnFlushDone, WrapWeakPersistent(this)));
  return true;
}

template <typename Traits>
bool DecoderTemplate<Traits>::ProcessResetRequest(Request* request) {
  DVLOG(3) << __func__;
  DCHECK(!IsClosed());
  DCHECK(!pending_request_);
  DCHECK_EQ(request->type, Request::Type::kReset);
  DCHECK_GT(reset_generation_, 0u);

  // Signal [[codec implementation]] to cease producing output for the previous
  // configuration.
  if (decoder_) {
    pending_request_ = request;
    pending_request_->StartTracing();

    // Processing continues in OnResetDone().
    decoder_->Reset(
        WTF::Bind(&DecoderTemplate::OnResetDone, WrapWeakPersistent(this)));
  }

  return true;
}

template <typename Traits>
void DecoderTemplate<Traits>::Shutdown(DOMException* exception) {
  DVLOG(3) << __func__;
  if (IsClosed())
    return;

  TRACE_EVENT1(kCategory, GetTraceNames()->shutdown.c_str(), "has_exception",
               !!exception);

  // Abort pending work (otherwise it will never complete)
  if (pending_request_) {
    if (pending_request_->resolver) {
      pending_request_->resolver.Release()->Reject(
          MakeGarbageCollected<DOMException>(
              DOMExceptionCode::kAbortError,
              exception ? "Aborted due to error" : "Aborted due to close()"));
    }

    pending_request_.Release()->EndTracing(/*shutting_down*/ true);
  }

  // Abort all upcoming work.
  ResetAlgorithm();
  PauseCodecReclamation();

  // Store the error callback so that we can use it after clearing state.
  V8WebCodecsErrorCallback* error_cb = error_cb_.Get();

  // Prevent any new public API calls during teardown.
  // This should make it safe to call into JS synchronously.
  state_ = V8CodecState(V8CodecState::Enum::kClosed);

  // Prevent any late callbacks running.
  output_cb_.Release();
  error_cb_.Release();

  // Prevent any further logging from being reported.
  logger_->Neuter();

  // Clear decoding and JS-visible queue state. Use DeleteSoon() to avoid
  // deleting decoder_ when its callback (e.g. OnDecodeDone()) may be below us
  // in the stack.
  main_thread_task_runner_->DeleteSoon(FROM_HERE, std::move(decoder_));

  if (pending_request_) {
    // This request was added as part of calling ResetAlgorithm above. However,
    // OnResetDone() will never execute, since we are now in a kClosed state,
    // and |decoder_| has been reset.
    DCHECK_EQ(pending_request_->type, Request::Type::kReset);
    pending_request_.Release()->EndTracing(/*shutting_down*/ true);
  }

  bool trace_enabled = false;
  TRACE_EVENT_CATEGORY_GROUP_ENABLED(kCategory, &trace_enabled);
  if (trace_enabled) {
    for (auto& pending_decode : pending_decodes_)
      pending_decode.value->decode_trace.reset();
  }

  pending_decodes_.clear();
  num_pending_decodes_ = 0;

  // Fire the error callback if necessary.
  if (exception)
    error_cb->InvokeAndReportException(nullptr, exception);
}

template <typename Traits>
void DecoderTemplate<Traits>::ResetAlgorithm() {
  if (state_ == V8CodecState::Enum::kUnconfigured)
    return;

  state_ = V8CodecState(V8CodecState::Enum::kUnconfigured);

  // Increment reset counter to cause older pending requests to be rejected. See
  // ProcessRequests().
  reset_generation_++;

  // Any previous pending decode will be filtered by ProcessRequests(). Reset
  // the count immediately to report the correct value in decodeQueueSize().
  num_pending_decodes_ = 0;

  // Since configure is always required after reset we can drop any cached
  // configuration.
  active_config_.reset();

  Request* request = MakeGarbageCollected<Request>();
  request->type = Request::Type::kReset;
  request->reset_generation = reset_generation_;
  requests_.push_back(request);
  ProcessRequests();
}

template <typename Traits>
void DecoderTemplate<Traits>::OnFlushDone(media::Status status) {
  DVLOG(3) << __func__;
  if (IsClosed())
    return;

  DCHECK(pending_request_);
  DCHECK(pending_request_->type == Request::Type::kConfigure ||
         pending_request_->type == Request::Type::kFlush);

  if (!status.is_ok()) {
    Shutdown(logger_->MakeException("Error during flush.", status));
    return;
  }

  // If reset() has been called during the Flush(), we can skip reinitialization
  // since the client is required to do so manually.
  const bool is_flush = pending_request_->type == Request::Type::kFlush;
  if (is_flush && pending_request_->reset_generation != reset_generation_) {
    pending_request_->EndTracing();

    // We must reject the Promise for consistency in the behavior of reset().
    // It's also possible that we already dropped outputs, so the flush() may be
    // incomplete despite finishing successfully.
    pending_request_.Release()->resolver.Release()->Reject(
        MakeGarbageCollected<DOMException>(DOMExceptionCode::kAbortError,
                                           "Aborted due to reset()"));
    ProcessRequests();
    return;
  }

  if (!is_flush)
    SetHardwarePreference(pending_request_->hw_pref.value());

  // Processing continues in OnInitializeDone().
  Traits::InitializeDecoder(
      *decoder_, is_flush ? low_delay_ : pending_request_->low_delay.value(),
      is_flush ? *active_config_ : *pending_request_->media_config,
      WTF::Bind(&DecoderTemplate::OnInitializeDone, WrapWeakPersistent(this)),
      WTF::BindRepeating(&DecoderTemplate::OnOutput, WrapWeakPersistent(this),
                         reset_generation_));
}

template <typename Traits>
void DecoderTemplate<Traits>::OnInitializeDone(media::Status status) {
  DVLOG(3) << __func__;
  if (IsClosed())
    return;

  DCHECK(pending_request_);
  DCHECK(pending_request_->type == Request::Type::kConfigure ||
         pending_request_->type == Request::Type::kFlush);

  const bool is_flush = pending_request_->type == Request::Type::kFlush;
  if (!status.is_ok()) {
    std::string error_message;
    if (is_flush) {
      error_message = "Error during initialize after flush.";
    } else if (status.code() == media::StatusCode::kDecoderUnsupportedConfig) {
      error_message =
          "Unsupported configuration. Check isConfigSupported() prior to "
          "calling configure().";
    } else {
      error_message = "Decoder initialization error.";
    }
    Shutdown(logger_->MakeException(error_message, status));
    return;
  }

  if (is_flush) {
    pending_request_->resolver.Release()->Resolve();
  } else {
    Traits::UpdateDecoderLog(*decoder_, *pending_request_->media_config,
                             logger_->log());

    low_delay_ = pending_request_->low_delay.value();
    active_config_ = std::move(pending_request_->media_config);
  }

  pending_request_.Release()->EndTracing();

  if (!initializing_sync_)
    ProcessRequests();
  else
    DCHECK(!is_flush);
}

template <typename Traits>
void DecoderTemplate<Traits>::OnDecodeDone(uint32_t id, media::Status status) {
  DVLOG(3) << __func__;
  if (IsClosed())
    return;

  auto it = pending_decodes_.find(id);
  if (it != pending_decodes_.end()) {
    if (it->value->decode_trace)
      it->value->decode_trace->EndTrace(status);
    pending_decodes_.erase(it);
  }

  if (!status.is_ok() && status.code() != media::StatusCode::kAborted) {
    Shutdown(logger_->MakeException("Decoding error.", status));
    return;
  }

  ProcessRequests();
}

template <typename Traits>
void DecoderTemplate<Traits>::OnResetDone() {
  DVLOG(3) << __func__;
  if (IsClosed())
    return;

  DCHECK(pending_request_);
  DCHECK_EQ(pending_request_->type, Request::Type::kReset);

  pending_request_.Release()->EndTracing();
  ProcessRequests();
}

template <typename Traits>
void DecoderTemplate<Traits>::OnOutput(uint32_t reset_generation,
                                       scoped_refptr<MediaOutputType> output) {
  DVLOG(3) << __func__;

  // Suppress outputs belonging to an earlier reset_generation.
  if (reset_generation != reset_generation_)
    return;

  if (state_.AsEnum() != V8CodecState::Enum::kConfigured)
    return;

  auto* context = GetExecutionContext();
  if (!context)
    return;

  auto output_or_error = Traits::MakeOutput(std::move(output), context);

  if (output_or_error.has_error()) {
    Shutdown(logger_->MakeException("Error creating output from decoded data",
                                    std::move(output_or_error).error()));
    return;
  }

  OutputType* blink_output = std::move(output_or_error).value();

  TRACE_EVENT_BEGIN1(kCategory, GetTraceNames()->output.c_str(), "timestamp",
                     blink_output->timestamp());

  output_cb_->InvokeAndReportException(nullptr, blink_output);

  TRACE_EVENT_END0(kCategory, GetTraceNames()->output.c_str());

  MarkCodecActive();
}

template <typename Traits>
void DecoderTemplate<Traits>::TraceQueueSizes() const {
  TRACE_COUNTER_ID2(kCategory, GetTraceNames()->requests_counter.c_str(),
                    trace_counter_id_, "decodes", num_pending_decodes_, "other",
                    requests_.size() - num_pending_decodes_);
}

template <typename Traits>
void DecoderTemplate<Traits>::ContextDestroyed() {
  // Deallocate resources and supress late callbacks from media thread.
  Shutdown();
}

template <typename Traits>
void DecoderTemplate<Traits>::Trace(Visitor* visitor) const {
  visitor->Trace(script_state_);
  visitor->Trace(output_cb_);
  visitor->Trace(error_cb_);
  visitor->Trace(requests_);
  visitor->Trace(pending_request_);
  visitor->Trace(pending_decodes_);
  ScriptWrappable::Trace(visitor);
  ExecutionContextLifecycleObserver::Trace(visitor);
  ReclaimableCodec::Trace(visitor);
}

template <typename Traits>
void DecoderTemplate<Traits>::OnCodecReclaimed(DOMException* exception) {
  TRACE_EVENT0(kCategory, GetTraceNames()->reclaimed.c_str());

  if (state_.AsEnum() == V8CodecState::Enum::kUnconfigured) {
    decoder_.reset();

    // This codec isn't holding on to any resources, and doesn't need to be
    // reclaimed.
    PauseCodecReclamation();
    return;
  }

  DCHECK_EQ(state_.AsEnum(), V8CodecState::Enum::kConfigured);
  Shutdown(exception);
}

template <typename Traits>
bool DecoderTemplate<Traits>::HasPendingActivity() const {
  return pending_request_ || !requests_.IsEmpty();
}

template <typename Traits>
void DecoderTemplate<Traits>::Request::Trace(Visitor* visitor) const {
  visitor->Trace(resolver);
}

template <typename Traits>
const char* DecoderTemplate<Traits>::Request::TraceNameFromType() {
  using RequestType = typename DecoderTemplate<Traits>::Request::Type;

  const CodecTraceNames* trace_names = DecoderTemplate<Traits>::GetTraceNames();

  switch (type) {
    case RequestType::kConfigure:
      return trace_names->configure.c_str();
    case RequestType::kDecode:
      return trace_names->decode.c_str();
    case RequestType::kFlush:
      return trace_names->flush.c_str();
    case RequestType::kReset:
      return trace_names->reset.c_str();
  }
  return "InvalidCodecTraceName";
}

template <typename Traits>
void DecoderTemplate<Traits>::Request::StartTracing() {
#if DCHECK_IS_ON()
  DCHECK(!is_tracing);
  is_tracing = true;
#endif
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0(kCategory, TraceNameFromType(), this);
}

template <typename Traits>
void DecoderTemplate<Traits>::Request::EndTracing(bool shutting_down) {
#if DCHECK_IS_ON()
  DCHECK(is_tracing);
  is_tracing = false;
#endif
  TRACE_EVENT_NESTABLE_ASYNC_END1(kCategory, TraceNameFromType(), this,
                                  "completed", !shutting_down);
}

template class DecoderTemplate<AudioDecoderTraits>;
template class DecoderTemplate<VideoDecoderTraits>;

}  // namespace blink
