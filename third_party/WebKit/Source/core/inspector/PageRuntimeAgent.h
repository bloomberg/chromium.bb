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
class Page;
class SecurityOrigin;

class PageRuntimeAgent final : public InspectorRuntimeAgent {
public:
    static PassOwnPtrWillBeRawPtr<PageRuntimeAgent> create(InjectedScriptManager* injectedScriptManager, InspectorClient* client, ScriptDebugServer* scriptDebugServer, Page* page, InspectorPageAgent* pageAgent)
    {
        return adoptPtrWillBeNoop(new PageRuntimeAgent(injectedScriptManager, client, scriptDebugServer, page, pageAgent));
    }
    virtual ~PageRuntimeAgent();
    virtual void trace(Visitor*) override;
    virtual void init() override;
    virtual void enable(ErrorString*) override;
    virtual void run(ErrorString*) override;

    void didClearDocumentOfWindowObject(LocalFrame*);
    void didCreateIsolatedContext(LocalFrame*, ScriptState*, SecurityOrigin*);
    void frameWindowDiscarded(LocalDOMWindow*);

private:
    PageRuntimeAgent(InjectedScriptManager*, InspectorClient*, ScriptDebugServer*, Page*, InspectorPageAgent*);

    virtual InjectedScript injectedScriptForEval(ErrorString*, const int* executionContextId) override;
    virtual void muteConsole() override;
    virtual void unmuteConsole() override;
    void reportExecutionContextCreation();

    InspectorClient* m_client;
    RawPtrWillBeMember<Page> m_inspectedPage;
    RawPtrWillBeMember<InspectorPageAgent> m_pageAgent;
    bool m_mainWorldContextCreated;
};

} // namespace blink


#endif // !defined(InspectorPagerAgent_h)
