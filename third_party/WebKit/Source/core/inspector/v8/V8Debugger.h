// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8Debugger_h
#define V8Debugger_h

#include "core/CoreExport.h"
#include "core/InspectorTypeBuilder.h"
#include "core/inspector/v8/V8DebuggerListener.h"
#include "wtf/Forward.h"
#include "wtf/PassOwnPtr.h"

#include <v8-debug.h>
#include <v8.h>

namespace blink {

class JavaScriptCallFrame;
class V8DebuggerClient;
struct ScriptBreakpoint;

class CORE_EXPORT V8Debugger {
public:
    static PassOwnPtr<V8Debugger> create(v8::Isolate*, V8DebuggerClient*);
    virtual ~V8Debugger() { }

    virtual void enable() = 0;
    virtual void disable() = 0;
    virtual bool enabled() const = 0;

    static void setContextDebugData(v8::Local<v8::Context>, const String& contextDebugData);
    // Each script inherits debug data from v8::Context where it has been compiled.
    // Only scripts whose debug data contains |contextDebugDataSubstring| substring will be reported.
    // Passing empty string will result in reporting all scripts.
    virtual void getCompiledScripts(const String& contextDebugDataSubstring, Vector<V8DebuggerListener::ParsedScript>&) = 0;

    virtual String setBreakpoint(const String& sourceID, const ScriptBreakpoint&, int* actualLineNumber, int* actualColumnNumber, bool interstatementLocation) = 0;
    virtual void removeBreakpoint(const String& breakpointId) = 0;
    virtual void setBreakpointsActivated(bool) = 0;

    enum PauseOnExceptionsState {
        DontPauseOnExceptions,
        PauseOnAllExceptions,
        PauseOnUncaughtExceptions
    };
    virtual PauseOnExceptionsState pauseOnExceptionsState() = 0;
    virtual void setPauseOnExceptionsState(PauseOnExceptionsState) = 0;

    virtual void setPauseOnNextStatement(bool) = 0;
    virtual bool pausingOnNextStatement() = 0;
    virtual bool canBreakProgram() = 0;
    virtual void breakProgram() = 0;
    virtual void continueProgram() = 0;
    virtual void stepIntoStatement() = 0;
    virtual void stepOverStatement() = 0;
    virtual void stepOutOfFunction() = 0;
    virtual void clearStepping() = 0;

    virtual bool setScriptSource(const String& sourceID, const String& newContent, bool preview, String* error, RefPtr<TypeBuilder::Debugger::SetScriptSourceError>&, v8::Global<v8::Object>* newCallFrames, RefPtr<JSONObject>* result) = 0;
    virtual v8::Local<v8::Object> currentCallFrames() = 0;
    virtual v8::Local<v8::Object> currentCallFramesForAsyncStack() = 0;
    virtual PassRefPtr<JavaScriptCallFrame> callFrameNoScopes(int index) = 0;
    virtual int frameCount() = 0;

    virtual bool isPaused() = 0;

    virtual v8::Local<v8::Value> functionScopes(v8::Local<v8::Function>) = 0;
    virtual v8::Local<v8::Value> generatorObjectDetails(v8::Local<v8::Object>&) = 0;
    virtual v8::Local<v8::Value> collectionEntries(v8::Local<v8::Object>&) = 0;
    virtual v8::MaybeLocal<v8::Value> setFunctionVariableValue(v8::Local<v8::Value> functionValue, int scopeNumber, const String& variableName, v8::Local<v8::Value> newValue) = 0;

    virtual v8::Isolate* isolate() const = 0;
};

} // namespace blink


#endif // V8Debugger_h
