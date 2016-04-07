// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef AudioOutputDeviceClient_h
#define AudioOutputDeviceClient_h

#include "modules/ModulesExport.h"
#include "platform/Supplementable.h"
#include "public/platform/WebSetSinkIdCallbacks.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class ExecutionContext;
class LocalFrame;
class WebString;
class ScriptState;

class AudioOutputDeviceClient : public Supplement<LocalFrame> {
public:
    virtual ~AudioOutputDeviceClient() {}

    // Checks that a given sink exists and has permissions to be used from the origin of the current frame.
    virtual void checkIfAudioSinkExistsAndIsAuthorized(ExecutionContext*, const WebString& sinkId, PassOwnPtr<WebSetSinkIdCallbacks>) = 0;

    // Supplement requirements.
    static AudioOutputDeviceClient* from(ExecutionContext*);
    static const char* supplementName();
};

MODULES_EXPORT void provideAudioOutputDeviceClientTo(LocalFrame&, AudioOutputDeviceClient*);

} // namespace blink

#endif // AudioOutputDeviceClient_h
