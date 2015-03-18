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

#ifndef PageDebuggerAgent_h
#define PageDebuggerAgent_h

#include "bindings/core/v8/PageScriptDebugServer.h"
#include "core/inspector/InspectorDebuggerAgent.h"
#include "core/inspector/InspectorOverlay.h"

namespace blink {

class DocumentLoader;
class InspectorPageAgent;
class PageScriptDebugServer;

class PageDebuggerAgent final
    : public InspectorDebuggerAgent
    , public InspectorOverlay::Listener {
    WTF_MAKE_NONCOPYABLE(PageDebuggerAgent);
    WTF_MAKE_FAST_ALLOCATED_WILL_BE_REMOVED;
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(PageDebuggerAgent);
public:
    static PassOwnPtrWillBeRawPtr<PageDebuggerAgent> create(PageScriptDebugServer*, InspectorPageAgent*, InjectedScriptManager*, InspectorOverlay*, int debuggerId);
    ~PageDebuggerAgent() override;
    DECLARE_VIRTUAL_TRACE();

    void didStartProvisionalLoad(LocalFrame*);
    void didClearDocumentOfWindowObject(LocalFrame*);
    void didCommitLoadForLocalFrame(LocalFrame*) override;

protected:
    void enable() override;
    void disable() override;

private:
    void startListeningScriptDebugServer() override;
    void stopListeningScriptDebugServer() override;
    PageScriptDebugServer& scriptDebugServer() override;
    void muteConsole() override;
    void unmuteConsole() override;

    // InspectorOverlay::Listener implementation.
    void overlayResumed() override;
    void overlaySteppedOver() override;

    InjectedScript injectedScriptForEval(ErrorString*, const int* executionContextId) override;

    PageDebuggerAgent(PageScriptDebugServer*, InspectorPageAgent*, InjectedScriptManager*, InspectorOverlay*, int debuggerId);
    RawPtrWillBeMember<PageScriptDebugServer> m_pageScriptDebugServer;
    RawPtrWillBeMember<InspectorPageAgent> m_pageAgent;
    RawPtrWillBeMember<InspectorOverlay> m_overlay;
    int m_debuggerId;
};

} // namespace blink


#endif // !defined(PageDebuggerAgent_h)
