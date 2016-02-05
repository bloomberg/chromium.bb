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
#include "core/InspectorBackendDispatcher.h"
#include "core/InspectorFrontend.h"
#include "core/inspector/v8/V8RuntimeAgent.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

namespace blink {

class InjectedScript;
class InjectedScriptManager;
class JSONArray;
class V8DebuggerImpl;

typedef String ErrorString;

class CORE_EXPORT V8RuntimeAgentImpl : public V8RuntimeAgent, public InspectorBackendDispatcher::RuntimeCommandHandler {
    WTF_MAKE_NONCOPYABLE(V8RuntimeAgentImpl);
public:
    V8RuntimeAgentImpl(InjectedScriptManager*, V8DebuggerImpl*);
    ~V8RuntimeAgentImpl() override;

    // State management methods.
    void setInspectorState(PassRefPtr<JSONObject>) override;
    void setFrontend(InspectorFrontend::Runtime*) override;
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
        RefPtr<TypeBuilder::Runtime::RemoteObject>& result,
        TypeBuilder::OptOutput<bool>* wasThrown,
        RefPtr<TypeBuilder::Runtime::ExceptionDetails>&) final;
    void callFunctionOn(ErrorString*,
        const String& objectId,
        const String& expression,
        const RefPtr<JSONArray>* optionalArguments,
        const bool* doNotPauseOnExceptionsAndMuteConsole,
        const bool* returnByValue,
        const bool* generatePreview,
        RefPtr<TypeBuilder::Runtime::RemoteObject>& result,
        TypeBuilder::OptOutput<bool>* wasThrown) final;
    void releaseObject(ErrorString*, const String& objectId) final;
    void getProperties(ErrorString*, const String& objectId, const bool* ownProperties, const bool* accessorPropertiesOnly, const bool* generatePreview, RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::PropertyDescriptor>>& result, RefPtr<TypeBuilder::Array<TypeBuilder::Runtime::InternalPropertyDescriptor>>& internalProperties, RefPtr<TypeBuilder::Runtime::ExceptionDetails>&) final;
    void releaseObjectGroup(ErrorString*, const String& objectGroup) final;
    void run(ErrorString*) override;
    void isRunRequired(ErrorString*, bool* out_result) override;
    void setCustomObjectFormatterEnabled(ErrorString*, bool) final;
    void compileScript(ErrorString*, const String& expression, const String& sourceURL, bool persistScript, int executionContextId, TypeBuilder::OptOutput<TypeBuilder::Runtime::ScriptId>*, RefPtr<TypeBuilder::Runtime::ExceptionDetails>&) override;
    void runScript(ErrorString*, const TypeBuilder::Runtime::ScriptId&, int executionContextId, const String* objectGroup, const bool* doNotPauseOnExceptionsAndMuteConsole, RefPtr<TypeBuilder::Runtime::RemoteObject>& result, RefPtr<TypeBuilder::Runtime::ExceptionDetails>&) override;

    V8DebuggerImpl* debugger() { return m_debugger; }

private:
    InjectedScriptManager* injectedScriptManager() { return m_injectedScriptManager; }
    void reportExecutionContextCreated(v8::Local<v8::Context>, const String& type, const String& origin, const String& humanReadableName, const String& frameId) override;
    void reportExecutionContextDestroyed(v8::Local<v8::Context>) override;
    PassRefPtr<TypeBuilder::Runtime::ExceptionDetails> createExceptionDetails(v8::Isolate*, v8::Local<v8::Message>);

    RefPtr<JSONObject> m_state;
    InspectorFrontend::Runtime* m_frontend;
    InjectedScriptManager* m_injectedScriptManager;
    V8DebuggerImpl* m_debugger;
    bool m_enabled;
    HashMap<String, OwnPtr<v8::Global<v8::Script>>> m_compiledScripts;
};

} // namespace blink

#endif // V8RuntimeAgentImpl_h
