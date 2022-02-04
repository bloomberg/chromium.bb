// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_encoder.h"

#include <cinttypes>
#include <limits>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "media/audio/audio_features.h"
#include "media/audio/audio_opus_encoder.h"
#include "media/base/audio_parameters.h"
#include "media/base/limits.h"
#include "media/base/mime_util.h"
#include "media/base/offloading_audio_encoder.h"
#include "media/mojo/clients/mojo_audio_encoder.h"
#include "media/mojo/mojom/audio_encoder.mojom-blink.h"
#include "media/mojo/mojom/interface_factory.mojom.h"
#include "third_party/blink/public/common/thread_safe_browser_interface_broker_proxy.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/renderer/bindings/core/v8/to_v8_traits.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_typedefs.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_data_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_encoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_encoder_support.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_chunk_metadata.h"
#include "third_party/blink/renderer/modules/webaudio/audio_buffer.h"
#include "third_party/blink/renderer/modules/webcodecs/allow_shared_buffer_source_util.h"
#include "third_party/blink/renderer/modules/webcodecs/encoded_audio_chunk.h"
#include "third_party/blink/renderer/platform/bindings/exception_state.h"
#include "third_party/blink/renderer/platform/wtf/cross_thread_functional.h"

namespace blink {

namespace {

constexpr const char kCategory[] = "media";

AudioEncoderTraits::ParsedConfig* ParseConfigStatic(
    const AudioEncoderConfig* config,
    ExceptionState& exception_state) {
  if (!config) {
    exception_state.ThrowTypeError("No config provided");
    return nullptr;
  }
  auto* result = MakeGarbageCollected<AudioEncoderTraits::ParsedConfig>();

  result->options.codec = media::AudioCodec::kUnknown;
  bool is_codec_ambiguous = true;
  bool parse_succeeded = ParseAudioCodecString(
      "", config->codec().Utf8(), &is_codec_ambiguous, &result->options.codec);

  if (!parse_succeeded || is_codec_ambiguous) {
    exception_state.ThrowTypeError("Unknown codec.");
    return nullptr;
  }

  result->options.channels = config->numberOfChannels();
  if (result->options.channels < 1 ||
      result->options.channels > media::limits::kMaxChannels) {
    exception_state.ThrowTypeError(String::Format(
        "Invalid channel number; expected range from %d to %d, received %d.", 1,
        media::limits::kMaxChannels, result->options.channels));
    return nullptr;
  }

  result->options.sample_rate = config->sampleRate();
  if (result->options.sample_rate < media::limits::kMinSampleRate ||
      result->options.sample_rate > media::limits::kMaxSampleRate) {
    exception_state.ThrowTypeError(String::Format(
        "Invalid sample rate; expected range from %d to %d, received %d.",
        media::limits::kMinSampleRate, media::limits::kMaxSampleRate,
        result->options.sample_rate));
    return nullptr;
  }

  result->codec_string = config->codec();
  if (config->hasBitrate()) {
    if (config->bitrate() > std::numeric_limits<int>::max()) {
      exception_state.ThrowTypeError(String::Format(
          "Bitrate is too large; expected at most %d, received %" PRIu64,
          std::numeric_limits<int>::max(), config->bitrate()));
      return nullptr;
    }
    result->options.bitrate = static_cast<int>(config->bitrate());
  }

  return result;
}

bool VerifyCodecSupportStatic(AudioEncoderTraits::ParsedConfig* config,
                              ExceptionState* exception_state) {
  switch (config->options.codec) {
    case media::AudioCodec::kOpus: {
      if (config->options.channels > 2) {
        // Our Opus implementation only supports up to 2 channels
        if (exception_state) {
          exception_state->ThrowDOMException(
              DOMExceptionCode::kNotSupportedError,
              String::Format("Too many channels for Opus encoder; "
                             "expected at most 2, received %d.",
                             config->options.channels));
        }
        return false;
      }
      if (config->options.bitrate.has_value() &&
          config->options.bitrate.value() <
              media::AudioOpusEncoder::kMinBitrate) {
        if (exception_state) {
          exception_state->ThrowDOMException(
              DOMExceptionCode::kNotSupportedError,
              String::Format(
                  "Opus bitrate is too low; expected at least %d, received %d.",
                  media::AudioOpusEncoder::kMinBitrate,
                  config->options.bitrate.value()));
        }
        return false;
      }
      return true;
    }
    default:
      if (exception_state) {
        exception_state->ThrowDOMException(DOMExceptionCode::kNotSupportedError,
                                           "Unsupported codec type.");
      }
      return false;
  }
}

AudioEncoderConfig* CopyConfig(const AudioEncoderConfig& config) {
  auto* result = AudioEncoderConfig::Create();
  result->setCodec(config.codec());
  result->setSampleRate(config.sampleRate());
  result->setNumberOfChannels(config.numberOfChannels());
  if (config.hasBitrate())
    result->setBitrate(config.bitrate());
  return result;
}

std::unique_ptr<media::AudioEncoder> CreateSoftwareAudioEncoder(
    media::AudioCodec codec) {
  if (codec != media::AudioCodec::kOpus)
    return nullptr;
  auto software_encoder = std::make_unique<media::AudioOpusEncoder>();
  return std::make_unique<media::OffloadingAudioEncoder>(
      std::move(software_encoder));
}

std::unique_ptr<media::AudioEncoder> CreatePlatformAudioEncoder(
    media::AudioCodec codec) {
  if (!base::FeatureList::IsEnabled(features::kPlatformAudioEncoder))
    return nullptr;
  if (codec != media::AudioCodec::kOpus)
    return nullptr;

  mojo::PendingRemote<media::mojom::InterfaceFactory> pending_interface_factory;
  mojo::Remote<media::mojom::InterfaceFactory> interface_factory;
  Platform::Current()->GetBrowserInterfaceBroker()->GetInterface(
      pending_interface_factory.InitWithNewPipeAndPassReceiver());
  interface_factory.Bind(std::move(pending_interface_factory));

  mojo::PendingRemote<media::mojom::AudioEncoder> encoder_remote;
  interface_factory->CreateAudioEncoder(
      encoder_remote.InitWithNewPipeAndPassReceiver());
  return std::make_unique<media::MojoAudioEncoder>(std::move(encoder_remote));
}

}  // namespace

// static
const char* AudioEncoderTraits::GetName() {
  return "AudioEncoder";
}

AudioEncoder* AudioEncoder::Create(ScriptState* script_state,
                                   const AudioEncoderInit* init,
                                   ExceptionState& exception_state) {
  auto* result =
      MakeGarbageCollected<AudioEncoder>(script_state, init, exception_state);
  return exception_state.HadException() ? nullptr : result;
}

AudioEncoder::AudioEncoder(ScriptState* script_state,
                           const AudioEncoderInit* init,
                           ExceptionState& exception_state)
    : Base(script_state, init, exception_state) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kWebCodecs);
}

AudioEncoder::~AudioEncoder() = default;

std::unique_ptr<media::AudioEncoder> AudioEncoder::CreateMediaAudioEncoder(
    const ParsedConfig& config) {
  if (auto result = CreatePlatformAudioEncoder(config.options.codec))
    return result;
  return CreateSoftwareAudioEncoder(config.options.codec);
}

void AudioEncoder::ProcessConfigure(Request* request) {
  DCHECK_NE(state_.AsEnum(), V8CodecState::Enum::kClosed);
  DCHECK_EQ(request->type, Request::Type::kConfigure);
  DCHECK(active_config_);
  DCHECK_EQ(active_config_->options.codec, media::AudioCodec::kOpus);
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  request->StartTracing();

  media_encoder_ = CreateMediaAudioEncoder(*active_config_);
  if (!media_encoder_) {
    HandleError(logger_->MakeException(
        "Encoder creation error.",
        media::EncoderStatus(
            media::EncoderStatus::Codes::kEncoderInitializationError,
            "Unable to create encoder (most likely unsupported "
            "codec/acceleration requirement combination)")));
    request->EndTracing();
    return;
  }

  auto output_cb = ConvertToBaseRepeatingCallback(CrossThreadBindRepeating(
      &AudioEncoder::CallOutputCallback, WrapCrossThreadWeakPersistent(this),
      // We can't use |active_config_| from |this| because it can change by
      // the time the callback is executed.
      WrapCrossThreadPersistent(active_config_.Get()), reset_count_));

  auto done_callback = [](AudioEncoder* self, media::AudioCodec codec,
                          Request* req, media::EncoderStatus status) {
    if (!self || self->reset_count_ != req->reset_count) {
      req->EndTracing(/*aborted=*/true);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (!status.is_ok()) {
      self->HandleError(
          self->logger_->MakeException("Encoding error.", std::move(status)));
    } else {
      base::UmaHistogramEnumeration("Blink.WebCodecs.AudioEncoder.Codec",
                                    codec);
    }

    req->EndTracing();
    self->blocking_request_in_progress_ = false;
    self->ProcessRequests();
  };

  blocking_request_in_progress_ = true;
  first_output_after_configure_ = true;
  media_encoder_->Initialize(
      active_config_->options, std::move(output_cb),
      ConvertToBaseOnceCallback(CrossThreadBindOnce(
          done_callback, WrapCrossThreadWeakPersistent(this),
          active_config_->options.codec, WrapCrossThreadPersistent(request))));
}

void AudioEncoder::ProcessEncode(Request* request) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, V8CodecState::Enum::kConfigured);
  DCHECK(media_encoder_);
  DCHECK_EQ(request->type, Request::Type::kEncode);
  DCHECK_GT(requested_encodes_, 0);

  request->StartTracing();

  auto* audio_data = request->input.Release();

  auto data = audio_data->data();

  // The data shouldn't be closed at this point.
  DCHECK(data);

  auto done_callback = [](AudioEncoder* self, Request* req,
                          media::EncoderStatus status) {
    if (!self || self->reset_count_ != req->reset_count) {
      req->EndTracing(/*aborted=*/true);
      return;
    }
    DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
    if (!status.is_ok()) {
      self->HandleError(
          self->logger_->MakeException("Encoding error.", std::move(status)));
    }

    req->EndTracing();
    self->ProcessRequests();
  };

  if (data->channel_count() != active_config_->options.channels ||
      data->sample_rate() != active_config_->options.sample_rate) {
    HandleError(logger_->MakeException(
        "Input audio buffer is incompatible with codec parameters",
        media::EncoderStatus(media::EncoderStatus::Codes::kEncoderFailedEncode)
            .WithData("channels", data->channel_count())
            .WithData("sampleRate", data->sample_rate())));

    request->EndTracing();

    audio_data->close();
    return;
  }

  // If |data|'s memory layout allows it, |audio_bus| will be a simple wrapper
  // around it. Otherwise, |audio_bus| will contain a converted copy of |data|.
  auto audio_bus = media::AudioBuffer::WrapOrCopyToAudioBus(data);

  base::TimeTicks timestamp = base::TimeTicks() + data->timestamp();

  --requested_encodes_;
  media_encoder_->Encode(std::move(audio_bus), timestamp,
                         ConvertToBaseOnceCallback(CrossThreadBindOnce(
                             done_callback, WrapCrossThreadWeakPersistent(this),
                             WrapCrossThreadPersistent(request))));

  audio_data->close();
}

void AudioEncoder::ProcessReconfigure(Request* request) {
  // Audio decoders don't currently support any meaningful reconfiguring
}

AudioEncoder::ParsedConfig* AudioEncoder::ParseConfig(
    const AudioEncoderConfig* opts,
    ExceptionState& exception_state) {
  return ParseConfigStatic(opts, exception_state);
}

bool AudioEncoder::CanReconfigure(ParsedConfig& original_config,
                                  ParsedConfig& new_config) {
  return original_config.options.codec == new_config.options.codec &&
         original_config.options.channels == new_config.options.channels &&
         original_config.options.bitrate == new_config.options.bitrate &&
         original_config.options.sample_rate == new_config.options.sample_rate;
}

bool AudioEncoder::VerifyCodecSupport(ParsedConfig* config,
                                      ExceptionState& exception_state) {
  return VerifyCodecSupportStatic(config, &exception_state);
}

void AudioEncoder::CallOutputCallback(
    ParsedConfig* active_config,
    uint32_t reset_count,
    media::EncodedAudioBuffer encoded_buffer,
    absl::optional<media::AudioEncoder::CodecDescription> codec_desc) {
  DCHECK(active_config);
  if (!script_state_->ContextIsValid() || !output_callback_ ||
      state_.AsEnum() != V8CodecState::Enum::kConfigured ||
      reset_count != reset_count_) {
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  MarkCodecActive();

  auto buffer = media::DecoderBuffer::FromArray(
      std::move(encoded_buffer.encoded_data), encoded_buffer.encoded_data_size);
  buffer->set_timestamp(encoded_buffer.timestamp - base::TimeTicks());
  buffer->set_is_key_frame(true);
  buffer->set_duration(encoded_buffer.duration);
  auto* chunk = MakeGarbageCollected<EncodedAudioChunk>(std::move(buffer));

  auto* metadata = MakeGarbageCollected<EncodedAudioChunkMetadata>();
  if (first_output_after_configure_ || codec_desc.has_value()) {
    first_output_after_configure_ = false;
    auto* decoder_config = MakeGarbageCollected<AudioDecoderConfig>();
    decoder_config->setCodec(active_config->codec_string);
    decoder_config->setSampleRate(encoded_buffer.params.sample_rate());
    decoder_config->setNumberOfChannels(active_config->options.channels);
    if (codec_desc.has_value()) {
      auto* desc_array_buf = DOMArrayBuffer::Create(codec_desc.value().data(),
                                                    codec_desc.value().size());
      decoder_config->setDescription(
          MakeGarbageCollected<AllowSharedBufferSource>(desc_array_buf));
    }
    metadata->setDecoderConfig(decoder_config);
  }

  TRACE_EVENT_BEGIN1(kCategory, GetTraceNames()->output.c_str(), "timestamp",
                     chunk->timestamp());

  ScriptState::Scope scope(script_state_);
  output_callback_->InvokeAndReportException(nullptr, chunk, metadata);

  TRACE_EVENT_END0(kCategory, GetTraceNames()->output.c_str());
}

// static
ScriptPromise AudioEncoder::isConfigSupported(ScriptState* script_state,
                                              const AudioEncoderConfig* config,
                                              ExceptionState& exception_state) {
  auto* parsed_config = ParseConfigStatic(config, exception_state);
  if (!parsed_config) {
    DCHECK(exception_state.HadException());
    return ScriptPromise();
  }

  auto* support = AudioEncoderSupport::Create();
  support->setSupported(VerifyCodecSupportStatic(parsed_config, nullptr));
  support->setConfig(CopyConfig(*config));

  return ScriptPromise::Cast(
      script_state, ToV8Traits<AudioEncoderSupport>::ToV8(script_state, support)
                        .ToLocalChecked());
}

}  // namespace blink
