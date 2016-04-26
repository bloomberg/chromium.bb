// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8InjectedScriptHost_h
#define V8InjectedScriptHost_h

#include <v8.h>

namespace blink {

class InjectedScriptHost;
class V8DebuggerClient;

class V8InjectedScriptHost {
public:
    static v8::Local<v8::Object> wrap(v8::Local<v8::FunctionTemplate> constructorTemplate, v8::Local<v8::Context>, InjectedScriptHost*);
    static InjectedScriptHost* unwrap(v8::Local<v8::Context>, v8::Local<v8::Object>);
    static v8::Local<v8::FunctionTemplate> createWrapperTemplate(v8::Isolate*);

    static void inspectCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void inspectedObjectCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void internalConstructorNameCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void formatAccessorsAsProperties(const v8::FunctionCallbackInfo<v8::Value>&);
    static void isTypedArrayCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void subtypeCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void generatorObjectDetailsCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void collectionEntriesCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void getInternalPropertiesCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void getEventListenersCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void callFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void suppressWarningsAndCallFunctionCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void setNonEnumPropertyCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void setFunctionVariableValueCallback(const v8::FunctionCallbackInfo<v8::Value>&);
    static void bindCallback(const v8::FunctionCallbackInfo<v8::Value>&);
};

} // namespace blink

#endif // V8InjectedScriptHost_h
