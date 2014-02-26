/*
 * Copyright (C) 2009, 2012 Google Inc. All rights reserved.
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

#include "bindings/v8/WorkerScriptController.h"

#include "V8DedicatedWorkerGlobalScope.h"
#include "V8ServiceWorkerGlobalScope.h"
#include "V8SharedWorkerGlobalScope.h"
#include "V8WorkerGlobalScope.h"
#include "bindings/v8/ScriptSourceCode.h"
#include "bindings/v8/ScriptValue.h"
#include "bindings/v8/V8ErrorHandler.h"
#include "bindings/v8/V8GCController.h"
#include "bindings/v8/V8Initializer.h"
#include "bindings/v8/V8ObjectConstructor.h"
#include "bindings/v8/V8ScriptRunner.h"
#include "bindings/v8/WrapperTypeInfo.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/frame/DOMTimer.h"
#include "core/workers/SharedWorkerGlobalScope.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerObjectProxy.h"
#include "core/workers/WorkerThread.h"
#include "heap/ThreadState.h"
#include <v8.h>

#include "public/platform/Platform.h"
#include "public/platform/WebWorkerRunLoop.h"

namespace WebCore {

WorkerScriptController::WorkerScriptController(WorkerGlobalScope& workerGlobalScope)
    : m_workerGlobalScope(workerGlobalScope)
    , m_executionForbidden(false)
    , m_executionScheduledToTerminate(false)
{
    v8::Isolate* isolate = v8::Isolate::New();
    isolate->Enter();
    V8Initializer::initializeWorker(isolate);
    v8::V8::Initialize();
    m_isolateHolder = adoptPtr(new gin::IsolateHolder(isolate, v8ArrayBufferAllocator()));
    V8PerIsolateData::ensureInitialized(isolate);
    m_world = DOMWrapperWorld::create(WorkerWorldId, -1);
    m_interruptor = adoptPtr(new V8IsolateInterruptor(isolate));
    ThreadState::current()->addInterruptor(m_interruptor.get());
}

// We need to postpone V8 Isolate destruction until the very end of
// worker thread finalization when all objects on the worker heap
// are destroyed.
class IsolateCleanupTask : public ThreadState::CleanupTask {
public:
    static PassOwnPtr<IsolateCleanupTask> create(PassOwnPtr<gin::IsolateHolder> isolateHolder)
    {
        return adoptPtr(new IsolateCleanupTask(isolateHolder));
    }

    virtual void postCleanup()
    {
        v8::Isolate* isolate = m_isolateHolder->isolate();
        V8PerIsolateData::dispose(isolate);
        isolate->Exit();
        m_isolateHolder.clear();
        isolate->Dispose();
    }

private:
    explicit IsolateCleanupTask(PassOwnPtr<gin::IsolateHolder> isolateHolder) : m_isolateHolder(isolateHolder)  { }

    OwnPtr<gin::IsolateHolder> m_isolateHolder;
};

WorkerScriptController::~WorkerScriptController()
{
    ThreadState::current()->removeInterruptor(m_interruptor.get());

    RELEASE_ASSERT(m_world->hasOneRef());
    // ~DOMWrapperWorld() must be called before disposing the isolate.
    m_world = nullptr;

    // The corresponding call to didStartWorkerRunLoop is in
    // WorkerThread::workerThread().
    // See http://webkit.org/b/83104#c14 for why this is here.
    blink::Platform::current()->didStopWorkerRunLoop(blink::WebWorkerRunLoop(&m_workerGlobalScope.thread()->runLoop()));

    disposeContext();

    ThreadState::current()->addCleanupTask(IsolateCleanupTask::create(m_isolateHolder.release()));
}

void WorkerScriptController::disposeContext()
{
    m_perContextData.clear();
    m_contextHolder.clear();
}

bool WorkerScriptController::initializeContextIfNeeded()
{
    if (m_contextHolder)
        return true;

    v8::Handle<v8::Context> context = v8::Context::New(isolate());
    if (context.IsEmpty())
        return false;

    m_contextHolder = adoptPtr(new gin::ContextHolder(isolate()));
    m_contextHolder->SetContext(context);

    v8::Context::Scope scope(context);

    V8PerContextDataHolder::install(context, m_world.get());

    m_perContextData = V8PerContextData::create(context);
    if (!m_perContextData->init()) {
        disposeContext();
        return false;
    }

    // Set DebugId for the new context.
    context->SetEmbedderData(0, v8AtomicString(isolate(), "worker"));

    // Create a new JS object and use it as the prototype for the shadow global object.
    const WrapperTypeInfo* contextType = &V8DedicatedWorkerGlobalScope::wrapperTypeInfo;
    if (m_workerGlobalScope.isServiceWorkerGlobalScope())
        contextType = &V8ServiceWorkerGlobalScope::wrapperTypeInfo;
    else if (!m_workerGlobalScope.isDedicatedWorkerGlobalScope())
        contextType = &V8SharedWorkerGlobalScope::wrapperTypeInfo;
    v8::Handle<v8::Function> workerGlobalScopeConstructor = m_perContextData->constructorForType(contextType);
    v8::Local<v8::Object> jsWorkerGlobalScope = V8ObjectConstructor::newInstance(workerGlobalScopeConstructor);
    if (jsWorkerGlobalScope.IsEmpty()) {
        disposeContext();
        return false;
    }

    V8DOMWrapper::associateObjectWithWrapper<V8WorkerGlobalScope>(PassRefPtr<WorkerGlobalScope>(m_workerGlobalScope), contextType, jsWorkerGlobalScope, isolate(), WrapperConfiguration::Dependent);

    // Insert the object instance as the prototype of the shadow object.
    v8::Handle<v8::Object> globalObject = v8::Handle<v8::Object>::Cast(m_contextHolder->context()->Global()->GetPrototype());
    globalObject->SetPrototype(jsWorkerGlobalScope);

    return true;
}

ScriptValue WorkerScriptController::evaluate(const String& script, const String& fileName, const TextPosition& scriptStartPosition, WorkerGlobalScopeExecutionState* state)
{
    v8::HandleScope handleScope(isolate());

    if (!initializeContextIfNeeded())
        return ScriptValue();

    v8::Handle<v8::Context> context = m_contextHolder->context();
    if (!m_disableEvalPending.isEmpty()) {
        context->AllowCodeGenerationFromStrings(false);
        context->SetErrorMessageForCodeGenerationFromStrings(v8String(isolate(), m_disableEvalPending));
        m_disableEvalPending = String();
    }

    v8::Context::Scope scope(context);

    v8::TryCatch block;

    v8::Handle<v8::String> scriptString = v8String(isolate(), script);
    v8::Handle<v8::Script> compiledScript = V8ScriptRunner::compileScript(scriptString, fileName, scriptStartPosition, 0, isolate());
    v8::Local<v8::Value> result = V8ScriptRunner::runCompiledScript(compiledScript, &m_workerGlobalScope, isolate());

    if (!block.CanContinue()) {
        m_workerGlobalScope.script()->forbidExecution();
        return ScriptValue();
    }

    if (block.HasCaught()) {
        v8::Local<v8::Message> message = block.Message();
        state->hadException = true;
        state->errorMessage = toCoreString(message->Get());
        state->lineNumber = message->GetLineNumber();
        state->columnNumber = message->GetStartColumn() + 1;
        V8TRYCATCH_FOR_V8STRINGRESOURCE_RETURN(V8StringResource<>, sourceURL, message->GetScriptResourceName(), ScriptValue());
        state->sourceURL = sourceURL;
        state->exception = ScriptValue(block.Exception(), isolate());
        block.Reset();
    } else
        state->hadException = false;

    if (result.IsEmpty() || result->IsUndefined())
        return ScriptValue();

    return ScriptValue(result, isolate());
}

void WorkerScriptController::evaluate(const ScriptSourceCode& sourceCode, RefPtr<ErrorEvent>* errorEvent)
{
    if (isExecutionForbidden())
        return;

    WorkerGlobalScopeExecutionState state;
    evaluate(sourceCode.source(), sourceCode.url().string(), sourceCode.startPosition(), &state);
    if (state.hadException) {
        if (errorEvent) {
            *errorEvent = m_workerGlobalScope.shouldSanitizeScriptError(state.sourceURL, NotSharableCrossOrigin) ?
                ErrorEvent::createSanitizedError(nullptr) : ErrorEvent::create(state.errorMessage, state.sourceURL, state.lineNumber, state.columnNumber, nullptr);
            V8ErrorHandler::storeExceptionOnErrorEventWrapper(errorEvent->get(), state.exception.v8Value(), isolate());
        } else {
            ASSERT(!m_workerGlobalScope.shouldSanitizeScriptError(state.sourceURL, NotSharableCrossOrigin));
            RefPtr<ErrorEvent> event = m_errorEventFromImportedScript ? m_errorEventFromImportedScript.release() : ErrorEvent::create(state.errorMessage, state.sourceURL, state.lineNumber, state.columnNumber, nullptr);
            m_workerGlobalScope.reportException(event, nullptr, NotSharableCrossOrigin);
        }
    }
}

void WorkerScriptController::scheduleExecutionTermination()
{
    // The mutex provides a memory barrier to ensure that once
    // termination is scheduled, isExecutionTerminating will
    // accurately reflect that state when called from another thread.
    {
        MutexLocker locker(m_scheduledTerminationMutex);
        m_executionScheduledToTerminate = true;
    }
    v8::V8::TerminateExecution(isolate());
}

bool WorkerScriptController::isExecutionTerminating() const
{
    // See comments in scheduleExecutionTermination regarding mutex usage.
    MutexLocker locker(m_scheduledTerminationMutex);
    return m_executionScheduledToTerminate;
}

void WorkerScriptController::forbidExecution()
{
    ASSERT(m_workerGlobalScope.isContextThread());
    m_executionForbidden = true;
}

bool WorkerScriptController::isExecutionForbidden() const
{
    ASSERT(m_workerGlobalScope.isContextThread());
    return m_executionForbidden;
}

void WorkerScriptController::disableEval(const String& errorMessage)
{
    m_disableEvalPending = errorMessage;
}

void WorkerScriptController::rethrowExceptionFromImportedScript(PassRefPtr<ErrorEvent> errorEvent)
{
    m_errorEventFromImportedScript = errorEvent;
    throwError(V8ThrowException::createError(v8GeneralError, m_errorEventFromImportedScript->message(), isolate()), isolate());
}

WorkerScriptController* WorkerScriptController::controllerForContext(v8::Isolate* isolate)
{
    // Happens on frame destruction, check otherwise GetCurrent() will crash.
    if (!isolate || !isolate->InContext())
        return 0;
    v8::Handle<v8::Context> context = isolate->GetCurrentContext();
    v8::Handle<v8::Object> global = V8WorkerGlobalScope::findInstanceInPrototypeChain(context->Global(), isolate);
    // Return 0 if the current executing context is not the worker context.
    if (global.IsEmpty())
        return 0;
    WorkerGlobalScope* workerGlobalScope = V8WorkerGlobalScope::toNative(global);
    return workerGlobalScope->script();
}

} // namespace WebCore
