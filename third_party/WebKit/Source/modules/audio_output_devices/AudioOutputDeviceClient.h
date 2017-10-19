// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioOutputDeviceClient_h
#define AudioOutputDeviceClient_h

#include "core/frame/LocalFrame.h"
#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "public/platform/WebSetSinkIdCallbacks.h"
#include <memory>

namespace blink {

class ExecutionContext;
class WebString;

class MODULES_EXPORT AudioOutputDeviceClient : public Supplement<LocalFrame> {
 public:
  explicit AudioOutputDeviceClient(LocalFrame&);
  virtual ~AudioOutputDeviceClient() {}

  // Checks that a given sink exists and has permissions to be used from the
  // origin of the current frame.
  virtual void CheckIfAudioSinkExistsAndIsAuthorized(
      ExecutionContext*,
      const WebString& sink_id,
      std::unique_ptr<WebSetSinkIdCallbacks>) = 0;

  virtual void Trace(blink::Visitor*);

  // Supplement requirements.
  static AudioOutputDeviceClient* From(ExecutionContext*);
  static const char* SupplementName();
};

MODULES_EXPORT void ProvideAudioOutputDeviceClientTo(LocalFrame&,
                                                     AudioOutputDeviceClient*);

}  // namespace blink

#endif  // AudioOutputDeviceClient_h
