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

#include "core/inspector/PageDebuggerAgent.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptController.h"
#include "core/dom/Document.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/AsyncCallTracker.h"
#include "core/inspector/InjectedScript.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/InstrumentingAgents.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/Page.h"

using blink::TypeBuilder::Debugger::ExceptionDetails;
using blink::TypeBuilder::Debugger::ScriptId;
using blink::TypeBuilder::Runtime::RemoteObject;

namespace blink {

PassOwnPtrWillBeRawPtr<PageDebuggerAgent> PageDebuggerAgent::create(MainThreadDebugger* mainThreadDebugger, InspectedFrames* inspectedFrames, InjectedScriptManager* injectedScriptManager)
{
    return adoptPtrWillBeNoop(new PageDebuggerAgent(mainThreadDebugger, inspectedFrames, injectedScriptManager));
}

PageDebuggerAgent::PageDebuggerAgent(MainThreadDebugger* mainThreadDebugger, InspectedFrames* inspectedFrames, InjectedScriptManager* injectedScriptManager)
    : InspectorDebuggerAgent(injectedScriptManager, mainThreadDebugger->debugger(), mainThreadDebugger->contextGroupId(inspectedFrames->root()))
    , m_inspectedFrames(inspectedFrames)
    , m_injectedScriptManager(injectedScriptManager)
{
}

PageDebuggerAgent::~PageDebuggerAgent()
{
}

DEFINE_TRACE(PageDebuggerAgent)
{
    visitor->trace(m_inspectedFrames);
    visitor->trace(m_injectedScriptManager);
    InspectorDebuggerAgent::trace(visitor);
}

bool PageDebuggerAgent::canExecuteScripts() const
{
    ScriptController& scriptController = m_inspectedFrames->root()->script();
    return scriptController.canExecuteScripts(NotAboutToExecuteScript);
}

void PageDebuggerAgent::enable(ErrorString* errorString)
{
    if (!canExecuteScripts()) {
        *errorString = "Script execution is prohibited";
        return;
    }
    InspectorDebuggerAgent::enable(errorString);
    m_instrumentingAgents->setPageDebuggerAgent(this);
}

void PageDebuggerAgent::disable(ErrorString* errorString)
{
    m_instrumentingAgents->setPageDebuggerAgent(nullptr);
    m_compiledScriptURLs.clear();
    InspectorDebuggerAgent::disable(errorString);
}

void PageDebuggerAgent::restore()
{
    if (canExecuteScripts())
        InspectorDebuggerAgent::restore();
}


void PageDebuggerAgent::muteConsole()
{
    FrameConsole::mute();
}

void PageDebuggerAgent::unmuteConsole()
{
    FrameConsole::unmute();
}

void PageDebuggerAgent::didStartProvisionalLoad(LocalFrame* frame)
{
    if (frame == m_inspectedFrames->root()) {
        ErrorString error;
        resume(&error);
    }
}

void PageDebuggerAgent::didClearDocumentOfWindowObject(LocalFrame* frame)
{
    // FIXME: what about nested objects?
    if (frame != m_inspectedFrames->root())
        return;
    m_asyncCallTracker->resetAsyncOperations();
    m_v8DebuggerAgent->reset();
}

void PageDebuggerAgent::compileScript(ErrorString* errorString, const String& expression, const String& sourceURL, bool persistScript, int executionContextId, TypeBuilder::OptOutput<ScriptId>* scriptId, RefPtr<ExceptionDetails>& exceptionDetails)
{
    InjectedScript injectedScript = m_injectedScriptManager->findInjectedScript(executionContextId);
    if (injectedScript.isEmpty()) {
        *errorString = "Inspected frame has gone";
        return;
    }
    ExecutionContext* executionContext = injectedScript.scriptState()->executionContext();
    RefPtrWillBeRawPtr<LocalFrame> protect(toDocument(executionContext)->frame());
    InspectorDebuggerAgent::compileScript(errorString, expression, sourceURL, persistScript, executionContextId, scriptId, exceptionDetails);
    if (!scriptId->isAssigned())
        return;

    String scriptIdValue = scriptId->getValue();
    if (!scriptIdValue.isEmpty())
        m_compiledScriptURLs.set(scriptId->getValue(), sourceURL);
}

void PageDebuggerAgent::runScript(ErrorString* errorString, const ScriptId& scriptId, int executionContextId, const String* const objectGroup, const bool* const doNotPauseOnExceptionsAndMuteConsole, RefPtr<RemoteObject>& result, RefPtr<ExceptionDetails>& exceptionDetails)
{
    InjectedScript injectedScript = m_injectedScriptManager->findInjectedScript(executionContextId);
    if (injectedScript.isEmpty()) {
        *errorString = "Inspected frame has gone";
        return;
    }
    ExecutionContext* executionContext = injectedScript.scriptState()->executionContext();

    String sourceURL = m_compiledScriptURLs.take(scriptId);
    LocalFrame* frame = toDocument(executionContext)->frame();
    TRACE_EVENT1("devtools.timeline", "EvaluateScript", "data", InspectorEvaluateScriptEvent::data(frame, sourceURL, TextPosition::minimumPosition()));

    RefPtrWillBeRawPtr<LocalFrame> protect(frame);
    InspectorDebuggerAgent::runScript(errorString, scriptId, executionContextId, objectGroup, doNotPauseOnExceptionsAndMuteConsole, result, exceptionDetails);

    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "UpdateCounters", TRACE_EVENT_SCOPE_THREAD, "data", InspectorUpdateCountersEvent::data());
}

} // namespace blink
