// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/media_capabilities/MediaCapabilities.h"

#include "bindings/core/v8/CallbackPromiseAdapter.h"
#include "bindings/core/v8/ScriptPromise.h"
#include "bindings/core/v8/ScriptPromiseResolver.h"
#include "bindings/core/v8/ScriptState.h"
#include "modules/media_capabilities/MediaConfiguration.h"
#include "modules/media_capabilities/MediaDecodingAbility.h"
#include "public/platform/Platform.h"
#include "public/platform/modules/media_capabilities/WebMediaCapabilitiesClient.h"
#include "public/platform/modules/media_capabilities/WebMediaConfiguration.h"
#include "public/platform/modules/media_capabilities/WebMediaDecodingAbility.h"

namespace blink {

namespace {

WebAudioConfiguration toWebAudioConfiguration(
    const AudioConfiguration& configuration) {
  WebAudioConfiguration webConfiguration;

  // contentType is mandatory.
  DCHECK(configuration.hasContentType());
  webConfiguration.contentType = configuration.contentType();

  // channels is optional and will be set to a null WebString if not present.
  webConfiguration.channels = configuration.hasChannels()
                                  ? WebString(configuration.channels())
                                  : WebString();

  if (configuration.hasBitrate())
    webConfiguration.bitrate = configuration.bitrate();

  if (configuration.hasSamplerate())
    webConfiguration.samplerate = configuration.samplerate();

  return webConfiguration;
}

WebVideoConfiguration toWebVideoConfiguration(
    const VideoConfiguration& configuration) {
  WebVideoConfiguration webConfiguration;

  // All the properties are mandatory.
  DCHECK(configuration.hasContentType());
  webConfiguration.contentType = configuration.contentType();

  DCHECK(configuration.hasWidth());
  webConfiguration.width = configuration.width();

  DCHECK(configuration.hasHeight());
  webConfiguration.height = configuration.height();

  DCHECK(configuration.hasBitrate());
  webConfiguration.bitrate = configuration.bitrate();

  DCHECK(configuration.hasFramerate());
  webConfiguration.framerate = configuration.framerate();

  return webConfiguration;
}

WebMediaConfiguration toWebMediaConfiguration(
    const MediaConfiguration& configuration) {
  WebMediaConfiguration webConfiguration;

  // type is mandatory.
  DCHECK(configuration.hasType());

  if (configuration.hasAudio()) {
    webConfiguration.audioConfiguration =
        toWebAudioConfiguration(configuration.audio());
  }

  if (configuration.hasVideo()) {
    webConfiguration.videoConfiguration =
        toWebVideoConfiguration(configuration.video());
  }

  return webConfiguration;
}

}  // anonymous namespace

MediaCapabilities::MediaCapabilities() = default;

ScriptPromise MediaCapabilities::query(
    ScriptState* scriptState,
    const MediaConfiguration& configuration) {
  ScriptPromiseResolver* resolver = ScriptPromiseResolver::create(scriptState);
  ScriptPromise promise = resolver->promise();

  Platform::current()->mediaCapabilitiesClient()->query(
      toWebMediaConfiguration(configuration),
      WTF::makeUnique<CallbackPromiseAdapter<MediaDecodingAbility, void>>(
          resolver));

  return promise;
}

DEFINE_TRACE(MediaCapabilities) {}

}  // namespace blink
