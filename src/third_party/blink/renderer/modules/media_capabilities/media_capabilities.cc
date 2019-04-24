// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/media_capabilities/media_capabilities.h"

#include <memory>

#include "media/base/mime_util.h"
#include "media/base/supported_types.h"
#include "media/filters/stream_parser_factory.h"
#include "third_party/blink/public/platform/modules/media_capabilities/web_media_capabilities_client.h"
#include "third_party/blink/public/platform/modules/media_capabilities/web_media_capabilities_info.h"
#include "third_party/blink/public/platform/modules/media_capabilities/web_media_configuration.h"
#include "third_party/blink/public/platform/modules/media_capabilities/web_media_decoding_configuration.h"
#include "third_party/blink/public/platform/platform.h"
#include "third_party/blink/public/platform/task_type.h"
#include "third_party/blink/public/platform/web_media_recorder_handler.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise.h"
#include "third_party/blink/renderer/bindings/core/v8/script_promise_resolver.h"
#include "third_party/blink/renderer/core/dom/dom_exception.h"
#include "third_party/blink/renderer/core/html/parser/html_parser_idioms.h"
#include "third_party/blink/renderer/modules/encryptedmedia/encrypted_media_utils.h"
#include "third_party/blink/renderer/modules/media_capabilities/media_capabilities_decoding_info.h"
#include "third_party/blink/renderer/modules/media_capabilities/media_capabilities_decoding_info_callbacks.h"
#include "third_party/blink/renderer/modules/media_capabilities/media_capabilities_encoding_info_callbacks.h"
#include "third_party/blink/renderer/modules/media_capabilities/media_capabilities_info.h"
#include "third_party/blink/renderer/modules/media_capabilities/media_configuration.h"
#include "third_party/blink/renderer/modules/media_capabilities/media_decoding_configuration.h"
#include "third_party/blink/renderer/modules/media_capabilities/media_encoding_configuration.h"
#include "third_party/blink/renderer/platform/bindings/script_state.h"
#include "third_party/blink/renderer/platform/bindings/v8_throw_exception.h"
#include "third_party/blink/renderer/platform/network/parsed_content_type.h"

namespace blink {

namespace {

constexpr const char* kApplicationMimeTypePrefix = "application/";
constexpr const char* kAudioMimeTypePrefix = "audio/";
constexpr const char* kVideoMimeTypePrefix = "video/";
constexpr const char* kCodecsMimeTypeParam = "codecs";

// Computes the effective framerate value based on the framerate field passed to
// the VideoConfiguration. It will return the parsed string as a double or
// compute the value in case of it is of the form "a/b".
// If the value is not valid, it will return NaN.
double ComputeFrameRate(const String& fps_str) {
  double result = ParseToDoubleForNumberType(fps_str);
  if (std::isfinite(result))
    return result > 0 ? result : std::numeric_limits<double>::quiet_NaN();

  wtf_size_t slash_position = fps_str.find('/');
  if (slash_position == kNotFound)
    return std::numeric_limits<double>::quiet_NaN();

  double numerator =
      ParseToDoubleForNumberType(fps_str.Substring(0, slash_position));
  double denominator = ParseToDoubleForNumberType(fps_str.Substring(
      slash_position + 1, fps_str.length() - slash_position - 1));
  if (std::isfinite(numerator) && std::isfinite(denominator) && numerator > 0 &&
      denominator > 0) {
    return numerator / denominator;
  }

  return std::numeric_limits<double>::quiet_NaN();
}

bool IsValidMimeType(const String& content_type, const String& prefix) {
  ParsedContentType parsed_content_type(content_type);

  if (!parsed_content_type.IsValid())
    return false;

  if (!parsed_content_type.MimeType().StartsWith(prefix) &&
      !parsed_content_type.MimeType().StartsWith(kApplicationMimeTypePrefix)) {
    return false;
  }
  const auto& parameters = parsed_content_type.GetParameters();

  if (parameters.ParameterCount() > 1)
    return false;

  if (parameters.ParameterCount() == 0)
    return true;

  return parameters.begin()->name.LowerASCII() == kCodecsMimeTypeParam;
}

bool IsValidMediaConfiguration(const MediaConfiguration* configuration) {
  return configuration->hasAudio() || configuration->hasVideo();
}

bool IsValidMediaDecodingConfiguration(
    const MediaDecodingConfiguration* configuration,
    String* message) {
  if (!IsValidMediaConfiguration(configuration)) {
    *message =
        "The configuration dictionary has neither |video| nor |audio| "
        "specified and needs at least one of them.";
    return false;
  }

  if (configuration->hasKeySystemConfiguration()) {
    if (configuration->keySystemConfiguration()->hasAudioRobustness() &&
        !configuration->hasAudio()) {
      *message =
          "The keySystemConfiguration object contains an "
          "audioRobustness property but the root configuration has no "
          "audio configuration.";
      return false;
    }

    if (configuration->keySystemConfiguration()->hasVideoRobustness() &&
        !configuration->hasVideo()) {
      *message =
          "The keySystemConfiguration object contains an "
          "videoRobustness property but the root configuration has no "
          "video configuration.";
      return false;
    }
  }

  return true;
}

bool IsValidVideoConfiguration(const VideoConfiguration* configuration) {
  DCHECK(configuration->hasContentType());

  if (!IsValidMimeType(configuration->contentType(), kVideoMimeTypePrefix))
    return false;

  DCHECK(configuration->hasFramerate());
  if (std::isnan(ComputeFrameRate(configuration->framerate())))
    return false;

  return true;
}

bool IsValidAudioConfiguration(const AudioConfiguration* configuration) {
  DCHECK(configuration->hasContentType());

  if (!IsValidMimeType(configuration->contentType(), kAudioMimeTypePrefix))
    return false;

  return true;
}

WebAudioConfiguration ToWebAudioConfiguration(
    const AudioConfiguration* configuration) {
  WebAudioConfiguration web_configuration;

  // |contentType| is mandatory.
  DCHECK(configuration->hasContentType());
  ParsedContentType parsed_content_type(configuration->contentType());
  DCHECK(parsed_content_type.IsValid());
  DCHECK(!parsed_content_type.GetParameters().HasDuplicatedNames());

  DEFINE_STATIC_LOCAL(const String, codecs, ("codecs"));
  web_configuration.mime_type = parsed_content_type.MimeType().LowerASCII();
  web_configuration.codec = parsed_content_type.ParameterValueForName(codecs);

  // |channels| is optional and will be set to a null WebString if not present.
  web_configuration.channels = configuration->hasChannels()
                                   ? WebString(configuration->channels())
                                   : WebString();

  if (configuration->hasBitrate())
    web_configuration.bitrate = configuration->bitrate();

  if (configuration->hasSamplerate())
    web_configuration.samplerate = configuration->samplerate();

  return web_configuration;
}

WebVideoConfiguration ToWebVideoConfiguration(
    const VideoConfiguration* configuration) {
  WebVideoConfiguration web_configuration;

  // All the properties are mandatory.
  DCHECK(configuration->hasContentType());
  ParsedContentType parsed_content_type(configuration->contentType());
  DCHECK(parsed_content_type.IsValid());
  DCHECK(!parsed_content_type.GetParameters().HasDuplicatedNames());

  DEFINE_STATIC_LOCAL(const String, codecs, ("codecs"));
  web_configuration.mime_type = parsed_content_type.MimeType().LowerASCII();
  web_configuration.codec = parsed_content_type.ParameterValueForName(codecs);

  DCHECK(configuration->hasWidth());
  web_configuration.width = configuration->width();

  DCHECK(configuration->hasHeight());
  web_configuration.height = configuration->height();

  DCHECK(configuration->hasBitrate());
  web_configuration.bitrate = configuration->bitrate();

  DCHECK(configuration->hasFramerate());
  double computed_framerate = ComputeFrameRate(configuration->framerate());
  DCHECK(!std::isnan(computed_framerate));
  web_configuration.framerate = computed_framerate;

  return web_configuration;
}

WebMediaCapabilitiesKeySystemConfiguration
ToWebMediaCapabilitiesKeySystemConfiguration(
    const MediaCapabilitiesKeySystemConfiguration* configuration) {
  WebMediaCapabilitiesKeySystemConfiguration web_configuration;

  // |keySystem| is mandatory.
  DCHECK(configuration->hasKeySystem());
  web_configuration.key_system = configuration->keySystem();

  if (configuration->hasInitDataType()) {
    web_configuration.init_data_type =
        EncryptedMediaUtils::ConvertToInitDataType(
            configuration->initDataType());
  }

  // |audio_robustness| and |video_robustess| have a default value.
  if (configuration->hasAudioRobustness())
    web_configuration.audio_robustness = configuration->audioRobustness();
  if (configuration->hasVideoRobustness())
    web_configuration.video_robustness = configuration->videoRobustness();

  // |distinctive_identifier| and |persistent_state| have a default value so
  // they should only be set if needed.
  if (configuration->hasDistinctiveIdentifier()) {
    web_configuration.distinctive_identifier =
        EncryptedMediaUtils::ConvertToMediaKeysRequirement(
            configuration->distinctiveIdentifier());
  }
  if (configuration->hasPersistentState()) {
    web_configuration.persistent_state =
        EncryptedMediaUtils::ConvertToMediaKeysRequirement(
            configuration->persistentState());
  }

  // |session_types| has a default value because it is a WebVector.
  if (configuration->hasSessionTypes()) {
    for (const String& session_type : configuration->sessionTypes()) {
      web_configuration.session_types.emplace_back(
          EncryptedMediaUtils::ConvertToSessionType(session_type));
    }
  }

  return web_configuration;
}

WebMediaDecodingConfiguration ToWebMediaConfiguration(
    const MediaDecodingConfiguration* configuration) {
  WebMediaDecodingConfiguration web_configuration;

  // |type| is mandatory.
  DCHECK(configuration->hasType());
  if (configuration->type() == "file")
    web_configuration.type = MediaConfigurationType::kFile;
  else if (configuration->type() == "media-source")
    web_configuration.type = MediaConfigurationType::kMediaSource;
  else
    NOTREACHED();

  if (configuration->hasAudio()) {
    web_configuration.audio_configuration =
        ToWebAudioConfiguration(configuration->audio());
  }

  if (configuration->hasVideo()) {
    web_configuration.video_configuration =
        ToWebVideoConfiguration(configuration->video());
  }

  if (configuration->hasKeySystemConfiguration()) {
    web_configuration.key_system_configuration =
        ToWebMediaCapabilitiesKeySystemConfiguration(
            configuration->keySystemConfiguration());
  }

  return web_configuration;
}

WebMediaConfiguration ToWebMediaConfiguration(
    const MediaEncodingConfiguration* configuration) {
  WebMediaConfiguration web_configuration;

  // |type| is required.
  DCHECK(configuration->hasType());
  if (configuration->type() == "record")
    web_configuration.type = MediaConfigurationType::kRecord;
  else if (configuration->type() == "transmission")
    web_configuration.type = MediaConfigurationType::kTransmission;
  else
    NOTREACHED();

  if (configuration->hasAudio()) {
    web_configuration.audio_configuration =
        ToWebAudioConfiguration(configuration->audio());
  }

  if (configuration->hasVideo()) {
    web_configuration.video_configuration =
        ToWebVideoConfiguration(configuration->video());
  }

  return web_configuration;
}

// Utility function that will create a MediaCapabilitiesDecodingInfo object with
// all the values set to either true or false.
MediaCapabilitiesDecodingInfo* CreateDecodingInfoWith(bool value) {
  MediaCapabilitiesDecodingInfo* info = MediaCapabilitiesDecodingInfo::Create();
  info->setSupported(value);
  info->setSmooth(value);
  info->setPowerEfficient(value);

  return info;
}

bool CheckMseSupport(const WebMediaConfiguration& configuration) {
  DCHECK_EQ(MediaConfigurationType::kMediaSource, configuration.type);

  // For MSE queries, we assume the queried audio and video streams will be
  // placed into separate source buffers.
  // TODO(chcunningham): Clarify this assumption in the spec.

  // Media MIME API expects a vector of codec strings. We query audio and video
  // separately, so |codec_string|.size() should always be 1 or 0 (when no
  // codecs parameter is required for the given mime type).
  std::vector<std::string> codec_vector;

  if (configuration.audio_configuration) {
    const WebAudioConfiguration& audio_config =
        configuration.audio_configuration.value();

    if (!audio_config.codec.Ascii().empty())
      codec_vector.push_back(audio_config.codec.Ascii());

    if (!media::StreamParserFactory::IsTypeSupported(
            audio_config.mime_type.Ascii(), codec_vector)) {
      DVLOG(2) << __func__ << " MSE does not support audio config: "
               << audio_config.mime_type.Ascii() << " "
               << (codec_vector.empty() ? "" : codec_vector[1]);
      return false;
    }
  }

  if (configuration.video_configuration) {
    const WebVideoConfiguration& video_config =
        configuration.video_configuration.value();

    codec_vector.clear();
    if (!video_config.codec.Ascii().empty())
      codec_vector.push_back(video_config.codec.Ascii());

    if (!media::StreamParserFactory::IsTypeSupported(
            video_config.mime_type.Ascii(), codec_vector)) {
      DVLOG(2) << __func__ << " MSE does not support video config: "
               << video_config.mime_type.Ascii() << " "
               << (codec_vector.empty() ? "" : codec_vector[1]);
      return false;
    }
  }

  return true;
}

// Returns whether the AudioConfiguration is supported.
// Sends console warnings if the codec string was badly formatted.
bool IsAudioConfigurationSupported(const WebAudioConfiguration& audio_config,
                                   String* console_warning) {
  media::AudioCodec audio_codec = media::kUnknownAudioCodec;
  bool is_audio_codec_ambiguous = true;

  if (!media::ParseAudioCodecString(audio_config.mime_type.Ascii(),
                                    audio_config.codec.Ascii(),
                                    &is_audio_codec_ambiguous, &audio_codec)) {
    console_warning->append(("Failed to parse audio contentType: " +
                             audio_config.mime_type.Ascii() +
                             "; codecs=" + audio_config.codec.Ascii())
                                .c_str());

    return false;
  }

  if (is_audio_codec_ambiguous) {
    console_warning->append(("Invalid (ambiguous) audio codec string: " +
                             audio_config.codec.Ascii())
                                .c_str());

    return false;
  }

  return media::IsSupportedAudioType({audio_codec});
}

}  // anonymous namespace

MediaCapabilities::MediaCapabilities() = default;

ScriptPromise MediaCapabilities::decodingInfo(
    ScriptState* script_state,
    const MediaDecodingConfiguration* configuration) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  String message;
  if (!IsValidMediaDecodingConfiguration(configuration, &message)) {
    resolver->Reject(
        V8ThrowException::CreateTypeError(script_state->GetIsolate(), message));
    return promise;
  }

  if (configuration->hasVideo() &&
      !IsValidVideoConfiguration(configuration->video())) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "The video configuration dictionary is not valid."));
    return promise;
  }

  if (configuration->hasAudio() &&
      !IsValidAudioConfiguration(configuration->audio())) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "The audio configuration dictionary is not valid."));
    return promise;
  }

  if (configuration->hasKeySystemConfiguration()) {
    ExecutionContext* execution_context = ExecutionContext::From(script_state);
    DCHECK(execution_context);
    if (execution_context->IsWorkerGlobalScope()) {
      resolver->Reject(DOMException::Create(
          DOMExceptionCode::kInvalidStateError,
          "Encrypted Media decoding info not available in Worker context."));
      return promise;
    }

    if (!execution_context->IsSecureContext()) {
      resolver->Reject(
          DOMException::Create(DOMExceptionCode::kSecurityError,
                               "Encrypted Media decoding info can only be "
                               "queried in a secure context."));
      return promise;
    }
  }

  WebMediaDecodingConfiguration web_config =
      ToWebMediaConfiguration(configuration);

  DCHECK(message.IsEmpty());

  // MSE support is cheap to check (regex matching). Do it first.
  if (web_config.type == MediaConfigurationType::kMediaSource &&
      !CheckMseSupport(web_config)) {
    resolver->Resolve(WrapPersistent(CreateDecodingInfoWith(false)));
    return promise;
  }

  bool audio_supported = true;

  if (configuration->hasAudio()) {
    audio_supported = IsAudioConfigurationSupported(
        *web_config.audio_configuration, &message);
  }

  // No need to check video capabilities if video not included in configuration
  // or when audio is already known to be unsupported.
  if (!audio_supported || !configuration->hasVideo()) {
    // The call to IsAudioConfiguraationSupported may have returned a console
    // message to print. It would only happen when |audio_supported| is false.
    if (!message.IsEmpty()) {
      if (ExecutionContext* execution_context =
              ExecutionContext::From(script_state)) {
        execution_context->AddWarningMessage(ConsoleLogger::Source::kOther,
                                             message);
      }
    }

    resolver->Resolve(WrapPersistent(CreateDecodingInfoWith(audio_supported)));
    return promise;
  }

  Platform::Current()->MediaCapabilitiesClient()->DecodingInfo(
      web_config,
      std::make_unique<MediaCapabilitiesDecodingInfoCallbacks>(resolver));

  return promise;
}

ScriptPromise MediaCapabilities::encodingInfo(
    ScriptState* script_state,
    const MediaEncodingConfiguration* configuration) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!IsValidMediaConfiguration(configuration)) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "The configuration dictionary has neither |video| nor |audio| "
        "specified and needs at least one of them."));
    return promise;
  }

  if (configuration->hasVideo() &&
      !IsValidVideoConfiguration(configuration->video())) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "The video configuration dictionary is not valid."));
    return promise;
  }

  if (configuration->hasAudio() &&
      !IsValidAudioConfiguration(configuration->audio())) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "The audio configuration dictionary is not valid."));
    return promise;
  }

  // TODO(crbug.com/817382): Add "transmission" type support.
  if (configuration->type() == "transmission") {
    resolver->Reject(
        DOMException::Create(DOMExceptionCode::kNotSupportedError,
                             "\"transmission\" type is not implemented yet."));
    return promise;
  }

  std::unique_ptr<WebMediaRecorderHandler> handler =
      Platform::Current()->CreateMediaRecorderHandler(
          ExecutionContext::From(script_state)
              ->GetTaskRunner(TaskType::kInternalMediaRealTime));
  if (!handler) {
    resolver->Reject(DOMException::Create(
        DOMExceptionCode::kInvalidStateError,
        "Platform error: could not create MediaRecorderHandler."));
    return promise;
  }

  handler->EncodingInfo(
      ToWebMediaConfiguration(configuration),
      std::make_unique<MediaCapabilitiesEncodingInfoCallbacks>(resolver));

  return promise;
}

}  // namespace blink
