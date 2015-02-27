/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InspectorConsoleAgent_h
#define InspectorConsoleAgent_h

#include "core/InspectorFrontend.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/Forward.h"
#include "wtf/Noncopyable.h"

namespace blink {

class ConsoleMessage;
class ConsoleMessageStorage;
class InjectedScriptManager;

typedef String ErrorString;

class InspectorConsoleAgent : public InspectorBaseAgent<InspectorConsoleAgent>, public InspectorBackendDispatcher::ConsoleCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorConsoleAgent);
public:
    explicit InspectorConsoleAgent(InjectedScriptManager*);
    virtual ~InspectorConsoleAgent();
    DECLARE_VIRTUAL_TRACE();

    virtual void enable(ErrorString*) override;
    virtual void disable(ErrorString*) override;
    bool enabled() { return m_enabled; }

    virtual void setFrontend(InspectorFrontend*) override final;
    virtual void clearFrontend() override final;
    virtual void restore() override final;

    void addMessageToConsole(ConsoleMessage*);
    void consoleMessagesCleared();

    virtual void setLastEvaluationResult(ErrorString*, const String& objectId) override;

protected:
    void sendConsoleMessageToFrontend(ConsoleMessage*, bool generatePreview);
    virtual ConsoleMessageStorage* messageStorage() = 0;

    virtual void enableStackCapturingIfNeeded() = 0;
    virtual void disableStackCapturingIfNeeded() = 0;

    RawPtrWillBeMember<InjectedScriptManager> m_injectedScriptManager;
    InspectorFrontend::Console* m_frontend;
    bool m_enabled;
};

} // namespace blink


#endif // !defined(InspectorConsoleAgent_h)
