/*
 * Copyright (c) 2010-2011 Google Inc. All rights reserved.
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

#include "core/inspector/v8/V8DebuggerImpl.h"

#include "bindings/core/v8/V8Binding.h"
#include "core/inspector/v8/JavaScriptCallFrame.h"
#include "core/inspector/v8/ScriptBreakpoint.h"
#include "core/inspector/v8/V8DebuggerAgentImpl.h"
#include "core/inspector/v8/V8DebuggerClient.h"
#include "core/inspector/v8/V8JavaScriptCallFrame.h"
#include "platform/JSONValues.h"
#include "public/platform/Platform.h"
#include "public/platform/WebData.h"
#include "wtf/Atomics.h"
#include "wtf/Vector.h"
#include "wtf/text/CString.h"

namespace blink {

namespace {
const char stepIntoV8MethodName[] = "stepIntoStatement";
const char stepOutV8MethodName[] = "stepOutOfFunction";
volatile int s_lastContextId = 0;
}

static bool inLiveEditScope = false;

v8::MaybeLocal<v8::Value> V8DebuggerImpl::callDebuggerMethod(const char* functionName, int argc, v8::Local<v8::Value> argv[])
{
    v8::Local<v8::Object> debuggerScript = m_debuggerScript.Get(m_isolate);
    v8::Local<v8::Function> function = v8::Local<v8::Function>::Cast(debuggerScript->Get(v8InternalizedString(functionName)));
    ASSERT(m_isolate->InContext());
    return m_client->callInternalFunction(function, debuggerScript, argc, argv);
}

PassOwnPtr<V8Debugger> V8Debugger::create(v8::Isolate* isolate, V8DebuggerClient* client)
{
    return adoptPtr(new V8DebuggerImpl(isolate, client));
}

V8DebuggerImpl::V8DebuggerImpl(v8::Isolate* isolate, V8DebuggerClient* client)
    : m_isolate(isolate)
    , m_client(client)
    , m_breakpointsActivated(true)
    , m_runningNestedMessageLoop(false)
{
}

V8DebuggerImpl::~V8DebuggerImpl()
{
}

void V8DebuggerImpl::enable()
{
    ASSERT(!enabled());
    v8::HandleScope scope(m_isolate);
    v8::Debug::SetDebugEventListener(m_isolate, &V8DebuggerImpl::v8DebugEventCallback, v8::External::New(m_isolate, this));
    m_debuggerContext.Reset(m_isolate, v8::Debug::GetDebugContext(m_isolate));
    m_callFrameWrapperTemplate.Reset(m_isolate, V8JavaScriptCallFrame::createWrapperTemplate(m_isolate));
    compileDebuggerScript();
}

void V8DebuggerImpl::disable()
{
    ASSERT(enabled());
    clearBreakpoints();
    m_debuggerScript.Reset();
    m_debuggerContext.Reset();
    m_callFrameWrapperTemplate.Reset();
    v8::Debug::SetDebugEventListener(m_isolate, nullptr);
}

bool V8DebuggerImpl::enabled() const
{
    return !m_debuggerScript.IsEmpty();
}

void V8Debugger::setContextDebugData(v8::Local<v8::Context> context, const String& type, int contextGroupId)
{
    int contextId = atomicIncrement(&s_lastContextId);
    String debugData = String::number(contextGroupId) + "," + String::number(contextId) + "," + type;
    v8::HandleScope scope(context->GetIsolate());
    v8::Context::Scope contextScope(context);
    context->SetEmbedderData(static_cast<int>(v8::Context::kDebugIdIndex), v8String(context->GetIsolate(), debugData));
}

int V8Debugger::contextId(v8::Local<v8::Context> context)
{
    v8::Local<v8::Value> data = context->GetEmbedderData(static_cast<int>(v8::Context::kDebugIdIndex));
    if (data.IsEmpty() || !data->IsString())
        return 0;
    String dataString = toCoreString(data.As<v8::String>());
    if (dataString.isEmpty())
        return 0;
    size_t commaPos = dataString.find(",");
    if (commaPos == kNotFound)
        return 0;
    size_t commaPos2 = dataString.find(",", commaPos + 1);
    if (commaPos2 == kNotFound)
        return 0;
    return dataString.substring(commaPos + 1, commaPos2 - commaPos - 1).toInt();
}

static int getGroupId(v8::Local<v8::Context> context)
{
    v8::Local<v8::Value> data = context->GetEmbedderData(static_cast<int>(v8::Context::kDebugIdIndex));
    if (data.IsEmpty() || !data->IsString())
        return 0;
    String dataString = toCoreString(data.As<v8::String>());
    if (dataString.isEmpty())
        return 0;
    size_t commaPos = dataString.find(",");
    if (commaPos == kNotFound)
        return 0;
    return dataString.left(commaPos).toInt();
}

void V8DebuggerImpl::addAgent(int contextGroupId, V8DebuggerAgentImpl* agent)
{
    ASSERT(contextGroupId);
    ASSERT(!m_agentsMap.contains(contextGroupId));
    if (m_agentsMap.isEmpty())
        enable();
    m_agentsMap.set(contextGroupId, agent);

    Vector<V8DebuggerParsedScript> compiledScripts;
    getCompiledScripts(contextGroupId, compiledScripts);
    for (size_t i = 0; i < compiledScripts.size(); i++)
        agent->didParseSource(compiledScripts[i]);
}

void V8DebuggerImpl::removeAgent(int contextGroupId)
{
    ASSERT(contextGroupId);
    if (!m_agentsMap.contains(contextGroupId))
        return;

    if (!m_pausedContext.IsEmpty() && getGroupId(m_pausedContext) == contextGroupId)
        continueProgram();

    m_agentsMap.remove(contextGroupId);

    if (m_agentsMap.isEmpty())
        disable();
}

V8DebuggerAgentImpl* V8DebuggerImpl::getAgentForContext(v8::Local<v8::Context> context)
{
    int groupId = getGroupId(context);
    if (!groupId)
        return nullptr;
    return m_agentsMap.get(groupId);
}

void V8DebuggerImpl::getCompiledScripts(int contextGroupId, Vector<V8DebuggerParsedScript>& result)
{
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Object> debuggerScript = m_debuggerScript.Get(m_isolate);
    ASSERT(!debuggerScript->IsUndefined());
    v8::Local<v8::Function> getScriptsFunction = v8::Local<v8::Function>::Cast(debuggerScript->Get(v8InternalizedString("getScripts")));
    v8::Local<v8::Value> argv[] = { v8::Integer::New(m_isolate, contextGroupId) };
    v8::Local<v8::Value> value;
    if (!m_client->callInternalFunction(getScriptsFunction, debuggerScript, WTF_ARRAY_LENGTH(argv), argv).ToLocal(&value))
        return;
    ASSERT(value->IsArray());
    v8::Local<v8::Array> scriptsArray = v8::Local<v8::Array>::Cast(value);
    result.reserveCapacity(scriptsArray->Length());
    for (unsigned i = 0; i < scriptsArray->Length(); ++i)
        result.append(createParsedScript(v8::Local<v8::Object>::Cast(scriptsArray->Get(v8::Integer::New(m_isolate, i))), true));
}

String V8DebuggerImpl::setBreakpoint(const String& sourceID, const ScriptBreakpoint& scriptBreakpoint, int* actualLineNumber, int* actualColumnNumber, bool interstatementLocation)
{
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Object> info = v8::Object::New(m_isolate);
    info->Set(v8InternalizedString("sourceID"), v8String(m_isolate, sourceID));
    info->Set(v8InternalizedString("lineNumber"), v8::Integer::New(m_isolate, scriptBreakpoint.lineNumber));
    info->Set(v8InternalizedString("columnNumber"), v8::Integer::New(m_isolate, scriptBreakpoint.columnNumber));
    info->Set(v8InternalizedString("interstatementLocation"), v8Boolean(interstatementLocation, m_isolate));
    info->Set(v8InternalizedString("condition"), v8String(m_isolate, scriptBreakpoint.condition));

    v8::Local<v8::Function> setBreakpointFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.Get(m_isolate)->Get(v8InternalizedString("setBreakpoint")));
    v8::Local<v8::Value> breakpointId = v8CallOrCrash(v8::Debug::Call(debuggerContext(), setBreakpointFunction, info));
    if (!breakpointId->IsString())
        return "";
    *actualLineNumber = info->Get(v8InternalizedString("lineNumber"))->Int32Value();
    *actualColumnNumber = info->Get(v8InternalizedString("columnNumber"))->Int32Value();
    return toCoreString(breakpointId.As<v8::String>());
}

void V8DebuggerImpl::removeBreakpoint(const String& breakpointId)
{
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Object> info = v8::Object::New(m_isolate);
    info->Set(v8InternalizedString("breakpointId"), v8String(m_isolate, breakpointId));

    v8::Local<v8::Function> removeBreakpointFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.Get(m_isolate)->Get(v8InternalizedString("removeBreakpoint")));
    v8CallOrCrash(v8::Debug::Call(debuggerContext(), removeBreakpointFunction, info));
}

void V8DebuggerImpl::clearBreakpoints()
{
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Function> clearBreakpoints = v8::Local<v8::Function>::Cast(m_debuggerScript.Get(m_isolate)->Get(v8InternalizedString("clearBreakpoints")));
    v8CallOrCrash(v8::Debug::Call(debuggerContext(), clearBreakpoints));
}

void V8DebuggerImpl::setBreakpointsActivated(bool activated)
{
    if (!enabled()) {
        ASSERT_NOT_REACHED();
        return;
    }
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Object> info = v8::Object::New(m_isolate);
    info->Set(v8InternalizedString("enabled"), v8::Boolean::New(m_isolate, activated));
    v8::Local<v8::Function> setBreakpointsActivated = v8::Local<v8::Function>::Cast(m_debuggerScript.Get(m_isolate)->Get(v8InternalizedString("setBreakpointsActivated")));
    v8CallOrCrash(v8::Debug::Call(debuggerContext(), setBreakpointsActivated, info));

    m_breakpointsActivated = activated;
}

V8DebuggerImpl::PauseOnExceptionsState V8DebuggerImpl::pauseOnExceptionsState()
{
    ASSERT(enabled());
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Value> argv[] = { v8Undefined() };
    v8::Local<v8::Value> result = callDebuggerMethod("pauseOnExceptionsState", 0, argv).ToLocalChecked();
    return static_cast<V8DebuggerImpl::PauseOnExceptionsState>(result->Int32Value());
}

void V8DebuggerImpl::setPauseOnExceptionsState(PauseOnExceptionsState pauseOnExceptionsState)
{
    ASSERT(enabled());
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Value> argv[] = { v8::Int32::New(m_isolate, pauseOnExceptionsState) };
    callDebuggerMethod("setPauseOnExceptionsState", 1, argv);
}

void V8DebuggerImpl::setPauseOnNextStatement(bool pause)
{
    ASSERT(!m_runningNestedMessageLoop);
    if (pause)
        v8::Debug::DebugBreak(m_isolate);
    else
        v8::Debug::CancelDebugBreak(m_isolate);
}

bool V8DebuggerImpl::pausingOnNextStatement()
{
    return v8::Debug::CheckDebugBreak(m_isolate);
}

bool V8DebuggerImpl::canBreakProgram()
{
    if (!m_breakpointsActivated)
        return false;
    return m_isolate->InContext();
}

void V8DebuggerImpl::breakProgram()
{
    if (isPaused()) {
        ASSERT(!m_runningNestedMessageLoop);
        v8::Local<v8::Value> exception;
        v8::Local<v8::Array> hitBreakpoints;
        handleProgramBreak(m_pausedContext, m_executionState, exception, hitBreakpoints);
        return;
    }

    if (!canBreakProgram())
        return;

    v8::HandleScope scope(m_isolate);
    if (m_breakProgramCallbackTemplate.IsEmpty()) {
        v8::Local<v8::FunctionTemplate> templ = v8::FunctionTemplate::New(m_isolate);
        templ->SetCallHandler(&V8DebuggerImpl::breakProgramCallback, v8::External::New(m_isolate, this));
        m_breakProgramCallbackTemplate.Reset(m_isolate, templ);
    }

    v8::Local<v8::Function> breakProgramFunction = v8::Local<v8::FunctionTemplate>::New(m_isolate, m_breakProgramCallbackTemplate)->GetFunction();
    v8CallOrCrash(v8::Debug::Call(debuggerContext(), breakProgramFunction));
}

void V8DebuggerImpl::continueProgram()
{
    if (isPaused())
        m_client->quitMessageLoopOnPause();
    m_pausedContext.Clear();
    m_executionState.Clear();
}

void V8DebuggerImpl::stepIntoStatement()
{
    ASSERT(isPaused());
    ASSERT(!m_executionState.IsEmpty());
    v8::HandleScope handleScope(m_isolate);
    v8::Local<v8::Value> argv[] = { m_executionState };
    callDebuggerMethod(stepIntoV8MethodName, 1, argv);
    continueProgram();
}

void V8DebuggerImpl::stepOverStatement()
{
    ASSERT(isPaused());
    ASSERT(!m_executionState.IsEmpty());
    v8::HandleScope handleScope(m_isolate);
    v8::Local<v8::Value> argv[] = { m_executionState };
    callDebuggerMethod("stepOverStatement", 1, argv);
    continueProgram();
}

void V8DebuggerImpl::stepOutOfFunction()
{
    ASSERT(isPaused());
    ASSERT(!m_executionState.IsEmpty());
    v8::HandleScope handleScope(m_isolate);
    v8::Local<v8::Value> argv[] = { m_executionState };
    callDebuggerMethod(stepOutV8MethodName, 1, argv);
    continueProgram();
}

void V8DebuggerImpl::clearStepping()
{
    ASSERT(enabled());
    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());

    v8::Local<v8::Value> argv[] = { v8Undefined() };
    callDebuggerMethod("clearStepping", 0, argv);
}

bool V8DebuggerImpl::setScriptSource(const String& sourceID, const String& newContent, bool preview, String* error, RefPtr<TypeBuilder::Debugger::SetScriptSourceError>& errorData, v8::Global<v8::Object>* newCallFrames, TypeBuilder::OptOutput<bool>* stackChanged)
{
    class EnableLiveEditScope {
    public:
        explicit EnableLiveEditScope(v8::Isolate* isolate) : m_isolate(isolate)
        {
            v8::Debug::SetLiveEditEnabled(m_isolate, true);
            inLiveEditScope = true;
        }
        ~EnableLiveEditScope()
        {
            v8::Debug::SetLiveEditEnabled(m_isolate, false);
            inLiveEditScope = false;
        }
    private:
        v8::Isolate* m_isolate;
    };

    ASSERT(enabled());
    v8::HandleScope scope(m_isolate);

    OwnPtr<v8::Context::Scope> contextScope;
    if (!isPaused())
        contextScope = adoptPtr(new v8::Context::Scope(debuggerContext()));

    v8::Local<v8::Value> argv[] = { v8String(m_isolate, sourceID), v8String(m_isolate, newContent), v8Boolean(preview, m_isolate) };

    v8::Local<v8::Value> v8result;
    {
        EnableLiveEditScope enableLiveEditScope(m_isolate);
        v8::TryCatch tryCatch(m_isolate);
        tryCatch.SetVerbose(false);
        v8::MaybeLocal<v8::Value> maybeResult = callDebuggerMethod("liveEditScriptSource", 3, argv);
        if (tryCatch.HasCaught()) {
            v8::Local<v8::Message> message = tryCatch.Message();
            if (!message.IsEmpty())
                *error = toCoreStringWithUndefinedOrNullCheck(message->Get());
            else
                *error = "Unknown error.";
            return false;
        }
        v8result = maybeResult.ToLocalChecked();
    }
    ASSERT(!v8result.IsEmpty());
    v8::Local<v8::Object> resultTuple = v8result->ToObject(m_isolate);
    int code = static_cast<int>(resultTuple->Get(0)->ToInteger(m_isolate)->Value());
    switch (code) {
    case 0:
        {
            *stackChanged = resultTuple->Get(1)->BooleanValue();
            // Call stack may have changed after if the edited function was on the stack.
            if (!preview && isPaused())
                newCallFrames->Reset(m_isolate, currentCallFrames());
            return true;
        }
    // Compile error.
    case 1:
        {
            RefPtr<TypeBuilder::Debugger::SetScriptSourceError::CompileError> compileError =
                TypeBuilder::Debugger::SetScriptSourceError::CompileError::create()
                    .setMessage(toCoreStringWithUndefinedOrNullCheck(resultTuple->Get(2)))
                    .setLineNumber(resultTuple->Get(3)->ToInteger(m_isolate)->Value())
                    .setColumnNumber(resultTuple->Get(4)->ToInteger(m_isolate)->Value());

            *error = toCoreStringWithUndefinedOrNullCheck(resultTuple->Get(1));
            errorData = TypeBuilder::Debugger::SetScriptSourceError::create();
            errorData->setCompileError(compileError);
            return false;
        }
    }
    *error = "Unknown error.";
    return false;
}

int V8DebuggerImpl::frameCount()
{
    ASSERT(isPaused());
    ASSERT(!m_executionState.IsEmpty());
    v8::Local<v8::Value> argv[] = { m_executionState };
    v8::Local<v8::Value> result = callDebuggerMethod("frameCount", WTF_ARRAY_LENGTH(argv), argv).ToLocalChecked();
    if (result->IsInt32())
        return result->Int32Value();
    return 0;
}

PassRefPtr<JavaScriptCallFrame> V8DebuggerImpl::wrapCallFrames(int maximumLimit, ScopeInfoDetails scopeDetails)
{
    const int scopeBits = 2;
    static_assert(NoScopes < (1 << scopeBits), "there must be enough bits to encode ScopeInfoDetails");

    ASSERT(maximumLimit >= 0);
    int data = (maximumLimit << scopeBits) | scopeDetails;
    v8::Local<v8::Value> currentCallFrameV8;
    if (m_executionState.IsEmpty()) {
        v8::Local<v8::Function> currentCallFrameFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.Get(m_isolate)->Get(v8InternalizedString("currentCallFrame")));
        currentCallFrameV8 = v8CallOrCrash(v8::Debug::Call(debuggerContext(), currentCallFrameFunction, v8::Integer::New(m_isolate, data)));
    } else {
        v8::Local<v8::Value> argv[] = { m_executionState, v8::Integer::New(m_isolate, data) };
        currentCallFrameV8 = callDebuggerMethod("currentCallFrame", WTF_ARRAY_LENGTH(argv), argv).ToLocalChecked();
    }
    ASSERT(!currentCallFrameV8.IsEmpty());
    if (!currentCallFrameV8->IsObject())
        return nullptr;
    return JavaScriptCallFrame::create(m_client, debuggerContext(), v8::Local<v8::Object>::Cast(currentCallFrameV8));
}

v8::Local<v8::Object> V8DebuggerImpl::currentCallFramesInner(ScopeInfoDetails scopeDetails)
{
    if (!m_isolate->InContext())
        return v8::Local<v8::Object>();

    // Filter out stack traces entirely consisting of V8's internal scripts.
    v8::Local<v8::StackTrace> stackTrace = v8::StackTrace::CurrentStackTrace(m_isolate, 1);
    if (!stackTrace->GetFrameCount())
        return v8::Local<v8::Object>();

    RefPtr<JavaScriptCallFrame> currentCallFrame = wrapCallFrames(0, scopeDetails);
    if (!currentCallFrame)
        return v8::Local<v8::Object>();

    v8::Local<v8::FunctionTemplate> wrapperTemplate = v8::Local<v8::FunctionTemplate>::New(m_isolate, m_callFrameWrapperTemplate);
    v8::Local<v8::Context> context = m_pausedContext.IsEmpty() ? m_isolate->GetCurrentContext() : m_pausedContext;
    v8::Context::Scope scope(context);
    v8::Local<v8::Object> wrapper = V8JavaScriptCallFrame::wrap(m_client, wrapperTemplate, context, currentCallFrame.release());
    return wrapper;
}

v8::Local<v8::Object> V8DebuggerImpl::currentCallFrames()
{
    return currentCallFramesInner(AllScopes);
}

v8::Local<v8::Object> V8DebuggerImpl::currentCallFramesForAsyncStack()
{
    return currentCallFramesInner(FastAsyncScopes);
}

PassRefPtr<JavaScriptCallFrame> V8DebuggerImpl::callFrameNoScopes(int index)
{
    if (!m_isolate->InContext())
        return nullptr;
    v8::HandleScope handleScope(m_isolate);

    v8::Local<v8::Value> currentCallFrameV8;
    if (m_executionState.IsEmpty()) {
        v8::Local<v8::Function> currentCallFrameFunction = v8::Local<v8::Function>::Cast(m_debuggerScript.Get(m_isolate)->Get(v8InternalizedString("currentCallFrameByIndex")));
        currentCallFrameV8 = v8CallOrCrash(v8::Debug::Call(debuggerContext(), currentCallFrameFunction, v8::Integer::New(m_isolate, index)));
    } else {
        v8::Local<v8::Value> argv[] = { m_executionState, v8::Integer::New(m_isolate, index) };
        currentCallFrameV8 = callDebuggerMethod("currentCallFrameByIndex", WTF_ARRAY_LENGTH(argv), argv).ToLocalChecked();
    }
    ASSERT(!currentCallFrameV8.IsEmpty());
    if (!currentCallFrameV8->IsObject())
        return nullptr;
    return JavaScriptCallFrame::create(m_client, debuggerContext(), v8::Local<v8::Object>::Cast(currentCallFrameV8));
}

static V8DebuggerImpl* toV8DebuggerImpl(v8::Local<v8::Value> data)
{
    void* p = v8::Local<v8::External>::Cast(data)->Value();
    return static_cast<V8DebuggerImpl*>(p);
}

void V8DebuggerImpl::breakProgramCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    ASSERT(2 == info.Length());
    V8DebuggerImpl* thisPtr = toV8DebuggerImpl(info.Data());
    v8::Local<v8::Context> pausedContext = thisPtr->m_isolate->GetCurrentContext();
    v8::Local<v8::Value> exception;
    v8::Local<v8::Array> hitBreakpoints;
    thisPtr->handleProgramBreak(pausedContext, v8::Local<v8::Object>::Cast(info[0]), exception, hitBreakpoints);
}

void V8DebuggerImpl::handleProgramBreak(v8::Local<v8::Context> pausedContext, v8::Local<v8::Object> executionState, v8::Local<v8::Value> exception, v8::Local<v8::Array> hitBreakpointNumbers, bool isPromiseRejection)
{
    // Don't allow nested breaks.
    if (m_runningNestedMessageLoop)
        return;

    V8DebuggerAgentImpl* agent = getAgentForContext(pausedContext);
    if (!agent)
        return;

    Vector<String> breakpointIds;
    if (!hitBreakpointNumbers.IsEmpty()) {
        breakpointIds.resize(hitBreakpointNumbers->Length());
        for (size_t i = 0; i < hitBreakpointNumbers->Length(); i++) {
            v8::Local<v8::Value> hitBreakpointNumber = hitBreakpointNumbers->Get(i);
            ASSERT(!hitBreakpointNumber.IsEmpty() && hitBreakpointNumber->IsInt32());
            breakpointIds[i] = String::number(hitBreakpointNumber->Int32Value());
        }
    }

    m_pausedContext = pausedContext;
    m_executionState = executionState;
    V8DebuggerAgentImpl::SkipPauseRequest result = agent->didPause(pausedContext, currentCallFrames(), exception, breakpointIds, isPromiseRejection);
    if (result == V8DebuggerAgentImpl::RequestNoSkip) {
        m_runningNestedMessageLoop = true;
        int groupId = getGroupId(pausedContext);
        ASSERT(groupId);
        m_client->runMessageLoopOnPause(groupId);
        // The agent may have been removed in the nested loop.
        agent = getAgentForContext(pausedContext);
        if (agent)
            agent->didContinue();
        m_runningNestedMessageLoop = false;
    }
    m_pausedContext.Clear();
    m_executionState.Clear();

    if (result == V8DebuggerAgentImpl::RequestStepFrame) {
        v8::Local<v8::Value> argv[] = { executionState };
        callDebuggerMethod("stepFrameStatement", 1, argv);
    } else if (result == V8DebuggerAgentImpl::RequestStepInto) {
        v8::Local<v8::Value> argv[] = { executionState };
        callDebuggerMethod(stepIntoV8MethodName, 1, argv);
    } else if (result == V8DebuggerAgentImpl::RequestStepOut) {
        v8::Local<v8::Value> argv[] = { executionState };
        callDebuggerMethod(stepOutV8MethodName, 1, argv);
    }
}

void V8DebuggerImpl::v8DebugEventCallback(const v8::Debug::EventDetails& eventDetails)
{
    V8DebuggerImpl* thisPtr = toV8DebuggerImpl(eventDetails.GetCallbackData());
    thisPtr->handleV8DebugEvent(eventDetails);
}

v8::Local<v8::Value> V8DebuggerImpl::callInternalGetterFunction(v8::Local<v8::Object> object, const char* functionName)
{
    v8::Local<v8::Value> getterValue = object->Get(v8InternalizedString(functionName));
    ASSERT(!getterValue.IsEmpty() && getterValue->IsFunction());
    return m_client->callInternalFunction(v8::Local<v8::Function>::Cast(getterValue), object, 0, 0).ToLocalChecked();
}

void V8DebuggerImpl::handleV8DebugEvent(const v8::Debug::EventDetails& eventDetails)
{
    if (!enabled())
        return;
    v8::DebugEvent event = eventDetails.GetEvent();
    if (event != v8::AsyncTaskEvent && event != v8::Break && event != v8::Exception && event != v8::AfterCompile && event != v8::BeforeCompile && event != v8::CompileError && event != v8::PromiseEvent)
        return;

    v8::Local<v8::Context> eventContext = eventDetails.GetEventContext();
    ASSERT(!eventContext.IsEmpty());

    V8DebuggerAgentImpl* agent = getAgentForContext(eventContext);
    if (agent) {
        v8::HandleScope scope(m_isolate);
        if (event == v8::AfterCompile || event == v8::CompileError) {
            v8::Context::Scope contextScope(debuggerContext());
            v8::Local<v8::Value> argv[] = { eventDetails.GetEventData() };
            v8::Local<v8::Value> value = callDebuggerMethod("getAfterCompileScript", 1, argv).ToLocalChecked();
            ASSERT(value->IsObject());
            v8::Local<v8::Object> object = v8::Local<v8::Object>::Cast(value);
            agent->didParseSource(createParsedScript(object, event == v8::AfterCompile));
        } else if (event == v8::Exception) {
            v8::Local<v8::Object> eventData = eventDetails.GetEventData();
            v8::Local<v8::Value> exception = callInternalGetterFunction(eventData, "exception");
            v8::Local<v8::Value> promise = callInternalGetterFunction(eventData, "promise");
            bool isPromiseRejection = !promise.IsEmpty() && promise->IsObject();
            handleProgramBreak(eventContext, eventDetails.GetExecutionState(), exception, v8::Local<v8::Array>(), isPromiseRejection);
        } else if (event == v8::Break) {
            v8::Local<v8::Value> argv[] = { eventDetails.GetEventData() };
            v8::Local<v8::Value> hitBreakpoints = callDebuggerMethod("getBreakpointNumbers", 1, argv).ToLocalChecked();
            ASSERT(hitBreakpoints->IsArray());
            handleProgramBreak(eventContext, eventDetails.GetExecutionState(), v8::Local<v8::Value>(), hitBreakpoints.As<v8::Array>());
        } else if (event == v8::AsyncTaskEvent) {
            if (agent->v8AsyncTaskEventsEnabled())
                handleV8AsyncTaskEvent(agent, eventContext, eventDetails.GetExecutionState(), eventDetails.GetEventData());
        } else if (event == v8::PromiseEvent) {
            if (agent->v8PromiseEventsEnabled())
                handleV8PromiseEvent(agent, eventContext, eventDetails.GetExecutionState(), eventDetails.GetEventData());
        }
    }
}

void V8DebuggerImpl::handleV8AsyncTaskEvent(V8DebuggerAgentImpl* agent, v8::Local<v8::Context> context, v8::Local<v8::Object> executionState, v8::Local<v8::Object> eventData)
{
    String type = toCoreStringWithUndefinedOrNullCheck(callInternalGetterFunction(eventData, "type"));
    String name = toCoreStringWithUndefinedOrNullCheck(callInternalGetterFunction(eventData, "name"));
    int id = callInternalGetterFunction(eventData, "id")->ToInteger(m_isolate)->Value();

    m_pausedContext = context;
    m_executionState = executionState;
    agent->didReceiveV8AsyncTaskEvent(context, type, name, id);
    m_pausedContext.Clear();
    m_executionState.Clear();
}

void V8DebuggerImpl::handleV8PromiseEvent(V8DebuggerAgentImpl* agent, v8::Local<v8::Context> context, v8::Local<v8::Object> executionState, v8::Local<v8::Object> eventData)
{
    v8::Local<v8::Value> argv[] = { eventData };
    v8::Local<v8::Value> value = callDebuggerMethod("getPromiseDetails", 1, argv).ToLocalChecked();
    ASSERT(value->IsObject());
    v8::Local<v8::Object> promiseDetails = v8::Local<v8::Object>::Cast(value);
    v8::Local<v8::Object> promise = promiseDetails->Get(v8InternalizedString("promise"))->ToObject(m_isolate);
    int status = promiseDetails->Get(v8InternalizedString("status"))->ToInteger(m_isolate)->Value();
    v8::Local<v8::Value> parentPromise = promiseDetails->Get(v8InternalizedString("parentPromise"));

    m_pausedContext = context;
    m_executionState = executionState;
    agent->didReceiveV8PromiseEvent(context, promise, parentPromise, status);
    m_pausedContext.Clear();
    m_executionState.Clear();
}

V8DebuggerParsedScript V8DebuggerImpl::createParsedScript(v8::Local<v8::Object> object, bool success)
{
    v8::Local<v8::Value> id = object->Get(v8InternalizedString("id"));
    ASSERT(!id.IsEmpty() && id->IsInt32());

    V8DebuggerParsedScript parsedScript;
    parsedScript.scriptId = String::number(id->Int32Value());
    parsedScript.script.setURL(toCoreStringWithUndefinedOrNullCheck(object->Get(v8InternalizedString("name"))))
        .setSourceURL(toCoreStringWithUndefinedOrNullCheck(object->Get(v8InternalizedString("sourceURL"))))
        .setSourceMappingURL(toCoreStringWithUndefinedOrNullCheck(object->Get(v8InternalizedString("sourceMappingURL"))))
        .setSource(toCoreStringWithUndefinedOrNullCheck(object->Get(v8InternalizedString("source"))))
        .setStartLine(object->Get(v8InternalizedString("startLine"))->ToInteger(m_isolate)->Value())
        .setStartColumn(object->Get(v8InternalizedString("startColumn"))->ToInteger(m_isolate)->Value())
        .setEndLine(object->Get(v8InternalizedString("endLine"))->ToInteger(m_isolate)->Value())
        .setEndColumn(object->Get(v8InternalizedString("endColumn"))->ToInteger(m_isolate)->Value())
        .setIsContentScript(object->Get(v8InternalizedString("isContentScript"))->ToBoolean(m_isolate)->Value())
        .setIsInternalScript(object->Get(v8InternalizedString("isInternalScript"))->ToBoolean(m_isolate)->Value())
        .setExecutionContextId(object->Get(v8InternalizedString("executionContextId"))->ToInteger(m_isolate)->Value())
        .setIsLiveEdit(inLiveEditScope);
    parsedScript.success = success;
    return parsedScript;
}

void V8DebuggerImpl::compileDebuggerScript()
{
    if (!m_debuggerScript.IsEmpty()) {
        ASSERT_NOT_REACHED();
        return;
    }

    v8::HandleScope scope(m_isolate);
    v8::Context::Scope contextScope(debuggerContext());
    const WebData& source = Platform::current()->loadResource("DebuggerScriptSource.js");
    v8::Local<v8::String> scriptValue = v8::String::NewFromUtf8(m_isolate, source.data(), v8::NewStringType::kInternalized, source.size()).ToLocalChecked();
    v8::Local<v8::Value> value;
    if (!m_client->compileAndRunInternalScript(scriptValue).ToLocal(&value))
        return;
    ASSERT(value->IsObject());
    m_debuggerScript.Reset(m_isolate, value.As<v8::Object>());
}

v8::Local<v8::Context> V8DebuggerImpl::debuggerContext() const
{
    ASSERT(!m_debuggerContext.IsEmpty());
    return m_debuggerContext.Get(m_isolate);
}

v8::Local<v8::String> V8DebuggerImpl::v8InternalizedString(const char* str) const
{
    return v8::String::NewFromUtf8(m_isolate, str, v8::NewStringType::kInternalized).ToLocalChecked();
}

v8::MaybeLocal<v8::Value> V8DebuggerImpl::functionScopes(v8::Local<v8::Function> function)
{
    if (!enabled()) {
        ASSERT_NOT_REACHED();
        return v8::Local<v8::Value>::New(m_isolate, v8::Undefined(m_isolate));
    }
    v8::Local<v8::Value> argv[] = { function };
    return callDebuggerMethod("getFunctionScopes", 1, argv);
}

v8::Local<v8::Value> V8DebuggerImpl::generatorObjectDetails(v8::Local<v8::Object>& object)
{
    if (!enabled()) {
        ASSERT_NOT_REACHED();
        return v8::Local<v8::Value>::New(m_isolate, v8::Undefined(m_isolate));
    }
    v8::Local<v8::Value> argv[] = { object };
    return callDebuggerMethod("getGeneratorObjectDetails", 1, argv).ToLocalChecked();
}

v8::Local<v8::Value> V8DebuggerImpl::collectionEntries(v8::Local<v8::Object>& object)
{
    if (!enabled()) {
        ASSERT_NOT_REACHED();
        return v8::Local<v8::Value>::New(m_isolate, v8::Undefined(m_isolate));
    }
    v8::Local<v8::Value> argv[] = { object };
    return callDebuggerMethod("getCollectionEntries", 1, argv).ToLocalChecked();
}

v8::MaybeLocal<v8::Value> V8DebuggerImpl::setFunctionVariableValue(v8::Local<v8::Value> functionValue, int scopeNumber, const String& variableName, v8::Local<v8::Value> newValue)
{
    if (m_debuggerScript.IsEmpty()) {
        ASSERT_NOT_REACHED();
        return m_isolate->ThrowException(v8::String::NewFromUtf8(m_isolate, "Debugging is not enabled.", v8::NewStringType::kNormal).ToLocalChecked());
    }

    v8::Local<v8::Value> argv[] = {
        functionValue,
        v8::Local<v8::Value>(v8::Integer::New(m_isolate, scopeNumber)),
        v8String(m_isolate, variableName),
        newValue
    };
    return callDebuggerMethod("setFunctionVariableValue", 4, argv);
}


bool V8DebuggerImpl::isPaused()
{
    return !m_pausedContext.IsEmpty();
}

} // namespace blink
