// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WebMediaRecorderHandler_h
#define WebMediaRecorderHandler_h

#include "WebCommon.h"

namespace blink {

class WebMediaRecorderHandlerClient;
class WebMediaStream;
class WebString;

// Platform interface of a MediaRecorder.
class BLINK_PLATFORM_EXPORT WebMediaRecorderHandler {
public:
    virtual ~WebMediaRecorderHandler() = default;
    virtual bool initialize(WebMediaRecorderHandlerClient* client, const WebMediaStream& stream, const WebString& mimeType) { return false; }
    virtual bool start() { return false; }
    virtual bool start(int timeslice) { return false; }
    virtual void stop() {}
    virtual void pause() {}
    virtual void resume() {}

    // MediaRecorder API canRecordMimeType() is a tristate in which the returned
    // value 'probably' means that "the user agent is confident that mimeType
    // represents a type that it can record" [1], but a number of reasons might
    // prevent a firm answer at this stage, so a boolean is a better option,
    // because "Implementors are encouraged to return "maybe" unless the type
    // can be confidently established as being supported or not." [1].
    // [1] https://w3c.github.io/mediacapture-record/MediaRecorder.html#methods
    virtual bool canSupportMimeType(const WebString& mimeType) { return false; }
};

} // namespace blink

#endif  // WebMediaRecorderHandler_h
