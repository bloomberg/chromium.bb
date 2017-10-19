// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/audio_output_devices/AudioOutputDeviceClient.h"

#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalFrame.h"

namespace blink {

AudioOutputDeviceClient::AudioOutputDeviceClient(LocalFrame& frame)
    : Supplement<LocalFrame>(frame) {}

const char* AudioOutputDeviceClient::SupplementName() {
  return "AudioOutputDeviceClient";
}

AudioOutputDeviceClient* AudioOutputDeviceClient::From(
    ExecutionContext* context) {
  if (!context || !context->IsDocument())
    return nullptr;

  const Document* document = ToDocument(context);
  if (!document->GetFrame())
    return nullptr;

  return static_cast<AudioOutputDeviceClient*>(
      Supplement<LocalFrame>::From(document->GetFrame(), SupplementName()));
}

void ProvideAudioOutputDeviceClientTo(LocalFrame& frame,
                                      AudioOutputDeviceClient* client) {
  Supplement<LocalFrame>::ProvideTo(
      frame, AudioOutputDeviceClient::SupplementName(), client);
}

void AudioOutputDeviceClient::Trace(blink::Visitor* visitor) {
  Supplement<LocalFrame>::Trace(visitor);
}

}  // namespace blink
