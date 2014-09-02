// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PrivateScriptRunner_h
#define PrivateScriptRunner_h

#include "wtf/text/WTFString.h"
#include <v8.h>

namespace blink {

class ExceptionState;
class LocalFrame;
class ScriptState;

class PrivateScriptRunner {
public:
    static v8::Handle<v8::Value> installClassIfNeeded(LocalFrame*, String className);
    static v8::Handle<v8::Value> runDOMAttributeGetter(ScriptState*, String className, String attributeName, v8::Handle<v8::Value> holder);
    static void runDOMAttributeSetter(ScriptState*, String className, String attributeName, v8::Handle<v8::Value> holder, v8::Handle<v8::Value> v8Value);
    static v8::Handle<v8::Value> runDOMMethod(ScriptState*, String className, String methodName, v8::Handle<v8::Value> holder, int argc, v8::Handle<v8::Value> argv[]);

    static void rethrowExceptionInPrivateScript(v8::Isolate*, ExceptionState&, v8::TryCatch&);
};

} // namespace blink

#endif // V8PrivateScriptRunner_h
