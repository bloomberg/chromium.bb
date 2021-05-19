// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/webcodecs/audio_decoder.h"

#include "media/base/audio_codecs.h"
#include "media/base/audio_decoder.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/channel_layout.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_util.h"
#include "media/base/mime_util.h"
#include "media/base/supported_types.h"
#include "media/base/waiting.h"
#include "third_party/blink/public/mojom/web_feature/web_feature.mojom-blink.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_config.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_init.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_audio_decoder_support.h"
#include "third_party/blink/renderer/bindings/modules/v8/v8_encoded_audio_chunk.h"
#include "third_party/blink/renderer/modules/webaudio/base_audio_context.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_decoder_broker.h"
#include "third_party/blink/renderer/modules/webcodecs/audio_frame.h"
#include "third_party/blink/renderer/modules/webcodecs/codec_config_eval.h"
#include "third_party/blink/renderer/platform/audio/audio_utilities.h"
#include "third_party/blink/renderer/platform/instrumentation/use_counter.h"

#include <memory>
#include <vector>

namespace blink {

bool IsValidConfig(const AudioDecoderConfig& config,
                   media::AudioType& out_audio_type,
                   String& out_console_message) {
  media::AudioCodec codec = media::kUnknownAudioCodec;
  bool is_codec_ambiguous = true;
  bool parse_succeeded = ParseAudioCodecString("", config.codec().Utf8(),
                                               &is_codec_ambiguous, &codec);

  if (!parse_succeeded) {
    out_console_message = "Failed to parse codec string.";
    return false;
  }

  if (is_codec_ambiguous) {
    out_console_message = "Codec string is ambiguous.";
    return false;
  }

  out_audio_type = {codec};
  return true;
}

AudioDecoderConfig* CopyConfig(const AudioDecoderConfig& config) {
  AudioDecoderConfig* copy = AudioDecoderConfig::Create();
  copy->setCodec(config.codec());
  copy->setSampleRate(config.sampleRate());
  copy->setNumberOfChannels(config.numberOfChannels());
  if (config.hasDescription()) {
    DOMArrayPiece buffer(config.description());
    DOMArrayBuffer* buffer_copy =
        DOMArrayBuffer::Create(buffer.Data(), buffer.ByteLength());
    copy->setDescription(
        ArrayBufferOrArrayBufferView::FromArrayBuffer(buffer_copy));
  }
  return copy;
}

// static
std::unique_ptr<AudioDecoderTraits::MediaDecoderType>
AudioDecoderTraits::CreateDecoder(
    ExecutionContext& execution_context,
    media::GpuVideoAcceleratorFactories* gpu_factories,
    media::MediaLog* media_log) {
  return std::make_unique<AudioDecoderBroker>(media_log, execution_context);
}

// static
void AudioDecoderTraits::UpdateDecoderLog(const MediaDecoderType& decoder,
                                          const MediaConfigType& media_config,
                                          media::MediaLog* media_log) {
  media_log->SetProperty<media::MediaLogProperty::kFrameTitle>(
      std::string("AudioDecoder(WebCodecs)"));
  media_log->SetProperty<media::MediaLogProperty::kAudioDecoderName>(
      decoder.GetDecoderType());
  media_log->SetProperty<media::MediaLogProperty::kIsPlatformAudioDecoder>(
      decoder.IsPlatformDecoder());
  media_log->SetProperty<media::MediaLogProperty::kAudioTracks>(
      std::vector<MediaConfigType>{media_config});
}

// static
media::StatusOr<AudioDecoderTraits::OutputType*> AudioDecoderTraits::MakeOutput(
    scoped_refptr<MediaOutputType> output,
    ExecutionContext* context) {
  if (!blink::audio_utilities::IsValidAudioBufferSampleRate(
          output->sample_rate())) {
    return media::Status(
        media::StatusCode::kInvalidArgument,
        String::Format("Invalid decoded audio output sample rate. Got %u, "
                       "which is outside [%f, %f]",
                       output->sample_rate(),
                       blink::audio_utilities::MinAudioBufferSampleRate(),
                       blink::audio_utilities::MaxAudioBufferSampleRate())
            .Ascii());
  }

  if (static_cast<uint32_t>(output->channel_count()) >
      BaseAudioContext::MaxNumberOfChannels()) {
    return media::Status(media::StatusCode::kInvalidArgument,
                         String::Format("Invalid decoded audio output channel "
                                        "count. Got %u, which exceeds %u",
                                        output->channel_count(),
                                        BaseAudioContext::MaxNumberOfChannels())
                             .Ascii());
  }

  return MakeGarbageCollected<AudioDecoderTraits::OutputType>(
      std::move(output));
}

// static
void AudioDecoderTraits::InitializeDecoder(
    MediaDecoderType& decoder,
    const MediaConfigType& media_config,
    MediaDecoderType::InitCB init_cb,
    MediaDecoderType::OutputCB output_cb) {
  decoder.Initialize(media_config, nullptr /* cdm_context */,
                     std::move(init_cb), output_cb, media::WaitingCB());
}

// static
int AudioDecoderTraits::GetMaxDecodeRequests(const MediaDecoderType& decoder) {
  return 1;
}

// static
AudioDecoder* AudioDecoder::Create(ScriptState* script_state,
                                   const AudioDecoderInit* init,
                                   ExceptionState& exception_state) {
  auto* result =
      MakeGarbageCollected<AudioDecoder>(script_state, init, exception_state);
  return exception_state.HadException() ? nullptr : result;
}

// static
ScriptPromise AudioDecoder::isConfigSupported(ScriptState* script_state,
                                              const AudioDecoderConfig* config,
                                              ExceptionState& exception_state) {
  media::AudioType audio_type;
  String console_message;

  if (!IsValidConfig(*config, audio_type, console_message)) {
    exception_state.ThrowTypeError(console_message);
    return ScriptPromise();
  }

  AudioDecoderSupport* support = AudioDecoderSupport::Create();
  support->setSupported(media::IsSupportedAudioType(audio_type));
  support->setConfig(CopyConfig(*config));

  return ScriptPromise::Cast(script_state, ToV8(support, script_state));
}

// static
CodecConfigEval AudioDecoder::MakeMediaAudioDecoderConfig(
    const ConfigType& config,
    MediaConfigType& out_media_config,
    String& out_console_message) {
  media::AudioType audio_type;

  if (!IsValidConfig(config, audio_type, out_console_message))
    return CodecConfigEval::kInvalid;

  std::vector<uint8_t> extra_data;
  if (config.hasDescription()) {
    DOMArrayPiece buffer(config.description());
    uint8_t* start = static_cast<uint8_t*>(buffer.Data());
    size_t size = buffer.ByteLength();
    extra_data.assign(start, start + size);
  }

  media::ChannelLayout channel_layout =
      config.numberOfChannels() > 8
          // GuesschannelLayout() doesn't know how to guess above 8 channels.
          ? media::CHANNEL_LAYOUT_DISCRETE
          : media::GuessChannelLayout(config.numberOfChannels());

  // TODO(chcunningham): Add sample format to IDL.
  out_media_config.Initialize(
      audio_type.codec, media::kSampleFormatPlanarF32, channel_layout,
      config.sampleRate(), extra_data, media::EncryptionScheme::kUnencrypted,
      base::TimeDelta() /* seek preroll */, 0 /* codec delay */);

  return CodecConfigEval::kSupported;
}

AudioDecoder::AudioDecoder(ScriptState* script_state,
                           const AudioDecoderInit* init,
                           ExceptionState& exception_state)
    : DecoderTemplate<AudioDecoderTraits>(script_state, init, exception_state) {
  UseCounter::Count(ExecutionContext::From(script_state),
                    WebFeature::kWebCodecs);
}

CodecConfigEval AudioDecoder::MakeMediaConfig(const ConfigType& config,
                                              MediaConfigType* out_media_config,
                                              String* out_console_message) {
  DCHECK(out_media_config);
  DCHECK(out_console_message);
  return MakeMediaAudioDecoderConfig(config, *out_media_config,
                                     *out_console_message);
}

media::StatusOr<scoped_refptr<media::DecoderBuffer>>
AudioDecoder::MakeDecoderBuffer(const InputType& chunk) {
  auto decoder_buffer = media::DecoderBuffer::CopyFrom(
      static_cast<uint8_t*>(chunk.data()->Data()), chunk.data()->ByteLength());
  decoder_buffer->set_timestamp(
      base::TimeDelta::FromMicroseconds(chunk.timestamp()));
  decoder_buffer->set_is_key_frame(chunk.type() == "key");
  return decoder_buffer;
}

}  // namespace blink
