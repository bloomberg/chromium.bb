// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8Console_h
#define V8Console_h

#include <v8.h>

namespace blink {

class InspectedContext;

// Console API
// https://console.spec.whatwg.org/#console-interface
class V8Console {
public:
    static v8::MaybeLocal<v8::Object> create(v8::Local<v8::Context>, InspectedContext*, bool hasMemoryAttribute);
    static void clearInspectedContextIfNeeded(v8::Local<v8::Context>, v8::Local<v8::Object> console);

    static void debugCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void errorCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void infoCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void logCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void warnCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void dirCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void dirxmlCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void tableCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void traceCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void groupCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void groupCollapsedCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void groupEndCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void clearCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void countCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void assertCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void markTimelineCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void profileCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void profileEndCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void timelineCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void timelineEndCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void timeCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void timeEndCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void timeStampCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    // TODO(philipj): There is no spec for the Memory Info API, see blink-dev:
    // https://groups.google.com/a/chromium.org/d/msg/blink-dev/g5YRCGpC9vs/b4OJz71NmPwJ
    static void memoryGetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void memorySetterCallback(const v8::FunctionCallbackInfo<v8::Value>&);
};

} // namespace blink

#endif // V8Console_h
