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
#include "bindings/core/v8/SourceLocation.h"
#include "bindings/core/v8/V8ErrorHandler.h"
#include "bindings/core/v8/V8Node.h"
#include "bindings/core/v8/V8Window.h"
#include "bindings/core/v8/WorkerOrWorkletScriptController.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/Document.h"
#include "core/dom/Element.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/StaticNodeList.h"
#include "core/events/ErrorEvent.h"
#include "core/frame/Deprecation.h"
#include "core/frame/FrameConsole.h"
#include "core/frame/FrameHost.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Settings.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/IdentifiersFactory.h"
#include "core/inspector/InspectedFrames.h"
#include "core/inspector/InspectorTaskRunner.h"
#include "core/timing/MemoryInfo.h"
#include "core/workers/MainThreadWorkletGlobalScope.h"
#include "core/xml/XPathEvaluator.h"
#include "core/xml/XPathResult.h"
#include "platform/UserGestureIndicator.h"
#include "platform/v8_inspector/public/V8Inspector.h"
#include "wtf/PtrUtil.h"
#include "wtf/ThreadingPrimitives.h"
#include <memory>

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

LocalFrame* toFrame(ExecutionContext* context)
{
    if (!context)
        return nullptr;
    if (context->isDocument())
        return toDocument(context)->frame();
    if (context->isMainThreadWorkletGlobalScope())
        return toMainThreadWorkletGlobalScope(context)->frame();
    return nullptr;
}

}

MainThreadDebugger* MainThreadDebugger::s_instance = nullptr;

MainThreadDebugger::MainThreadDebugger(v8::Isolate* isolate)
    : ThreadDebugger(isolate)
    , m_taskRunner(wrapUnique(new InspectorTaskRunner()))
    , m_paused(false)
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

void MainThreadDebugger::reportConsoleMessage(ExecutionContext* context, ConsoleMessage* message)
{
    if (LocalFrame* frame = toFrame(context))
        frame->console().reportMessageToClient(message);
}

int MainThreadDebugger::contextGroupId(ExecutionContext* context)
{
    LocalFrame* frame = toFrame(context);
    return frame ? contextGroupId(frame) : 0;
}

void MainThreadDebugger::setClientMessageLoop(std::unique_ptr<ClientMessageLoop> clientMessageLoop)
{
    ASSERT(!m_clientMessageLoop);
    ASSERT(clientMessageLoop);
    m_clientMessageLoop = std::move(clientMessageLoop);
}

void MainThreadDebugger::didClearContextsForFrame(LocalFrame* frame)
{
    DCHECK(isMainThread());
    if (frame->localFrameRoot() == frame)
        v8Inspector()->resetContextGroup(contextGroupId(frame));
}

void MainThreadDebugger::contextCreated(ScriptState* scriptState, LocalFrame* frame, SecurityOrigin* origin)
{
    ASSERT(isMainThread());
    v8::HandleScope handles(scriptState->isolate());
    DOMWrapperWorld& world = scriptState->world();
    v8Inspector()->contextCreated(V8ContextInfo(scriptState->context(), contextGroupId(frame), world.isMainWorld(), origin ? origin->toRawString() : "", world.isIsolatedWorld() ? world.isolatedWorldHumanReadableName() : "", IdentifiersFactory::frameId(frame), scriptState->getExecutionContext()->isDocument()));
}

void MainThreadDebugger::contextWillBeDestroyed(ScriptState* scriptState)
{
    v8::HandleScope handles(scriptState->isolate());
    v8Inspector()->contextDestroyed(scriptState->context());
}

void MainThreadDebugger::exceptionThrown(ExecutionContext* context, ErrorEvent* event)
{
    LocalFrame* frame = nullptr;
    ScriptState* scriptState = nullptr;
    if (context->isDocument()) {
        frame = toDocument(context)->frame();
        scriptState = event->world() ? ScriptState::forWorld(frame, *event->world()) : nullptr;
    }
    if (context->isMainThreadWorkletGlobalScope()) {
        frame = toMainThreadWorkletGlobalScope(context)->frame();
        scriptState = toMainThreadWorkletGlobalScope(context)->scriptController()->getScriptState();
    }

    if (frame->host() && frame->host()->consoleMessageStorage().isMuted())
        return;

    const String16 defaultMessage = "Uncaught";
    if (scriptState && scriptState->contextIsValid()) {
        ScriptState::Scope scope(scriptState);
        v8::Local<v8::Value> exception = V8ErrorHandler::loadExceptionFromErrorEventWrapper(scriptState, event, scriptState->context()->Global());
        SourceLocation* location = event->location();
        v8Inspector()->exceptionThrown(scriptState->context(), defaultMessage, exception, event->messageForConsole(), location->url(), location->lineNumber(), location->columnNumber(), location->cloneStackTrace(), location->scriptId());
    }

    // TODO(dgozman): do not wrap in ConsoleMessage.
    frame->console().reportMessageToClient(ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, event->messageForConsole(), event->location()->clone()));
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

void MainThreadDebugger::interruptMainThreadAndRun(std::unique_ptr<InspectorTaskRunner::Task> task)
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
    m_paused = true;

    if (UserGestureToken* token = UserGestureIndicator::currentToken())
        token->setPauseInDebugger();
    // Wait for continue or step command.
    if (m_clientMessageLoop)
        m_clientMessageLoop->run(pausedFrame);
}

void MainThreadDebugger::quitMessageLoopOnPause()
{
    m_paused = false;
    if (m_clientMessageLoop)
        m_clientMessageLoop->quitNow();
}

void MainThreadDebugger::muteWarningsAndDeprecations(int contextGroupId)
{
    LocalFrame* frame = WeakIdentifierMap<LocalFrame>::lookup(contextGroupId);
    if (frame && frame->host()) {
        frame->host()->consoleMessageStorage().mute();
        frame->host()->useCounter().muteForInspector();
        frame->host()->deprecation().muteForInspector();
    }
}

void MainThreadDebugger::unmuteWarningsAndDeprecations(int contextGroupId)
{
    LocalFrame* frame = WeakIdentifierMap<LocalFrame>::lookup(contextGroupId);
    if (frame && frame->host()) {
        frame->host()->consoleMessageStorage().unmute();
        frame->host()->useCounter().unmuteForInspector();
        frame->host()->deprecation().unmuteForInspector();
    }
}

v8::Local<v8::Context> MainThreadDebugger::ensureDefaultContextInGroup(int contextGroupId)
{
    LocalFrame* frame = WeakIdentifierMap<LocalFrame>::lookup(contextGroupId);
    ScriptState* scriptState = frame ? ScriptState::forMainWorld(frame) : nullptr;
    return scriptState ? scriptState->context() : v8::Local<v8::Context>();
}

void MainThreadDebugger::beginEnsureAllContextsInGroup(int contextGroupId)
{
    LocalFrame* frame = WeakIdentifierMap<LocalFrame>::lookup(contextGroupId);
    frame->settings()->setForceMainWorldInitialization(true);
}

void MainThreadDebugger::endEnsureAllContextsInGroup(int contextGroupId)
{
    LocalFrame* frame = WeakIdentifierMap<LocalFrame>::lookup(contextGroupId);
    frame->settings()->setForceMainWorldInitialization(false);
}

bool MainThreadDebugger::canExecuteScripts(int contextGroupId)
{
    LocalFrame* frame = WeakIdentifierMap<LocalFrame>::lookup(contextGroupId);
    return frame->script().canExecuteScripts(NotAboutToExecuteScript);
}

void MainThreadDebugger::consoleAPIMessage(int contextGroupId, V8ConsoleAPIType type, const String16& message, const String16& url, unsigned lineNumber, unsigned columnNumber, V8StackTrace* stackTrace)
{
    LocalFrame* frame = WeakIdentifierMap<LocalFrame>::lookup(contextGroupId);
    if (!frame)
        return;
    if (type == V8ConsoleAPIType::kClear && frame->host())
        frame->host()->consoleMessageStorage().clear();
    // TODO(dgozman): do not wrap in ConsoleMessage.
    ConsoleMessage* consoleMessage = ConsoleMessage::create(ConsoleAPIMessageSource, consoleAPITypeToMessageLevel(type), message, SourceLocation::create(url, lineNumber, columnNumber, stackTrace ? stackTrace->clone() : nullptr, 0));
    frame->console().reportMessageToClient(consoleMessage);
}

v8::MaybeLocal<v8::Value> MainThreadDebugger::memoryInfo(v8::Isolate* isolate, v8::Local<v8::Context> context)
{
    ExecutionContext* executionContext = toExecutionContext(context);
    ASSERT_UNUSED(executionContext, executionContext);
    ASSERT(executionContext->isDocument());
    return toV8(MemoryInfo::create(), context->Global(), isolate);
}

void MainThreadDebugger::installAdditionalCommandLineAPI(v8::Local<v8::Context> context, v8::Local<v8::Object> object)
{
    ThreadDebugger::installAdditionalCommandLineAPI(context, object);
    createFunctionProperty(context, object, "$", MainThreadDebugger::querySelectorCallback, "function $(selector, [startNode]) { [Command Line API] }");
    createFunctionProperty(context, object, "$$", MainThreadDebugger::querySelectorAllCallback, "function $$(selector, [startNode]) { [Command Line API] }");
    createFunctionProperty(context, object, "$x", MainThreadDebugger::xpathSelectorCallback, "function $x(xpath, [startNode]) { [Command Line API] }");
}

static Node* secondArgumentAsNode(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() > 1) {
        if (Node* node = V8Node::toImplWithTypeCheck(info.GetIsolate(), info[1]))
            return node;
    }
    ExecutionContext* executionContext = toExecutionContext(info.GetIsolate()->GetCurrentContext());
    if (executionContext->isDocument())
        return toDocument(executionContext);
    return nullptr;
}

void MainThreadDebugger::querySelectorCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1)
        return;
    String selector = toCoreStringWithUndefinedOrNullCheck(info[0]);
    if (selector.isEmpty())
        return;
    Node* node = secondArgumentAsNode(info);
    if (!node || !node->isContainerNode())
        return;
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "$", "CommandLineAPI", info.Holder(), info.GetIsolate());
    Element* element = toContainerNode(node)->querySelector(AtomicString(selector), exceptionState);
    if (exceptionState.throwIfNeeded())
        return;
    if (element)
        info.GetReturnValue().Set(toV8(element, info.Holder(), info.GetIsolate()));
    else
        info.GetReturnValue().Set(v8::Null(info.GetIsolate()));
}

void MainThreadDebugger::querySelectorAllCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1)
        return;
    String selector = toCoreStringWithUndefinedOrNullCheck(info[0]);
    if (selector.isEmpty())
        return;
    Node* node = secondArgumentAsNode(info);
    if (!node || !node->isContainerNode())
        return;
    ExceptionState exceptionState(ExceptionState::ExecutionContext, "$$", "CommandLineAPI", info.Holder(), info.GetIsolate());
    // toV8(elementList) doesn't work here, since we need a proper Array instance, not NodeList.
    StaticElementList* elementList = toContainerNode(node)->querySelectorAll(AtomicString(selector), exceptionState);
    if (exceptionState.throwIfNeeded() || !elementList)
        return;
    v8::Isolate* isolate = info.GetIsolate();
    v8::Local<v8::Context> context = isolate->GetCurrentContext();
    v8::Local<v8::Array> nodes = v8::Array::New(isolate, elementList->length());
    for (size_t i = 0; i < elementList->length(); ++i) {
        Element* element = elementList->item(i);
        if (!nodes->Set(context, i, toV8(element, info.Holder(), info.GetIsolate())).FromMaybe(false))
            return;
    }
    info.GetReturnValue().Set(nodes);
}

void MainThreadDebugger::xpathSelectorCallback(const v8::FunctionCallbackInfo<v8::Value>& info)
{
    if (info.Length() < 1)
        return;
    String selector = toCoreStringWithUndefinedOrNullCheck(info[0]);
    if (selector.isEmpty())
        return;
    Node* node = secondArgumentAsNode(info);
    if (!node || !node->isContainerNode())
        return;

    ExceptionState exceptionState(ExceptionState::ExecutionContext, "$x", "CommandLineAPI", info.Holder(), info.GetIsolate());
    XPathResult* result = XPathEvaluator::create()->evaluate(selector, node, nullptr, XPathResult::kAnyType, ScriptValue(), exceptionState);
    if (exceptionState.throwIfNeeded() || !result)
        return;
    if (result->resultType() == XPathResult::kNumberType) {
        info.GetReturnValue().Set(toV8(result->numberValue(exceptionState), info.Holder(), info.GetIsolate()));
    } else if (result->resultType() == XPathResult::kStringType) {
        info.GetReturnValue().Set(toV8(result->stringValue(exceptionState), info.Holder(), info.GetIsolate()));
    } else if (result->resultType() == XPathResult::kBooleanType) {
        info.GetReturnValue().Set(toV8(result->booleanValue(exceptionState), info.Holder(), info.GetIsolate()));
    } else {
        v8::Isolate* isolate = info.GetIsolate();
        v8::Local<v8::Context> context = isolate->GetCurrentContext();
        v8::Local<v8::Array> nodes = v8::Array::New(isolate);
        size_t index = 0;
        while (Node* node = result->iterateNext(exceptionState)) {
            if (exceptionState.throwIfNeeded())
                return;
            if (!nodes->Set(context, index++, toV8(node, info.Holder(), info.GetIsolate())).FromMaybe(false))
                return;
        }
        info.GetReturnValue().Set(nodes);
    }
    exceptionState.throwIfNeeded();
}

} // namespace blink
