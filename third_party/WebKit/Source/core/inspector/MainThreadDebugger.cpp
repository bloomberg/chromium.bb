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
#include "bindings/core/v8/V8Window.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectorTaskRunner.h"
#include "core/inspector/v8/V8Debugger.h"
#include "platform/UserGestureIndicator.h"
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

}

// TODO(Oilpan): avoid keeping a raw reference separate from the
// owner one; does not enable heap-movable objects.
MainThreadDebugger* MainThreadDebugger::s_instance = nullptr;

MainThreadDebugger::MainThreadDebugger(PassOwnPtr<ClientMessageLoop> clientMessageLoop, v8::Isolate* isolate)
    : ThreadDebugger(isolate)
    , m_clientMessageLoop(clientMessageLoop)
    , m_taskRunner(adoptPtr(new InspectorTaskRunner(isolate)))
{
    MutexLocker locker(creationMutex());
    ASSERT(!s_instance);
    s_instance = this;
}

MainThreadDebugger::~MainThreadDebugger()
{
    MutexLocker locker(creationMutex());
    ASSERT(s_instance == this);
    s_instance = nullptr;
}

Mutex& MainThreadDebugger::creationMutex()
{
    DEFINE_THREAD_SAFE_STATIC_LOCAL(Mutex, mutex, (new Mutex));
    return mutex;
}

void MainThreadDebugger::initializeContext(v8::Local<v8::Context> context, LocalFrame* frame, int worldId)
{
    String type = worldId == MainWorldId ? "page" : "injected";
    V8Debugger::setContextDebugData(context, type, contextGroupId(frame));
}

int MainThreadDebugger::contextGroupId(LocalFrame* frame)
{
    LocalFrame* localFrameRoot = frame->localFrameRoot();
    return frameId(localFrameRoot);
}

MainThreadDebugger* MainThreadDebugger::instance()
{
    ASSERT(isMainThread());
    return s_instance;
}

void MainThreadDebugger::interruptMainThreadAndRun(PassOwnPtr<InspectorTaskRunner::Task> task)
{
    MutexLocker locker(creationMutex());
    if (s_instance)
        s_instance->m_taskRunner->interruptAndRun(task);
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
    m_clientMessageLoop->run(pausedFrame);
}

void MainThreadDebugger::quitMessageLoopOnPause()
{
    m_clientMessageLoop->quitNow();
}

bool MainThreadDebugger::callingContextCanAccessContext(v8::Local<v8::Context> calling, v8::Local<v8::Context> target)
{
    DOMWindow* window = toDOMWindow(target);
    return window && BindingSecurity::shouldAllowAccessTo(m_isolate, toLocalDOMWindow(toDOMWindow(calling)), window, DoNotReportSecurityError);
}

} // namespace blink
