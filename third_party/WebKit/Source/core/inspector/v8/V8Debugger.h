// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8Debugger_h
#define V8Debugger_h

#include "core/CoreExport.h"
#include "core/InspectorTypeBuilder.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include "wtf/PassOwnPtr.h"

#include <v8-debug.h>
#include <v8.h>

namespace blink {

class JavaScriptCallFrame;
class V8DebuggerClient;
struct ScriptBreakpoint;

class CORE_EXPORT V8Debugger {
    USING_FAST_MALLOC(V8Debugger);
public:
    static PassOwnPtr<V8Debugger> create(v8::Isolate*, V8DebuggerClient*);
    virtual ~V8Debugger() { }

    virtual bool enabled() const = 0;

    // Each v8::Context is a part of a group. The group id is used to find approapriate
    // V8DebuggerAgent to notify about events in the context.
    // |contextGroupId| must be non-0.
    static void setContextDebugData(v8::Local<v8::Context>, const String& type, int contextGroupId);

    static int contextId(v8::Local<v8::Context>);

    enum PauseOnExceptionsState {
        DontPauseOnExceptions,
        PauseOnAllExceptions,
        PauseOnUncaughtExceptions
    };
    virtual PauseOnExceptionsState pauseOnExceptionsState() = 0;
    virtual void setPauseOnExceptionsState(PauseOnExceptionsState) = 0;

    // TODO: remove these methods once runtime agent is part of the implementation.
    virtual void setPauseOnNextStatement(bool) = 0;
    virtual bool pausingOnNextStatement() = 0;

    // TODO: these methods will not be public once InjectedScriptHost is in the implementation.
    virtual v8::MaybeLocal<v8::Value> functionScopes(v8::Local<v8::Function>) = 0;
    virtual v8::Local<v8::Value> generatorObjectDetails(v8::Local<v8::Object>&) = 0;
    virtual v8::Local<v8::Value> collectionEntries(v8::Local<v8::Object>&) = 0;
    virtual v8::MaybeLocal<v8::Value> setFunctionVariableValue(v8::Local<v8::Value> functionValue, int scopeNumber, const String& variableName, v8::Local<v8::Value> newValue) = 0;
};

} // namespace blink


#endif // V8Debugger_h
