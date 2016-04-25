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

#include "core/inspector/MainThreadDebugger.h"

#include "bindings/core/v8/BindingSecurity.h"
#include "bindings/core/v8/DOMWrapperWorld.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/V8Window.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorTaskRunner.h"
#include "core/timing/MemoryInfo.h"
#include "core/workers/MainThreadWorkletGlobalScope.h"
#include "platform/UserGestureIndicator.h"
#include "platform/v8_inspector/public/V8Debugger.h"
#include "public/platform/Platform.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/ThreadingPrimitives.h"

namespace blink {

namespace {

int frameId(LocalFrame* frame)
{
    ASSERT(frame);
    return WeakIdentifierMap<LocalFrame>::identifier(frame);
}

Mutex& creationMutex()
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, mutex, (new Mutex));
    return mutex;
}

}

// TODO(Oilpan): avoid keeping a raw reference separate from the
// owner one; does not enable heap-movable objects.
MainThreadDebugger* MainThreadDebugger::s_instance = nullptr;

MainThreadDebugger::MainThreadDebugger(v8::Isolate* isolate)
    : ThreadDebugger(isolate)
    , m_taskRunner(adoptPtr(new InspectorTaskRunner()))
{
    MutexLocker locker(creationMutex());
    ASSERT(!s_instance);
    s_instance = this;
    IdentifiersFactory::setProcessId(Platform::current()->getUniqueIdForProcess());
}

MainThreadDebugger::~MainThreadDebugger()
{
    MutexLocker locker(creationMutex());
    ASSERT(s_instance == this);
    s_instance = nullptr;
}

void MainThreadDebugger::setClientMessageLoop(PassOwnPtr<ClientMessageLoop> clientMessageLoop)
{
    ASSERT(!m_clientMessageLoop);
    ASSERT(clientMessageLoop);
    m_clientMessageLoop = std::move(clientMessageLoop);
}

void MainThreadDebugger::contextCreated(ScriptState* scriptState, LocalFrame* frame, SecurityOrigin* origin)
{
    ASSERT(isMainThread());
    v8::HandleScope handles(scriptState->isolate());
    DOMWrapperWorld& world = scriptState->world();
    if (frame->localFrameRoot() == frame && world.isMainWorld())
        debugger()->resetContextGroup(contextGroupId(frame));
    debugger()->contextCreated(V8ContextInfo(scriptState->context(), contextGroupId(frame), world.isMainWorld(), origin ? origin->toRawString() : "", world.isIsolatedWorld() ? world.isolatedWorldHumanReadableName() : "", IdentifiersFactory::frameId(frame), scriptState->getExecutionContext()->isDocument()));
}

void MainThreadDebugger::contextWillBeDestroyed(ScriptState* scriptState)
{
    v8::HandleScope handles(scriptState->isolate());
    debugger()->contextDestroyed(scriptState->context());
}

int MainThreadDebugger::contextGroupId(LocalFrame* frame)
{
    LocalFrame* localFrameRoot = frame->localFrameRoot();
    return frameId(localFrameRoot);
}

MainThreadDebugger* MainThreadDebugger::instance()
{
    ASSERT(isMainThread());
    V8PerIsolateData* data = V8PerIsolateData::from(V8PerIsolateData::mainThreadIsolate());
    ASSERT(data->threadDebugger() && !data->threadDebugger()->isWorker());
    return static_cast<MainThreadDebugger*>(data->threadDebugger());
}

void MainThreadDebugger::interruptMainThreadAndRun(PassOwnPtr<InspectorTaskRunner::Task> task)
{
    MutexLocker locker(creationMutex());
    if (s_instance) {
        s_instance->m_taskRunner->appendTask(std::move(task));
        s_instance->m_taskRunner->interruptAndRunAllTasksDontWait(s_instance->m_isolate);
    }
}

void MainThreadDebugger::runMessageLoopOnPause(int contextGroupId)
{
    LocalFrame* pausedFrame = WeakIdentifierMap<LocalFrame>::lookup(contextGroupId);
    // Do not pause in Context of detached frame.
    if (!pausedFrame)
        return;
    ASSERT(pausedFrame == pausedFrame->localFrameRoot());

    if (UserGestureToken* token = UserGestureIndicator::currentToken())
        token->setPauseInDebugger();
    // Wait for continue or step command.
    if (m_clientMessageLoop)
        m_clientMessageLoop->run(pausedFrame);
}

void MainThreadDebugger::quitMessageLoopOnPause()
{
    if (m_clientMessageLoop)
        m_clientMessageLoop->quitNow();
}

void MainThreadDebugger::muteWarningsAndDeprecations()
{
    FrameConsole::mute();
    UseCounter::muteForInspector();
}

void MainThreadDebugger::unmuteWarningsAndDeprecations()
{
    FrameConsole::unmute();
    UseCounter::unmuteForInspector();
}

void MainThreadDebugger::muteConsole()
{
    FrameConsole::mute();
}

void MainThreadDebugger::unmuteConsole()
{
    FrameConsole::unmute();
}

bool MainThreadDebugger::callingContextCanAccessContext(v8::Local<v8::Context> calling, v8::Local<v8::Context> target)
{
    ExecutionContext* executionContext = toExecutionContext(target);
    ASSERT(executionContext);

    if (executionContext->isWorkletGlobalScope()) {
        MainThreadWorkletGlobalScope* globalScope = toMainThreadWorkletGlobalScope(executionContext);
        return globalScope && BindingSecurity::shouldAllowAccessTo(m_isolate, toLocalDOMWindow(toDOMWindow(calling)), globalScope, DoNotReportSecurityError);
    }

    DOMWindow* window = toDOMWindow(target);
    return window && BindingSecurity::shouldAllowAccessTo(m_isolate, toLocalDOMWindow(toDOMWindow(calling)), window, DoNotReportSecurityError);
}

int MainThreadDebugger::ensureDefaultContextInGroup(int contextGroupId)
{
    LocalFrame* frame = WeakIdentifierMap<LocalFrame>::lookup(contextGroupId);
    if (!frame)
        return 0;
    ScriptState* scriptState = ScriptState::forMainWorld(frame);
    if (!scriptState)
        return 0;
    v8::HandleScope scopes(scriptState->isolate());
    return V8Debugger::contextId(scriptState->context());
}

void MainThreadDebugger::reportMessageToConsole(v8::Local<v8::Context> context, ConsoleMessage* consoleMessage)
{
    ExecutionContext* executionContext = toExecutionContext(context);
    ASSERT(executionContext);
    if (executionContext->isWorkletGlobalScope()) {
        executionContext->addConsoleMessage(consoleMessage);
        return;
    }

    DOMWindow* window = toDOMWindow(context);
    if (!window)
        return;
    LocalDOMWindow* localDomWindow = toLocalDOMWindow(window);
    if (!localDomWindow)
        return;
    LocalFrame* frame = localDomWindow->frame();
    if (!frame)
        return;
    frame->console().addMessage(consoleMessage);
}

v8::MaybeLocal<v8::Value> MainThreadDebugger::memoryInfo(v8::Isolate* isolate, v8::Local<v8::Context> context, v8::Local<v8::Object> creationContext)
{
    ExecutionContext* executionContext = toExecutionContext(context);
    ASSERT_UNUSED(executionContext, executionContext);
    ASSERT(executionContext->isDocument());
    return toV8(MemoryInfo::create(), creationContext, isolate);
}

} // namespace blink
