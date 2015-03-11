/*
 * Copyright (c) 2011 Google Inc. All rights reserved.
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

#include "config.h"
#include "bindings/core/v8/PageScriptDebugServer.h"

#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8Binding.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "bindings/core/v8/V8Window.h"
#include "bindings/core/v8/WindowProxy.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/ScriptDebugListener.h"
#include "core/page/Page.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/StdLibExtras.h"
#include "wtf/TemporaryChange.h"
#include "wtf/ThreadingPrimitives.h"
#include "wtf/text/StringBuilder.h"

namespace blink {

static LocalFrame* retrieveFrameWithGlobalObjectCheck(v8::Handle<v8::Context> context)
{
    if (context.IsEmpty())
        return 0;

    // FIXME: This is a temporary hack for crbug.com/345014.
    // Currently it's possible that V8 can trigger Debugger::ProcessDebugEvent for a context
    // that is being initialized (i.e., inside Context::New() of the context).
    // We should fix the V8 side so that it won't trigger the event for a half-baked context
    // because there is no way in the embedder side to check if the context is half-baked or not.
    if (isMainThread() && DOMWrapperWorld::windowIsBeingInitialized())
        return 0;

    v8::Handle<v8::Value> global = V8Window::findInstanceInPrototypeChain(context->Global(), context->GetIsolate());
    if (global.IsEmpty())
        return 0;

    return toLocalFrame(toFrameIfNotDetached(context));
}

PageScriptDebugServer* PageScriptDebugServer::s_instance = nullptr;

PageScriptDebugServer::PageScriptDebugServer(PassOwnPtr<ClientMessageLoop> clientMessageLoop, v8::Isolate* isolate)
    : ScriptDebugServer(isolate)
    , m_clientMessageLoop(clientMessageLoop)
    , m_pausedFrame(nullptr)
{
    MutexLocker locker(creationMutex());
    ASSERT(!s_instance);
    s_instance = this;
}

PageScriptDebugServer::~PageScriptDebugServer()
{
    MutexLocker locker(creationMutex());
    ASSERT(s_instance == this);
    s_instance = nullptr;
}

Mutex& PageScriptDebugServer::creationMutex()
{
    AtomicallyInitializedStaticReference(Mutex, mutex, (new Mutex));
    return mutex;
}

DEFINE_TRACE(PageScriptDebugServer)
{
#if ENABLE(OILPAN)
    visitor->trace(m_listenersMap);
    visitor->trace(m_pausedFrame);
#endif
    ScriptDebugServer::trace(visitor);
}

void PageScriptDebugServer::addListener(ScriptDebugListener* listener, LocalFrame* localFrameRoot)
{
    ASSERT(localFrameRoot == localFrameRoot->localFrameRoot());

    ScriptController& scriptController = localFrameRoot->script();
    if (!scriptController.canExecuteScripts(NotAboutToExecuteScript))
        return;

    v8::HandleScope scope(m_isolate);

    if (!m_listenersMap.size()) {
        v8::Debug::SetDebugEventListener(&PageScriptDebugServer::v8DebugEventCallback, v8::External::New(m_isolate, this));
        ensureDebuggerScriptCompiled();
    }

    v8::Local<v8::Context> debuggerContext = v8::Debug::GetDebugContext();
    v8::Context::Scope contextScope(debuggerContext);

    v8::Local<v8::Object> debuggerScript = m_debuggerScript.newLocal(m_isolate);
    ASSERT(!debuggerScript->IsUndefined());
    m_listenersMap.set(localFrameRoot, listener);

    WindowProxy* windowProxy = scriptController.existingWindowProxy(DOMWrapperWorld::mainWorld());
    if (!windowProxy || !windowProxy->isContextInitialized())
        return;
    v8::Local<v8::Context> context = windowProxy->context();
    v8::Handle<v8::Function> getScriptsFunction = v8::Local<v8::Function>::Cast(debuggerScript->Get(v8AtomicString(m_isolate, "getScripts")));
    v8::Handle<v8::Value> argv[] = { V8PerContextDebugData::contextDebugData(context) };
    v8::Handle<v8::Value> value = V8ScriptRunner::callInternalFunction(getScriptsFunction, debuggerScript, WTF_ARRAY_LENGTH(argv), argv, m_isolate);
    if (value.IsEmpty())
        return;
    ASSERT(!value->IsUndefined() && value->IsArray());
    v8::Handle<v8::Array> scriptsArray = v8::Handle<v8::Array>::Cast(value);
    for (unsigned i = 0; i < scriptsArray->Length(); ++i)
        dispatchDidParseSource(listener, v8::Handle<v8::Object>::Cast(scriptsArray->Get(v8::Integer::New(m_isolate, i))), CompileSuccess);
}

void PageScriptDebugServer::removeListener(ScriptDebugListener* listener, LocalFrame* localFrame)
{
    if (!m_listenersMap.contains(localFrame))
        return;

    if (m_pausedFrame == localFrame)
        continueProgram();

    m_listenersMap.remove(localFrame);

    if (m_listenersMap.isEmpty()) {
        discardDebuggerScript();
        v8::Debug::SetDebugEventListener(0);
        // FIXME: Remove all breakpoints set by the agent.
    }
}

PageScriptDebugServer* PageScriptDebugServer::instance()
{
    ASSERT(isMainThread());
    return s_instance;
}

void PageScriptDebugServer::interruptMainThreadAndRun(PassOwnPtr<Task> task)
{
    MutexLocker locker(creationMutex());
    if (s_instance)
        s_instance->interruptAndRun(task);
}

void PageScriptDebugServer::compileScript(ScriptState* scriptState, const String& expression, const String& sourceURL, bool persistScript, String* scriptId, String* exceptionDetailsText, int* lineNumber, int* columnNumber, RefPtrWillBeRawPtr<ScriptCallStack>* stackTrace)
{
    ExecutionContext* executionContext = scriptState->executionContext();
    RefPtrWillBeRawPtr<LocalFrame> protect(toDocument(executionContext)->frame());
    ScriptDebugServer::compileScript(scriptState, expression, sourceURL, persistScript, scriptId, exceptionDetailsText, lineNumber, columnNumber, stackTrace);
    if (!scriptId->isNull())
        m_compiledScriptURLs.set(*scriptId, sourceURL);
}

void PageScriptDebugServer::clearCompiledScripts()
{
    ScriptDebugServer::clearCompiledScripts();
    m_compiledScriptURLs.clear();
}

void PageScriptDebugServer::runScript(ScriptState* scriptState, const String& scriptId, ScriptValue* result, bool* wasThrown, String* exceptionDetailsText, int* lineNumber, int* columnNumber, RefPtrWillBeRawPtr<ScriptCallStack>* stackTrace)
{
    String sourceURL = m_compiledScriptURLs.take(scriptId);

    ExecutionContext* executionContext = scriptState->executionContext();
    LocalFrame* frame = toDocument(executionContext)->frame();
    TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "EvaluateScript", "data", InspectorEvaluateScriptEvent::data(frame, sourceURL, TextPosition::minimumPosition().m_line.oneBasedInt()));
    InspectorInstrumentationCookie cookie;
    if (frame)
        cookie = InspectorInstrumentation::willEvaluateScript(frame, sourceURL, TextPosition::minimumPosition().m_line.oneBasedInt());

    RefPtrWillBeRawPtr<LocalFrame> protect(frame);
    ScriptDebugServer::runScript(scriptState, scriptId, result, wasThrown, exceptionDetailsText, lineNumber, columnNumber, stackTrace);

    if (frame)
        InspectorInstrumentation::didEvaluateScript(cookie);
    TRACE_EVENT_INSTANT1(TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "UpdateCounters", "data", InspectorUpdateCountersEvent::data());
}

ScriptDebugListener* PageScriptDebugServer::getDebugListenerForContext(v8::Handle<v8::Context> context)
{
    v8::HandleScope scope(m_isolate);
    LocalFrame* frame = retrieveFrameWithGlobalObjectCheck(context);
    if (!frame)
        return 0;
    return m_listenersMap.get(frame->localFrameRoot());
}

void PageScriptDebugServer::runMessageLoopOnPause(v8::Handle<v8::Context> context)
{
    v8::HandleScope scope(m_isolate);
    LocalFrame* frame = retrieveFrameWithGlobalObjectCheck(context);
    m_pausedFrame = frame->localFrameRoot();

    // Wait for continue or step command.
    m_clientMessageLoop->run(m_pausedFrame);

    // The listener may have been removed in the nested loop.
    if (ScriptDebugListener* listener = m_listenersMap.get(m_pausedFrame))
        listener->didContinue();

    m_pausedFrame = 0;
}

void PageScriptDebugServer::quitMessageLoopOnPause()
{
    m_clientMessageLoop->quitNow();
}

void PageScriptDebugServer::muteWarningsAndDeprecations()
{
    FrameConsole::mute();
    UseCounter::muteForInspector();
}

void PageScriptDebugServer::unmuteWarningsAndDeprecations()
{
    FrameConsole::unmute();
    UseCounter::unmuteForInspector();
}

} // namespace blink
