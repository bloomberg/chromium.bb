// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioOutputDeviceClientImpl_h
#define AudioOutputDeviceClientImpl_h

#include <memory>
#include "modules/ModulesExport.h"
#include "modules/audio_output_devices/AudioOutputDeviceClient.h"
#include "platform/heap/Handle.h"

namespace blink {

class MODULES_EXPORT AudioOutputDeviceClientImpl
    : public GarbageCollectedFinalized<AudioOutputDeviceClientImpl>,
      public AudioOutputDeviceClient {
  USING_GARBAGE_COLLECTED_MIXIN(AudioOutputDeviceClientImpl);
  WTF_MAKE_NONCOPYABLE(AudioOutputDeviceClientImpl);

 public:
  explicit AudioOutputDeviceClientImpl(LocalFrame&);

  ~AudioOutputDeviceClientImpl() override;

  // AudioOutputDeviceClient implementation.
  void CheckIfAudioSinkExistsAndIsAuthorized(
      ExecutionContext*,
      const WebString& sink_id,
      std::unique_ptr<WebSetSinkIdCallbacks>) override;

  // GarbageCollectedFinalized implementation.
  virtual void Trace(blink::Visitor* visitor) {
    AudioOutputDeviceClient::Trace(visitor);
  }

 private:
  AudioOutputDeviceClientImpl();
};

}  // namespace blink

#endif  // AudioOutputDeviceClientImpl_h
