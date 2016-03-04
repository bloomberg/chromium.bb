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
#include "platform/v8_inspector/public/V8RuntimeAgent.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

namespace blink {

class InjectedScript;
class InjectedScriptManager;
class ScriptState;
class V8Debugger;

namespace protocol {
class ListValue;
}

typedef String ErrorString;

using protocol::Maybe;

class CORE_EXPORT InspectorRuntimeAgent
    : public InspectorBaseAgent<InspectorRuntimeAgent, protocol::Frontend::Runtime>
    , public protocol::Dispatcher::RuntimeCommandHandler
    , public V8RuntimeAgent::Client {
    WTF_MAKE_NONCOPYABLE(InspectorRuntimeAgent);
public:
    class Client {
    public:
        virtual ~Client() { }

        virtual void resumeStartup() { }
    };

    ~InspectorRuntimeAgent() override;

    // V8RuntimeAgent::Client.
    void reportExecutionContexts() override { }

    // InspectorBaseAgent overrides.
    void setState(protocol::DictionaryValue*) override;
    void setFrontend(protocol::Frontend*) override;
    void clearFrontend() override;
    void restore() override;

    // Part of the protocol.
    void evaluate(ErrorString*, const String& expression, const Maybe<String>& objectGroup, const Maybe<bool>& includeCommandLineAPI, const Maybe<bool>& doNotPauseOnExceptionsAndMuteConsole, const Maybe<int>& contextId, const Maybe<bool>& returnByValue, const Maybe<bool>& generatePreview, OwnPtr<protocol::Runtime::RemoteObject>* result, Maybe<bool>* wasThrown, Maybe<protocol::Runtime::ExceptionDetails>*) override;
    void callFunctionOn(ErrorString*, const String& objectId, const String& functionDeclaration, const Maybe<protocol::Array<protocol::Runtime::CallArgument>>& arguments, const Maybe<bool>& doNotPauseOnExceptionsAndMuteConsole, const Maybe<bool>& returnByValue, const Maybe<bool>& generatePreview, OwnPtr<protocol::Runtime::RemoteObject>* result, Maybe<bool>* wasThrown) override;
    void getProperties(ErrorString*, const String& objectId, const Maybe<bool>& ownProperties, const Maybe<bool>& accessorPropertiesOnly, const Maybe<bool>& generatePreview, OwnPtr<protocol::Array<protocol::Runtime::PropertyDescriptor>>* result, Maybe<protocol::Array<protocol::Runtime::InternalPropertyDescriptor>>* internalProperties, Maybe<protocol::Runtime::ExceptionDetails>*) override;
    void releaseObject(ErrorString*, const String& objectId) override;
    void releaseObjectGroup(ErrorString*, const String& objectGroup) override;
    void run(ErrorString*) override;
    void enable(ErrorString*) override;
    void disable(ErrorString*) override;
    void setCustomObjectFormatterEnabled(ErrorString*, bool enabled) override;
    void compileScript(ErrorString*, const String& expression, const String& sourceURL, bool persistScript, int executionContextId, Maybe<String>* scriptId, Maybe<protocol::Runtime::ExceptionDetails>*) override;
    void runScript(ErrorString*, const String& scriptId, int executionContextId, const Maybe<String>& objectGroup, const Maybe<bool>& doNotPauseOnExceptionsAndMuteConsole, const Maybe<bool>& includeCommandLineAPI, OwnPtr<protocol::Runtime::RemoteObject>* result, Maybe<protocol::Runtime::ExceptionDetails>*) override;

    virtual void muteConsole() = 0;
    virtual void unmuteConsole() = 0;

    V8RuntimeAgent* v8Agent() { return m_v8RuntimeAgent.get(); }

protected:
    InspectorRuntimeAgent(V8Debugger*, Client*);
    virtual ScriptState* defaultScriptState() = 0;

    void reportExecutionContextCreated(ScriptState*, const String& type, const String& origin, const String& humanReadableName, const String& frameId);
    void reportExecutionContextDestroyed(ScriptState*);

    bool m_enabled;
    OwnPtr<V8RuntimeAgent> m_v8RuntimeAgent;
    Client* m_client;
};

} // namespace blink

#endif // InspectorRuntimeAgent_h
