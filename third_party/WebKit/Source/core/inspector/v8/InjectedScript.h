/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InjectedScript_h
#define InjectedScript_h

#include "core/inspector/v8/InjectedScriptManager.h"
#include "core/inspector/v8/InjectedScriptNative.h"
#include "platform/inspector_protocol/TypeBuilder.h"
#include "wtf/Allocator.h"
#include "wtf/Forward.h"
#include <v8.h>

namespace blink {

class InjectedScriptManager;
class JSONValue;
class RemoteObjectId;
class V8FunctionCall;
class V8DebuggerClient;

typedef String ErrorString;

class InjectedScript final {
    USING_FAST_MALLOC(InjectedScript);
public:
    ~InjectedScript();

    void evaluate(
        ErrorString*,
        const String& expression,
        const String& objectGroup,
        bool includeCommandLineAPI,
        bool returnByValue,
        bool generatePreview,
        RefPtr<protocol::TypeBuilder::Runtime::RemoteObject>* result,
        protocol::TypeBuilder::OptOutput<bool>* wasThrown,
        RefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails>*);
    void callFunctionOn(
        ErrorString*,
        const String& objectId,
        const String& expression,
        const String& arguments,
        bool returnByValue,
        bool generatePreview,
        RefPtr<protocol::TypeBuilder::Runtime::RemoteObject>* result,
        protocol::TypeBuilder::OptOutput<bool>* wasThrown);
    void evaluateOnCallFrame(
        ErrorString*,
        v8::Local<v8::Object> callFrames,
        bool isAsyncCallStack,
        const String& callFrameId,
        const String& expression,
        const String& objectGroup,
        bool includeCommandLineAPI,
        bool returnByValue,
        bool generatePreview,
        RefPtr<protocol::TypeBuilder::Runtime::RemoteObject>* result,
        protocol::TypeBuilder::OptOutput<bool>* wasThrown,
        RefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails>*);
    void restartFrame(ErrorString*, v8::Local<v8::Object> callFrames, const String& callFrameId);
    void getStepInPositions(ErrorString*, v8::Local<v8::Object> callFrames, const String& callFrameId, RefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::Debugger::Location>>& positions);
    void setVariableValue(ErrorString*, v8::Local<v8::Object> callFrames, const String* callFrameIdOpt, const String* functionObjectIdOpt, int scopeNumber, const String& variableName, const String& newValueStr);
    void getFunctionDetails(ErrorString*, const String& functionId, RefPtr<protocol::TypeBuilder::Debugger::FunctionDetails>* result);
    void getGeneratorObjectDetails(ErrorString*, const String& functionId, RefPtr<protocol::TypeBuilder::Debugger::GeneratorObjectDetails>* result);
    void getCollectionEntries(ErrorString*, const String& objectId, RefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::Debugger::CollectionEntry>>* result);
    void getProperties(ErrorString*, const String& objectId, bool ownProperties, bool accessorPropertiesOnly, bool generatePreview, RefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::Runtime::PropertyDescriptor>>* result, RefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails>*);
    void getInternalProperties(ErrorString*, const String& objectId, RefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::Runtime::InternalPropertyDescriptor>>* result, RefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails>*);
    void releaseObject(const String& objectId);

    PassRefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::Debugger::CallFrame>> wrapCallFrames(v8::Local<v8::Object>, int asyncOrdinal);

    PassRefPtr<protocol::TypeBuilder::Runtime::RemoteObject> wrapObject(v8::Local<v8::Value>, const String& groupName, bool generatePreview = false) const;
    PassRefPtr<protocol::TypeBuilder::Runtime::RemoteObject> wrapTable(v8::Local<v8::Value> table, v8::Local<v8::Value> columns) const;
    v8::Local<v8::Value> findObject(const RemoteObjectId&) const;
    String objectGroupName(const RemoteObjectId&) const;
    void releaseObjectGroup(const String&);

    void setCustomObjectFormatterEnabled(bool);
    int contextId() { return m_contextId; }
    String origin() const { return m_origin; }
    void setOrigin(const String& origin) { m_origin = origin; }

    v8::Isolate* isolate() { return m_isolate; }
    v8::Local<v8::Context> context() const;
    void dispose();

private:
    friend InjectedScript* InjectedScriptManager::injectedScriptFor(v8::Local<v8::Context>);
    InjectedScript(InjectedScriptManager*, v8::Local<v8::Context>, v8::Local<v8::Object>, V8DebuggerClient*, PassRefPtr<InjectedScriptNative>, int contextId);

    bool canAccessInspectedWindow() const;
    v8::Local<v8::Value> v8Value() const;
    v8::Local<v8::Value> callFunctionWithEvalEnabled(V8FunctionCall&, bool& hadException) const;
    void makeCall(V8FunctionCall&, RefPtr<JSONValue>* result);
    void makeEvalCall(ErrorString*, V8FunctionCall&, RefPtr<protocol::TypeBuilder::Runtime::RemoteObject>* result, protocol::TypeBuilder::OptOutput<bool>* wasThrown, RefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails>* = 0);
    void makeCallWithExceptionDetails(V8FunctionCall&, RefPtr<JSONValue>* result, RefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails>*);

    InjectedScriptManager* m_manager;
    v8::Isolate* m_isolate;
    v8::Global<v8::Context> m_context;
    v8::Global<v8::Value> m_value;
    V8DebuggerClient* m_client;
    RefPtr<InjectedScriptNative> m_native;
    int m_contextId;
    String m_origin;
};

} // namespace blink

#endif
