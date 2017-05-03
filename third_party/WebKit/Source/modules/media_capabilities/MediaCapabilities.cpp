// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_capabilities/MediaCapabilities.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "core/dom/DOMException.h"
#include "modules/media_capabilities/MediaCapabilitiesInfo.h"
#include "modules/media_capabilities/MediaConfiguration.h"
#include "modules/media_capabilities/MediaDecodingConfiguration.h"
#include "modules/media_capabilities/MediaEncodingConfiguration.h"
#include "platform/bindings/ScriptState.h"
#include "platform/network/ParsedContentType.h"
#include "public/platform/Platform.h"
#include "public/platform/WebMediaRecorderHandler.h"
#include "public/platform/modules/media_capabilities/WebMediaCapabilitiesClient.h"
#include "public/platform/modules/media_capabilities/WebMediaCapabilitiesInfo.h"
#include "public/platform/modules/media_capabilities/WebMediaConfiguration.h"

namespace blink {

namespace {

WebAudioConfiguration ToWebAudioConfiguration(
    const AudioConfiguration& configuration) {
  WebAudioConfiguration web_configuration;

  // |contentType| is mandatory.
  DCHECK(configuration.hasContentType());
  ParsedContentType parsed_content_type(configuration.contentType(),
                                        ParsedContentType::Mode::kStrict);

  // TODO(chcunningham): Throw TypeError for invalid input.
  // DCHECK(parsed_content_type.IsValid());

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
  ParsedContentType parsed_content_type(configuration.contentType(),
                                        ParsedContentType::Mode::kStrict);

  // TODO(chcunningham): Throw TypeError for invalid input.
  // DCHECK(parsed_content_type.IsValid());

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
  web_configuration.framerate = configuration.framerate();

  return web_configuration;
}

WebMediaConfiguration ToWebMediaConfiguration(
    const MediaConfiguration& configuration) {
  WebMediaConfiguration web_configuration;

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

  // |type| is mandatory.
  DCHECK(configuration.hasType());

  Platform::Current()->MediaCapabilitiesClient()->DecodingInfo(
      ToWebMediaConfiguration(configuration),
      WTF::MakeUnique<CallbackPromiseAdapter<MediaCapabilitiesInfo, void>>(
          resolver));

  return promise;
}

ScriptPromise MediaCapabilities::encodingInfo(
    ScriptState* script_state,
    const MediaEncodingConfiguration& configuration) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::Create(script_state);
  ScriptPromise promise = resolver->Promise();

  if (!configuration.hasVideo() && !configuration.hasAudio()) {
    resolver->Reject(DOMException::Create(
        kSyntaxError,
        "The configuration dictionary has neither |video| nor |audio| "
        "specified and needs at least one of them."));
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
      WTF::MakeUnique<CallbackPromiseAdapter<MediaCapabilitiesInfo, void>>(
          resolver));
  return promise;
}

DEFINE_TRACE(MediaCapabilities) {}

}  // namespace blink
