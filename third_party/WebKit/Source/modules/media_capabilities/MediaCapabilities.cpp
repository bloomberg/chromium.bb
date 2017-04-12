// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_capabilities/MediaCapabilities.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "modules/media_capabilities/MediaCapabilitiesInfo.h"
#include "modules/media_capabilities/MediaDecodingConfiguration.h"
#include "public/platform/Platform.h"
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
  web_configuration.content_type = configuration.contentType();

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
  web_configuration.content_type = configuration.contentType();

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
    const MediaDecodingConfiguration& configuration) {
  WebMediaConfiguration web_configuration;

  // |type| is mandatory.
  DCHECK(configuration.hasType());

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

  Platform::Current()->MediaCapabilitiesClient()->DecodingInfo(
      ToWebMediaConfiguration(configuration),
      WTF::MakeUnique<CallbackPromiseAdapter<MediaCapabilitiesInfo, void>>(
          resolver));

  return promise;
}

DEFINE_TRACE(MediaCapabilities) {}

}  // namespace blink
