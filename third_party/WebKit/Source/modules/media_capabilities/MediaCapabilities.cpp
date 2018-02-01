// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_capabilities/MediaCapabilities.h"

#include <memory>

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "modules/media_capabilities/MediaCapabilitiesInfo.h"
#include "modules/media_capabilities/MediaConfiguration.h"
#include "modules/media_capabilities/MediaDecodingConfiguration.h"
#include "modules/media_capabilities/MediaEncodingConfiguration.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8ThrowException.h"
#include "platform/network/ParsedContentType.h"
#include "public/platform/Platform.h"
#include "public/platform/WebMediaRecorderHandler.h"
#include "public/platform/modules/media_capabilities/WebMediaCapabilitiesClient.h"
#include "public/platform/modules/media_capabilities/WebMediaCapabilitiesInfo.h"
#include "public/platform/modules/media_capabilities/WebMediaConfiguration.h"

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

  size_t slash_position = fps_str.find('/');
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

bool IsValidMediaConfiguration(const MediaConfiguration& configuration) {
  return configuration.hasAudio() || configuration.hasVideo();
}

bool IsValidVideoConfiguration(const VideoConfiguration& configuration) {
  DCHECK(configuration.hasContentType());

  if (!IsValidMimeType(configuration.contentType(), kVideoMimeTypePrefix))
    return false;

  DCHECK(configuration.hasFramerate());
  if (std::isnan(ComputeFrameRate(configuration.framerate())))
    return false;

  return true;
}

bool IsValidAudioConfiguration(const AudioConfiguration& configuration) {
  DCHECK(configuration.hasContentType());

  if (!IsValidMimeType(configuration.contentType(), kAudioMimeTypePrefix))
    return false;

  return true;
}

WebAudioConfiguration ToWebAudioConfiguration(
    const AudioConfiguration& configuration) {
  WebAudioConfiguration web_configuration;

  // |contentType| is mandatory.
  DCHECK(configuration.hasContentType());
  ParsedContentType parsed_content_type(configuration.contentType());
  DCHECK(parsed_content_type.IsValid());
  DCHECK(!parsed_content_type.GetParameters().HasDuplicatedNames());

  DEFINE_STATIC_LOCAL(const String, codecs, ("codecs"));
  web_configuration.mime_type = parsed_content_type.MimeType().LowerASCII();
  web_configuration.codec = parsed_content_type.ParameterValueForName(codecs);

  // |channels| is optional and will be set to a null WebString if not present.
  web_configuration.channels = configuration.hasChannels()
                                   ? WebString(configuration.channels())
                                   : WebString();

  if (configuration.hasBitrate())
    web_configuration.bitrate = configuration.bitrate();

  if (configuration.hasSamplerate())
    web_configuration.samplerate = configuration.samplerate();

  return web_configuration;
}

WebVideoConfiguration ToWebVideoConfiguration(
    const VideoConfiguration& configuration) {
  WebVideoConfiguration web_configuration;

  // All the properties are mandatory.
  DCHECK(configuration.hasContentType());
  ParsedContentType parsed_content_type(configuration.contentType());
  DCHECK(parsed_content_type.IsValid());
  DCHECK(!parsed_content_type.GetParameters().HasDuplicatedNames());

  DEFINE_STATIC_LOCAL(const String, codecs, ("codecs"));
  web_configuration.mime_type = parsed_content_type.MimeType().LowerASCII();
  web_configuration.codec = parsed_content_type.ParameterValueForName(codecs);

  DCHECK(configuration.hasWidth());
  web_configuration.width = configuration.width();

  DCHECK(configuration.hasHeight());
  web_configuration.height = configuration.height();

  DCHECK(configuration.hasBitrate());
  web_configuration.bitrate = configuration.bitrate();

  DCHECK(configuration.hasFramerate());
  double computed_framerate = ComputeFrameRate(configuration.framerate());
  DCHECK(!std::isnan(computed_framerate));
  web_configuration.framerate = computed_framerate;

  return web_configuration;
}

WebMediaConfiguration ToWebMediaConfiguration(
    const MediaDecodingConfiguration& configuration) {
  WebMediaConfiguration web_configuration;

  // |type| is mandatory.
  DCHECK(configuration.hasType());
  if (configuration.type() == "file")
    web_configuration.type = MediaConfigurationType::kFile;
  else if (configuration.type() == "media-source")
    web_configuration.type = MediaConfigurationType::kMediaSource;
  else
    NOTREACHED();

  if (configuration.hasAudio()) {
    web_configuration.audio_configuration =
        ToWebAudioConfiguration(configuration.audio());
  }

  if (configuration.hasVideo()) {
    web_configuration.video_configuration =
        ToWebVideoConfiguration(configuration.video());
  }

  return web_configuration;
}

WebMediaConfiguration ToWebMediaConfiguration(
    const MediaEncodingConfiguration& configuration) {
  WebMediaConfiguration web_configuration;

  // TODO(mcasas): parse and set the type for encoding.

  if (configuration.hasAudio()) {
    web_configuration.audio_configuration =
        ToWebAudioConfiguration(configuration.audio());
  }

  if (configuration.hasVideo()) {
    web_configuration.video_configuration =
        ToWebVideoConfiguration(configuration.video());
  }

  return web_configuration;
}

}  // anonymous namespace

MediaCapabilities::MediaCapabilities() = default;

ScriptPromise MediaCapabilities::decodingInfo(
    ScriptState* script_state,
    const MediaDecodingConfiguration& configuration) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!IsValidMediaConfiguration(configuration)) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "The configuration dictionary has neither |video| nor |audio| "
        "specified and needs at least one of them."));
    return promise;
  }

  if (configuration.hasVideo() &&
      !IsValidVideoConfiguration(configuration.video())) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "The video configuration dictionary is not valid."));
    return promise;
  }

  if (configuration.hasAudio() &&
      !IsValidAudioConfiguration(configuration.audio())) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "The audio configuration dictionary is not valid."));
    return promise;
  }

  Platform::Current()->MediaCapabilitiesClient()->DecodingInfo(
      ToWebMediaConfiguration(configuration),
      std::make_unique<CallbackPromiseAdapter<MediaCapabilitiesInfo, void>>(
          resolver));

  return promise;
}

ScriptPromise MediaCapabilities::encodingInfo(
    ScriptState* script_state,
    const MediaEncodingConfiguration& configuration) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!IsValidMediaConfiguration(configuration)) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "The configuration dictionary has neither |video| nor |audio| "
        "specified and needs at least one of them."));
    return promise;
  }

  if (configuration.hasVideo() &&
      !IsValidVideoConfiguration(configuration.video())) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "The video configuration dictionary is not valid."));
    return promise;
  }

  if (configuration.hasAudio() &&
      !IsValidAudioConfiguration(configuration.audio())) {
    resolver->Reject(V8ThrowException::CreateTypeError(
        script_state->GetIsolate(),
        "The audio configuration dictionary is not valid."));
    return promise;
  }

  std::unique_ptr<WebMediaRecorderHandler> handler =
      Platform::Current()->CreateMediaRecorderHandler();
  if (!handler) {
    resolver->Reject(DOMException::Create(
        kInvalidStateError,
        "Platform error: could not create MediaRecorderHandler."));
    return promise;
  }

  handler->EncodingInfo(
      ToWebMediaConfiguration(configuration),
      std::make_unique<CallbackPromiseAdapter<MediaCapabilitiesInfo, void>>(
          resolver));
  return promise;
}

}  // namespace blink
