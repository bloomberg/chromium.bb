/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef V8RuntimeAgentImpl_h
#define V8RuntimeAgentImpl_h

#include "core/CoreExport.h"
#include "core/inspector/v8/public/V8RuntimeAgent.h"
#include "platform/inspector_protocol/Frontend.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

namespace blink {

class InjectedScriptManager;
class JSONArray;
class V8DebuggerImpl;

typedef String ErrorString;

class CORE_EXPORT V8RuntimeAgentImpl : public V8RuntimeAgent {
    WTF_MAKE_NONCOPYABLE(V8RuntimeAgentImpl);
public:
    V8RuntimeAgentImpl(V8DebuggerImpl*);
    ~V8RuntimeAgentImpl() override;

    // State management methods.
    void setInspectorState(PassRefPtr<JSONObject>) override;
    void setFrontend(protocol::Frontend::Runtime*) override;
    void clearFrontend() override;
    void restore() override;

    // Part of the protocol.
    void enable(ErrorString*) override;
    void disable(ErrorString*) override;
    void evaluate(ErrorString*,
        const String& expression,
        const String* objectGroup,
        const bool* includeCommandLineAPI,
        const bool* doNotPauseOnExceptionsAndMuteConsole,
        const int* executionContextId,
        const bool* returnByValue,
        const bool* generatePreview,
        RefPtr<protocol::TypeBuilder::Runtime::RemoteObject>& result,
        protocol::TypeBuilder::OptOutput<bool>* wasThrown,
        RefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails>&) override;
    void callFunctionOn(ErrorString*,
        const String& objectId,
        const String& expression,
        const RefPtr<JSONArray>* optionalArguments,
        const bool* doNotPauseOnExceptionsAndMuteConsole,
        const bool* returnByValue,
        const bool* generatePreview,
        RefPtr<protocol::TypeBuilder::Runtime::RemoteObject>& result,
        protocol::TypeBuilder::OptOutput<bool>* wasThrown) override;
    void releaseObject(ErrorString*, const String& objectId) override;
    void getProperties(ErrorString*, const String& objectId, const bool* ownProperties, const bool* accessorPropertiesOnly, const bool* generatePreview, RefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::Runtime::PropertyDescriptor>>& result, RefPtr<protocol::TypeBuilder::Array<protocol::TypeBuilder::Runtime::InternalPropertyDescriptor>>& internalProperties, RefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails>&) override;
    void releaseObjectGroup(ErrorString*, const String& objectGroup) override;
    void run(ErrorString*) override;
    void isRunRequired(ErrorString*, bool* out_result) override;
    void setCustomObjectFormatterEnabled(ErrorString*, bool) override;
    void compileScript(ErrorString*, const String& expression, const String& sourceURL, bool persistScript, int executionContextId, protocol::TypeBuilder::OptOutput<protocol::TypeBuilder::Runtime::ScriptId>*, RefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails>&) override;
    void runScript(ErrorString*, const protocol::TypeBuilder::Runtime::ScriptId&, int executionContextId, const String* objectGroup, const bool* doNotPauseOnExceptionsAndMuteConsole, RefPtr<protocol::TypeBuilder::Runtime::RemoteObject>& result, RefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails>&) override;

    V8DebuggerImpl* debugger() { return m_debugger; }
    InjectedScriptManager* injectedScriptManager() { return m_injectedScriptManager.get(); }

private:
    void setClearConsoleCallback(PassOwnPtr<ClearConsoleCallback>) override;
    void setInspectObjectCallback(PassOwnPtr<InspectCallback>) override;
    int ensureDefaultContextAvailable(v8::Local<v8::Context>) override;
    PassRefPtr<protocol::TypeBuilder::Runtime::RemoteObject> wrapObject(v8::Local<v8::Context>, v8::Local<v8::Value>, const String& groupName, bool generatePreview = false) override;
    PassRefPtr<protocol::TypeBuilder::Runtime::RemoteObject> wrapTable(v8::Local<v8::Context>, v8::Local<v8::Value> table, v8::Local<v8::Value> columns) override;
    void disposeObjectGroup(const String&) override;
    v8::Local<v8::Value> findObject(const String& objectId, v8::Local<v8::Context>* = nullptr, String* groupName = nullptr) override;
    void addInspectedObject(PassOwnPtr<Inspectable>) override;
    void clearInspectedObjects() override;

    void reportExecutionContextCreated(v8::Local<v8::Context>, const String& type, const String& origin, const String& humanReadableName, const String& frameId) override;
    void reportExecutionContextDestroyed(v8::Local<v8::Context>) override;
    PassRefPtr<protocol::TypeBuilder::Runtime::ExceptionDetails> createExceptionDetails(v8::Isolate*, v8::Local<v8::Message>);

    RefPtr<JSONObject> m_state;
    protocol::Frontend::Runtime* m_frontend;
    OwnPtr<InjectedScriptManager> m_injectedScriptManager;
    V8DebuggerImpl* m_debugger;
    bool m_enabled;
    HashMap<String, OwnPtr<v8::Global<v8::Script>>> m_compiledScripts;
};

} // namespace blink

#endif // V8RuntimeAgentImpl_h
