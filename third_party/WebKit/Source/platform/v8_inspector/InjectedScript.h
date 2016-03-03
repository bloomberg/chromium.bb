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

#include "platform/inspector_protocol/TypeBuilder.h"
#include "platform/v8_inspector/InjectedScriptManager.h"
#include "platform/v8_inspector/InjectedScriptNative.h"
#include <v8.h>

namespace blink {

class InjectedScriptManager;
class RemoteObjectId;
class V8FunctionCall;
class V8DebuggerClient;

namespace protocol {
class DictionaryValue;
}

typedef String ErrorString;

using protocol::Maybe;

class InjectedScript final {
public:
    ~InjectedScript();

    void evaluate(
        ErrorString*,
        const String& expression,
        const String& objectGroup,
        bool includeCommandLineAPI,
        bool returnByValue,
        bool generatePreview,
        OwnPtr<protocol::Runtime::RemoteObject>* result,
        Maybe<bool>* wasThrown,
        Maybe<protocol::Runtime::ExceptionDetails>*);
    void callFunctionOn(
        ErrorString*,
        const String& objectId,
        const String& expression,
        const String& arguments,
        bool returnByValue,
        bool generatePreview,
        OwnPtr<protocol::Runtime::RemoteObject>* result,
        Maybe<bool>* wasThrown);
    void evaluateOnCallFrame(
        ErrorString*,
        v8::Local<v8::Object> callFrames,
        const String& callFrameId,
        const String& expression,
        const String& objectGroup,
        bool includeCommandLineAPI,
        bool returnByValue,
        bool generatePreview,
        OwnPtr<protocol::Runtime::RemoteObject>* result,
        Maybe<bool>* wasThrown,
        Maybe<protocol::Runtime::ExceptionDetails>*);
    void restartFrame(ErrorString*, v8::Local<v8::Object> callFrames, const String& callFrameId);
    void getStepInPositions(ErrorString*, v8::Local<v8::Object> callFrames, const String& callFrameId, Maybe<protocol::Array<protocol::Debugger::Location>>* positions);
    void setVariableValue(ErrorString*, v8::Local<v8::Object> callFrames, const Maybe<String>& callFrameIdOpt, const Maybe<String>& functionObjectIdOpt, int scopeNumber, const String& variableName, const String& newValueStr);
    void getFunctionDetails(ErrorString*, const String& functionId, OwnPtr<protocol::Debugger::FunctionDetails>* result);
    void getGeneratorObjectDetails(ErrorString*, const String& functionId, OwnPtr<protocol::Debugger::GeneratorObjectDetails>* result);
    void getCollectionEntries(ErrorString*, const String& objectId, OwnPtr<protocol::Array<protocol::Debugger::CollectionEntry>>* result);
    void getProperties(ErrorString*, const String& objectId, bool ownProperties, bool accessorPropertiesOnly, bool generatePreview, OwnPtr<protocol::Array<protocol::Runtime::PropertyDescriptor>>* result, Maybe<protocol::Runtime::ExceptionDetails>*);
    void getInternalProperties(ErrorString*, const String& objectId, Maybe<protocol::Array<protocol::Runtime::InternalPropertyDescriptor>>* result, Maybe<protocol::Runtime::ExceptionDetails>*);
    void releaseObject(const String& objectId);
    v8::MaybeLocal<v8::Value> runCompiledScript(v8::Local<v8::Script>, bool includeCommandLineAPI);

    PassOwnPtr<protocol::Array<protocol::Debugger::CallFrame>> wrapCallFrames(v8::Local<v8::Object>);

    PassOwnPtr<protocol::Runtime::RemoteObject> wrapObject(v8::Local<v8::Value>, const String& groupName, bool generatePreview = false) const;
    PassOwnPtr<protocol::Runtime::RemoteObject> wrapTable(v8::Local<v8::Value> table, v8::Local<v8::Value> columns) const;
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
    InjectedScript(InjectedScriptManager*, v8::Local<v8::Context>, v8::Local<v8::Object>, V8DebuggerClient*, PassOwnPtr<InjectedScriptNative>, int contextId);

    bool canAccessInspectedWindow() const;
    v8::Local<v8::Value> v8Value() const;
    v8::Local<v8::Value> callFunctionWithEvalEnabled(V8FunctionCall&, bool& hadException) const;
    PassOwnPtr<protocol::Value> makeCall(V8FunctionCall&);
    PassOwnPtr<protocol::Runtime::RemoteObject> makeEvalCall(ErrorString*, V8FunctionCall&, Maybe<bool>* wasThrown, Maybe<protocol::Runtime::ExceptionDetails>* = 0);
    PassOwnPtr<protocol::Value> makeCallWithExceptionDetails(V8FunctionCall&, Maybe<protocol::Runtime::ExceptionDetails>*);

    InjectedScriptManager* m_manager;
    v8::Isolate* m_isolate;
    v8::Global<v8::Context> m_context;
    v8::Global<v8::Value> m_value;
    V8DebuggerClient* m_client;
    OwnPtr<InjectedScriptNative> m_native;
    int m_contextId;
    String m_origin;
};

} // namespace blink

#endif
