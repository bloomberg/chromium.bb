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

#ifndef InspectorRuntimeAgent_h
#define InspectorRuntimeAgent_h

#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

namespace blink {

class InjectedScript;
class InjectedScriptManager;
class ScriptState;
class V8RuntimeAgent;

namespace protocol {
class ListValue;
}

using protocol::Maybe;

class CORE_EXPORT InspectorRuntimeAgent
    : public InspectorBaseAgent<InspectorRuntimeAgent, protocol::Frontend::Runtime>
    , public protocol::Backend::Runtime {
    WTF_MAKE_NONCOPYABLE(InspectorRuntimeAgent);
public:
    explicit InspectorRuntimeAgent(V8RuntimeAgent*);
    ~InspectorRuntimeAgent() override;

    // InspectorBaseAgent overrides.
    void init(InstrumentingAgents*, protocol::Frontend*, protocol::Dispatcher*, protocol::DictionaryValue*) override;
    void dispose() override;
    void restore() override;

    // Part of the protocol.
    void evaluate(ErrorString*, const String16& expression, const Maybe<String16>& objectGroup, const Maybe<bool>& includeCommandLineAPI, const Maybe<bool>& doNotPauseOnExceptionsAndMuteConsole, const Maybe<int>& contextId, const Maybe<bool>& returnByValue, const Maybe<bool>& generatePreview, const Maybe<bool>& userGesture, OwnPtr<protocol::Runtime::RemoteObject>* result, Maybe<bool>* wasThrown, Maybe<protocol::Runtime::ExceptionDetails>*) override;
    void callFunctionOn(ErrorString*, const String16& objectId, const String16& functionDeclaration, const Maybe<protocol::Array<protocol::Runtime::CallArgument>>& arguments, const Maybe<bool>& doNotPauseOnExceptionsAndMuteConsole, const Maybe<bool>& returnByValue, const Maybe<bool>& generatePreview, const Maybe<bool>& userGesture, OwnPtr<protocol::Runtime::RemoteObject>* result, Maybe<bool>* wasThrown) override;
    void getProperties(ErrorString*, const String16& objectId, const Maybe<bool>& ownProperties, const Maybe<bool>& accessorPropertiesOnly, const Maybe<bool>& generatePreview, OwnPtr<protocol::Array<protocol::Runtime::PropertyDescriptor>>* result, Maybe<protocol::Array<protocol::Runtime::InternalPropertyDescriptor>>* internalProperties, Maybe<protocol::Runtime::ExceptionDetails>*) override;
    void releaseObject(ErrorString*, const String16& objectId) override;
    void releaseObjectGroup(ErrorString*, const String16& objectGroup) override;
    void run(ErrorString*) override;
    void enable(ErrorString*) override;
    void disable(ErrorString*) override;
    void setCustomObjectFormatterEnabled(ErrorString*, bool enabled) override;
    void compileScript(ErrorString*, const String16& expression, const String16& sourceURL, bool persistScript, int executionContextId, Maybe<String16>* scriptId, Maybe<protocol::Runtime::ExceptionDetails>*) override;
    void runScript(ErrorString*, const String16& scriptId, int executionContextId, const Maybe<String16>& objectGroup, const Maybe<bool>& doNotPauseOnExceptionsAndMuteConsole, const Maybe<bool>& includeCommandLineAPI, OwnPtr<protocol::Runtime::RemoteObject>* result, Maybe<protocol::Runtime::ExceptionDetails>*) override;

protected:
    V8RuntimeAgent* m_v8RuntimeAgent;
};

} // namespace blink

#endif // InspectorRuntimeAgent_h
