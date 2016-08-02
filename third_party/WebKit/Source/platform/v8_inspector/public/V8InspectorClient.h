// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8InspectorClient_h
#define V8InspectorClient_h

#include "platform/inspector_protocol/Platform.h"
#include "platform/inspector_protocol/String16.h"

#include <v8.h>

namespace blink {

class V8StackTrace;

enum class V8ConsoleAPIType { kClear, kDebug, kLog, kInfo, kWarning, kError };

class PLATFORM_EXPORT V8InspectorClient {
public:
    virtual ~V8InspectorClient() { }
    virtual void runMessageLoopOnPause(int contextGroupId) = 0;
    virtual void quitMessageLoopOnPause() = 0;
    virtual void muteWarningsAndDeprecations(int contextGroupId) = 0;
    virtual void unmuteWarningsAndDeprecations(int contextGroupId) = 0;
    virtual void beginUserGesture() = 0;
    virtual void endUserGesture() = 0;
    virtual String16 valueSubtype(v8::Local<v8::Value>) = 0;
    virtual bool formatAccessorsAsProperties(v8::Local<v8::Value>) = 0;
    virtual bool isExecutionAllowed() = 0;
    virtual double currentTimeMS() = 0;
    virtual v8::Local<v8::Context> ensureDefaultContextInGroup(int contextGroupId) = 0;
    virtual bool isInspectableHeapObject(v8::Local<v8::Object>) = 0;
    virtual void beginEnsureAllContextsInGroup(int contextGroupId) = 0;
    virtual void endEnsureAllContextsInGroup(int contextGroupId) = 0;
    // TODO(dgozman): this was added to support service worker shadow page. We should not connect at all.
    virtual bool canExecuteScripts(int contextGroupId) = 0;

    virtual void installAdditionalCommandLineAPI(v8::Local<v8::Context>, v8::Local<v8::Object>) = 0;

    virtual void consoleTime(const String16& title) = 0;
    virtual void consoleTimeEnd(const String16& title) = 0;
    virtual void consoleTimeStamp(const String16& title) = 0;
    virtual void consoleAPIMessage(int contextGroupId, V8ConsoleAPIType, const String16& message, const String16& url, unsigned lineNumber, unsigned columnNumber, V8StackTrace*) = 0;

    virtual v8::MaybeLocal<v8::Value> memoryInfo(v8::Isolate*, v8::Local<v8::Context>) = 0;

    typedef void (*TimerCallback)(void*);
    virtual void startRepeatingTimer(double, TimerCallback, void* data) = 0;
    virtual void cancelTimer(void* data) = 0;
};

} // namespace blink


#endif // V8InspectorClient_h
