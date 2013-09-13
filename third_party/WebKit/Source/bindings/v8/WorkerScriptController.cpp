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
#include "core/page/DOMTimer.h"
#include "core/workers/WorkerGlobalScope.h"
#include "core/workers/WorkerObjectProxy.h"
#include "core/workers/WorkerThread.h"
#include <v8.h>

#include "public/platform/Platform.h"
#include "public/platform/WebWorkerRunLoop.h"

namespace WebCore {

WorkerScriptController::WorkerScriptController(WorkerGlobalScope* workerGlobalScope)
    : m_workerGlobalScope(workerGlobalScope)
    , m_isolate(v8::Isolate::New())
    , m_executionForbidden(false)
    , m_executionScheduledToTerminate(false)
{
    m_isolate->Enter();
    v8::V8::Initialize();
    V8PerIsolateData* data = V8PerIsolateData::create(m_isolate);
    m_domDataStore = adoptPtr(new DOMDataStore(WorkerWorld));
    data->setWorkerDOMDataStore(m_domDataStore.get());

    V8Initializer::initializeWorker(m_isolate);
}

WorkerScriptController::~WorkerScriptController()
{
    m_domDataStore.clear();

    // The corresponding call to didStartWorkerRunLoop is in
    // WorkerThread::workerThread().
    // See http://webkit.org/b/83104#c14 for why this is here.
    WebKit::Platform::current()->didStopWorkerRunLoop(WebKit::WebWorkerRunLoop(&m_workerGlobalScope->thread()->runLoop()));

    disposeContext();
    V8PerIsolateData::dispose(m_isolate);
    m_isolate->Exit();
    m_isolate->Dispose();
}

void WorkerScriptController::disposeContext()
{
    m_perContextData.clear();
    m_context.clear();
}

bool WorkerScriptController::initializeContextIfNeeded()
{
    if (!m_context.isEmpty())
        return true;

    m_context.set(m_isolate, v8::Context::New(m_isolate));
    if (m_context.isEmpty())
        return false;

    // Starting from now, use local context only.
    v8::Local<v8::Context> context = m_context.newLocal(m_isolate);

    v8::Context::Scope scope(context);

    m_perContextData = V8PerContextData::create(context);
    if (!m_perContextData->init()) {
        disposeContext();
        return false;
    }

    // Set DebugId for the new context.
    context->SetEmbedderData(0, v8::String::NewSymbol("worker"));

    // Create a new JS object and use it as the prototype for the shadow global object.
    WrapperTypeInfo* contextType = &V8DedicatedWorkerGlobalScope::info;
    if (!m_workerGlobalScope->isDedicatedWorkerGlobalScope())
        contextType = &V8SharedWorkerGlobalScope::info;
    v8::Handle<v8::Function> workerGlobalScopeConstructor = m_perContextData->constructorForType(contextType);
    v8::Local<v8::Object> jsWorkerGlobalScope = V8ObjectConstructor::newInstance(workerGlobalScopeConstructor);
    if (jsWorkerGlobalScope.IsEmpty()) {
        disposeContext();
        return false;
    }

    V8DOMWrapper::associateObjectWithWrapper<V8WorkerGlobalScope>(PassRefPtr<WorkerGlobalScope>(m_workerGlobalScope), contextType, jsWorkerGlobalScope, m_isolate, WrapperConfiguration::Dependent);

    // Insert the object instance as the prototype of the shadow object.
    v8::Handle<v8::Object> globalObject = v8::Handle<v8::Object>::Cast(m_context.newLocal(m_isolate)->Global()->GetPrototype());
    globalObject->SetPrototype(jsWorkerGlobalScope);

    return true;
}

ScriptValue WorkerScriptController::evaluate(const String& script, const String& fileName, const TextPosition& scriptStartPosition, WorkerGlobalScopeExecutionState* state)
{
    v8::HandleScope handleScope(m_isolate);

    if (!initializeContextIfNeeded())
        return ScriptValue();

    v8::Handle<v8::Context> context = m_context.newLocal(m_isolate);
    if (!m_disableEvalPending.isEmpty()) {
        context->AllowCodeGenerationFromStrings(false);
        context->SetErrorMessageForCodeGenerationFromStrings(v8String(m_disableEvalPending, m_isolate));
        m_disableEvalPending = String();
    }

    v8::Context::Scope scope(context);

    v8::TryCatch block;

    v8::Handle<v8::String> scriptString = v8String(script, m_isolate);
    v8::Handle<v8::Script> compiledScript = V8ScriptRunner::compileScript(scriptString, fileName, scriptStartPosition, 0, m_isolate);
    v8::Local<v8::Value> result = V8ScriptRunner::runCompiledScript(compiledScript, m_workerGlobalScope, m_isolate);

    if (!block.CanContinue()) {
        m_workerGlobalScope->script()->forbidExecution();
        return ScriptValue();
    }

    if (block.HasCaught()) {
        v8::Local<v8::Message> message = block.Message();
        state->hadException = true;
        state->errorMessage = toWebCoreString(message->Get());
        state->lineNumber = message->GetLineNumber();
        state->columnNumber = message->GetStartColumn();
        state->sourceURL = toWebCoreString(message->GetScriptResourceName());
        state->exception = ScriptValue(block.Exception());
        block.Reset();
    } else
        state->hadException = false;

    if (result.IsEmpty() || result->IsUndefined())
        return ScriptValue();

    return ScriptValue(result);
}

void WorkerScriptController::evaluate(const ScriptSourceCode& sourceCode, RefPtr<ErrorEvent>* errorEvent)
{
    if (isExecutionForbidden())
        return;

    WorkerGlobalScopeExecutionState state;
    evaluate(sourceCode.source(), sourceCode.url().string(), sourceCode.startPosition(), &state);
    if (state.hadException) {
        if (errorEvent) {
            *errorEvent = m_workerGlobalScope->shouldSanitizeScriptError(state.sourceURL, NotSharableCrossOrigin) ?
                ErrorEvent::createSanitizedError(0) : ErrorEvent::create(state.errorMessage, state.sourceURL, state.lineNumber, state.columnNumber, 0);
            V8ErrorHandler::storeExceptionOnErrorEventWrapper(errorEvent->get(), state.exception.v8Value(), m_isolate);
        } else {
            ASSERT(!m_workerGlobalScope->shouldSanitizeScriptError(state.sourceURL, NotSharableCrossOrigin));
            RefPtr<ErrorEvent> event = m_errorEventFromImportedScript ? m_errorEventFromImportedScript.release() : ErrorEvent::create(state.errorMessage, state.sourceURL, state.lineNumber, state.columnNumber, 0);
            m_workerGlobalScope->reportException(event, 0, NotSharableCrossOrigin);
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
    v8::V8::TerminateExecution(m_isolate);
}

bool WorkerScriptController::isExecutionTerminating() const
{
    // See comments in scheduleExecutionTermination regarding mutex usage.
    MutexLocker locker(m_scheduledTerminationMutex);
    return m_executionScheduledToTerminate;
}

void WorkerScriptController::forbidExecution()
{
    ASSERT(m_workerGlobalScope->isContextThread());
    m_executionForbidden = true;
}

bool WorkerScriptController::isExecutionForbidden() const
{
    ASSERT(m_workerGlobalScope->isContextThread());
    return m_executionForbidden;
}

void WorkerScriptController::disableEval(const String& errorMessage)
{
    m_disableEvalPending = errorMessage;
}

void WorkerScriptController::rethrowExceptionFromImportedScript(PassRefPtr<ErrorEvent> errorEvent)
{
    m_errorEventFromImportedScript = errorEvent;
    throwError(V8ThrowException::createError(v8GeneralError, m_errorEventFromImportedScript->message(), m_isolate), m_isolate);
}

WorkerScriptController* WorkerScriptController::controllerForContext()
{
    // Happens on frame destruction, check otherwise GetCurrent() will crash.
    if (!v8::Context::InContext())
        return 0;
    v8::Handle<v8::Context> context = v8::Context::GetCurrent();
    v8::Handle<v8::Object> global = context->Global();
    global = global->FindInstanceInPrototypeChain(V8WorkerGlobalScope::GetTemplate(context->GetIsolate(), WorkerWorld));
    // Return 0 if the current executing context is not the worker context.
    if (global.IsEmpty())
        return 0;
    WorkerGlobalScope* workerGlobalScope = V8WorkerGlobalScope::toNative(global);
    return workerGlobalScope->script();
}

} // namespace WebCore
