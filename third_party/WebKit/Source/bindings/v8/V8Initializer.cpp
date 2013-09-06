/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "bindings/v8/V8Initializer.h"

#include "V8DOMException.h"
#include "V8ErrorEvent.h"
#include "V8History.h"
#include "V8Location.h"
#include "V8Window.h"
#include "bindings/v8/DOMWrapperWorld.h"
#include "bindings/v8/ScriptCallStackFactory.h"
#include "bindings/v8/ScriptController.h"
#include "bindings/v8/ScriptProfiler.h"
#include "bindings/v8/V8Binding.h"
#include "bindings/v8/V8ErrorHandler.h"
#include "bindings/v8/V8GCController.h"
#include "bindings/v8/V8HiddenPropertyName.h"
#include "bindings/v8/V8PerContextData.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/inspector/ScriptCallStack.h"
#include "core/page/ConsoleTypes.h"
#include "core/page/ContentSecurityPolicy.h"
#include "core/page/DOMWindow.h"
#include "core/page/Frame.h"
#include "core/platform/MemoryUsageSupport.h"
#include "wtf/RefPtr.h"
#include "wtf/text/WTFString.h"
#include <v8-debug.h>

namespace WebCore {

static Frame* findFrame(v8::Local<v8::Object> host, v8::Local<v8::Value> data, v8::Isolate* isolate)
{
    WrapperTypeInfo* type = WrapperTypeInfo::unwrap(data);

    if (V8Window::info.equals(type)) {
        v8::Handle<v8::Object> windowWrapper = host->FindInstanceInPrototypeChain(V8Window::GetTemplate(isolate, worldTypeInMainThread(isolate)));
        if (windowWrapper.IsEmpty())
            return 0;
        return V8Window::toNative(windowWrapper)->frame();
    }

    if (V8History::info.equals(type))
        return V8History::toNative(host)->frame();

    if (V8Location::info.equals(type))
        return V8Location::toNative(host)->frame();

    // This function can handle only those types listed above.
    ASSERT_NOT_REACHED();
    return 0;
}

static void reportFatalErrorInMainThread(const char* location, const char* message)
{
    int memoryUsageMB = MemoryUsageSupport::actualMemoryUsageMB();
    printf("V8 error: %s (%s).  Current memory usage: %d MB\n", message, location, memoryUsageMB);
    CRASH();
}

static void messageHandlerInMainThread(v8::Handle<v8::Message> message, v8::Handle<v8::Value> data)
{
    DOMWindow* firstWindow = firstDOMWindow();
    if (!firstWindow->isCurrentlyDisplayedInFrame())
        return;

    String errorMessage = toWebCoreString(message->Get());

    v8::Handle<v8::StackTrace> stackTrace = message->GetStackTrace();
    RefPtr<ScriptCallStack> callStack;
    // Currently stack trace is only collected when inspector is open.
    if (!stackTrace.IsEmpty() && stackTrace->GetFrameCount() > 0)
        callStack = createScriptCallStack(stackTrace, ScriptCallStack::maxCallStackSizeToCapture, v8::Isolate::GetCurrent());

    v8::Handle<v8::Value> resourceName = message->GetScriptResourceName();
    bool shouldUseDocumentURL = resourceName.IsEmpty() || !resourceName->IsString();
    String resource = shouldUseDocumentURL ? firstWindow->document()->url() : toWebCoreString(resourceName);
    AccessControlStatus corsStatus = message->IsSharedCrossOrigin() ? SharableCrossOrigin : NotSharableCrossOrigin;

    RefPtr<ErrorEvent> event = ErrorEvent::create(errorMessage, resource, message->GetLineNumber(), message->GetStartColumn(), DOMWrapperWorld::current());
    if (V8DOMWrapper::isDOMWrapper(data)) {
        v8::Handle<v8::Object> obj = v8::Handle<v8::Object>::Cast(data);
        WrapperTypeInfo* type = toWrapperTypeInfo(obj);
        if (V8DOMException::info.isSubclass(type)) {
            DOMException* exception = V8DOMException::toNative(obj);
            if (exception && !exception->messageForConsole().isEmpty())
                event->setUnsanitizedMessage("Uncaught " + exception->toStringForConsole());
        }
    }

    // This method might be called while we're creating a new context. In this case, we
    // avoid storing the exception object, as we can't create a wrapper during context creation.
    DOMWrapperWorld* world = DOMWrapperWorld::current();
    Frame* frame = firstWindow->document()->frame();
    if (world && frame && frame->script()->existingWindowShell(world))
        V8ErrorHandler::storeExceptionOnErrorEventWrapper(event.get(), data, v8::Isolate::GetCurrent());
    firstWindow->document()->reportException(event.release(), callStack, corsStatus);
}

static void failedAccessCheckCallbackInMainThread(v8::Local<v8::Object> host, v8::AccessType type, v8::Local<v8::Value> data)
{
    Frame* target = findFrame(host, data, v8::Isolate::GetCurrent());
    if (!target)
        return;
    DOMWindow* targetWindow = target->domWindow();

    ExceptionState es(v8::Isolate::GetCurrent());
    es.throwSecurityError(targetWindow->sanitizedCrossDomainAccessErrorMessage(activeDOMWindow()), targetWindow->crossDomainAccessErrorMessage(activeDOMWindow()));
    es.throwIfNeeded();
}

static bool codeGenerationCheckCallbackInMainThread(v8::Local<v8::Context> context)
{
    if (ScriptExecutionContext* scriptExecutionContext = toScriptExecutionContext(context)) {
        if (ContentSecurityPolicy* policy = toDocument(scriptExecutionContext)->contentSecurityPolicy())
            return policy->allowEval(ScriptState::forContext(context));
    }
    return false;
}

static void initializeV8Common()
{
    v8::V8::AddGCPrologueCallback(V8GCController::gcPrologue);
    v8::V8::AddGCEpilogueCallback(V8GCController::gcEpilogue);
    v8::V8::IgnoreOutOfMemoryException();

    v8::Debug::SetLiveEditEnabled(false);
}

void V8Initializer::initializeMainThreadIfNeeded(v8::Isolate* isolate)
{
    ASSERT(isMainThread());

    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;

    initializeV8Common();

    v8::V8::SetFatalErrorHandler(reportFatalErrorInMainThread);
    v8::V8::AddMessageListener(messageHandlerInMainThread);
    v8::V8::SetFailedAccessCheckCallbackFunction(failedAccessCheckCallbackInMainThread);
    v8::V8::SetAllowCodeGenerationFromStringsCallback(codeGenerationCheckCallbackInMainThread);
    ScriptProfiler::initialize();
    V8PerIsolateData::ensureInitialized(isolate);
}

static void reportFatalErrorInWorker(const char* location, const char* message)
{
    // FIXME: We temporarily deal with V8 internal error situations such as out-of-memory by crashing the worker.
    CRASH();
}

static void messageHandlerInWorker(v8::Handle<v8::Message> message, v8::Handle<v8::Value> data)
{
    static bool isReportingException = false;
    // Exceptions that occur in error handler should be ignored since in that case
    // WorkerGlobalScope::reportException will send the exception to the worker object.
    if (isReportingException)
        return;
    isReportingException = true;

    // During the frame teardown, there may not be a valid context.
    if (ScriptExecutionContext* context = getScriptExecutionContext()) {
        String errorMessage = toWebCoreString(message->Get());
        String sourceURL = toWebCoreString(message->GetScriptResourceName());
        RefPtr<ErrorEvent> event = ErrorEvent::create(errorMessage, sourceURL, message->GetLineNumber(), message->GetStartColumn(), DOMWrapperWorld::current());
        AccessControlStatus corsStatus = message->IsSharedCrossOrigin() ? SharableCrossOrigin : NotSharableCrossOrigin;

        V8ErrorHandler::storeExceptionOnErrorEventWrapper(event.get(), data, v8::Isolate::GetCurrent());
        context->reportException(event.release(), 0, corsStatus);
    }

    isReportingException = false;
}

static const int kWorkerMaxStackSize = 500 * 1024;

void V8Initializer::initializeWorker(v8::Isolate* isolate)
{
    initializeV8Common();

    v8::V8::AddMessageListener(messageHandlerInWorker);
    v8::V8::SetFatalErrorHandler(reportFatalErrorInWorker);

    v8::ResourceConstraints resourceConstraints;
    uint32_t here;
    resourceConstraints.set_stack_limit(&here - kWorkerMaxStackSize / sizeof(uint32_t*));
    v8::SetResourceConstraints(&resourceConstraints);

    V8PerIsolateData::ensureInitialized(isolate);
}

} // namespace WebCore
