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

#ifndef PageRuntimeAgent_h
#define PageRuntimeAgent_h

#include "bindings/core/v8/ScriptState.h"
#include "core/inspector/InspectorRuntimeAgent.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

class InspectorClient;
class InspectorPageAgent;
class SecurityOrigin;

class PageRuntimeAgent final : public InspectorRuntimeAgent {
public:
    class Client {
    public:
        virtual ~Client() { }

        virtual void resumeStartup() { }
    };

    static PassOwnPtrWillBeRawPtr<PageRuntimeAgent> create(InjectedScriptManager* injectedScriptManager, Client* client, ScriptDebugServer* scriptDebugServer, InspectorPageAgent* pageAgent)
    {
        return adoptPtrWillBeNoop(new PageRuntimeAgent(injectedScriptManager, client, scriptDebugServer, pageAgent));
    }
    virtual ~PageRuntimeAgent();
    DECLARE_VIRTUAL_TRACE();
    virtual void init() override;
    virtual void enable(ErrorString*) override;
    virtual void run(ErrorString*) override;

    void didClearDocumentOfWindowObject(LocalFrame*);
    void didCreateScriptContext(LocalFrame*, ScriptState*, SecurityOrigin*, int worldId);
    void willReleaseScriptContext(LocalFrame*, ScriptState*);

private:
    PageRuntimeAgent(InjectedScriptManager*, Client*, ScriptDebugServer*, InspectorPageAgent*);

    virtual InjectedScript injectedScriptForEval(ErrorString*, const int* executionContextId) override;
    virtual void muteConsole() override;
    virtual void unmuteConsole() override;
    void reportExecutionContextCreation();

    Client* m_client;
    RawPtrWillBeMember<InspectorPageAgent> m_pageAgent;
    bool m_mainWorldContextCreated;
    int m_debuggerId;
};

} // namespace blink


#endif // !defined(InspectorPagerAgent_h)
