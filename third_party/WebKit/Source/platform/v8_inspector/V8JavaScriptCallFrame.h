// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8JavaScriptCallFrame_h
#define V8JavaScriptCallFrame_h

#include "platform/inspector_protocol/Allocator.h"
#include "wtf/PassRefPtr.h"
#include <v8.h>

namespace blink {

class JavaScriptCallFrame;
class V8DebuggerClient;

class V8JavaScriptCallFrame {
public:
    static v8::Local<v8::FunctionTemplate> createWrapperTemplate(v8::Isolate*);
    static v8::Local<v8::Object> wrap(V8DebuggerClient*, v8::Local<v8::FunctionTemplate> constructorTemplate, v8::Local<v8::Context>, PassRefPtr<JavaScriptCallFrame>);
    static JavaScriptCallFrame* unwrap(v8::Local<v8::Context>, v8::Local<v8::Object>);
};

} // namespace blink

#endif // V8JavaScriptCallFrame_h
