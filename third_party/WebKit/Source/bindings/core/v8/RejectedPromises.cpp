// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/RejectedPromises.h"

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/ExecutionContext.h"
#include "core/events/EventTarget.h"
#include "core/events/PromiseRejectionEvent.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ScriptArguments.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/Task.h"
#include "public/platform/Platform.h"
#include "public/platform/WebScheduler.h"
#include "public/platform/WebTaskRunner.h"
#include "public/platform/WebThread.h"
#include "wtf/Functional.h"

namespace blink {

static const unsigned maxReportedHandlersPendingResolution = 1000;

class RejectedPromises::Message final : public NoBaseWillBeGarbageCollectedFinalized<RejectedPromises::Message> {
public:
    static PassOwnPtrWillBeRawPtr<Message> create(ScriptState* scriptState, v8::Local<v8::Promise> promise, v8::Local<v8::Value> exception, const String& errorMessage, const String& resourceName, int scriptId, int lineNumber, int columnNumber, PassRefPtrWillBeRawPtr<ScriptCallStack> callStack, AccessControlStatus corsStatus)
    {
        return adoptPtrWillBeNoop(new Message(scriptState, promise, exception, errorMessage, resourceName, scriptId, lineNumber, columnNumber, callStack, corsStatus));
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_callStack);
    }

    bool isCollected()
    {
        return m_collected || !m_scriptState->contextIsValid();
    }

    bool hasPromise(v8::Local<v8::Value> promise)
    {
        ScriptState::Scope scope(m_scriptState);
        return promise == m_promise.newLocal(m_scriptState->isolate());
    }

    void report()
    {
        if (!m_scriptState->contextIsValid())
            return;
        // If execution termination has been triggered, quietly bail out.
        if (m_scriptState->isolate()->IsExecutionTerminating())
            return;
        ExecutionContext* executionContext = m_scriptState->executionContext();
        if (!executionContext)
            return;

        ScriptState::Scope scope(m_scriptState);
        v8::Local<v8::Value> value = m_promise.newLocal(m_scriptState->isolate());
        v8::Local<v8::Value> reason = m_exception.newLocal(m_scriptState->isolate());
        // Either collected or https://crbug.com/450330
        if (value.IsEmpty() || !value->IsPromise())
            return;
        ASSERT(!hasHandler());

        EventTarget* target = executionContext->errorEventTarget();
        if (RuntimeEnabledFeatures::promiseRejectionEventEnabled() && target && !executionContext->shouldSanitizeScriptError(m_resourceName, m_corsStatus)) {
            PromiseRejectionEventInit init;
            init.setPromise(ScriptPromise(m_scriptState, value));
            init.setReason(ScriptValue(m_scriptState, reason));
            init.setCancelable(true);
            RefPtrWillBeRawPtr<PromiseRejectionEvent> event = PromiseRejectionEvent::create(m_scriptState, EventTypeNames::unhandledrejection, init);
            // Log to console if event was not preventDefault()'ed.
            m_shouldLogToConsole = target->dispatchEvent(event);
        }

        if (m_shouldLogToConsole) {
            const String errorMessage = "Uncaught (in promise)";
            Vector<ScriptValue> args;
            args.append(ScriptValue(m_scriptState, v8String(m_scriptState->isolate(), errorMessage)));
            args.append(ScriptValue(m_scriptState, reason));
            RefPtrWillBeRawPtr<ScriptArguments> arguments = ScriptArguments::create(m_scriptState, args);

            String embedderErrorMessage = m_errorMessage;
            if (embedderErrorMessage.isEmpty())
                embedderErrorMessage = errorMessage;
            else if (embedderErrorMessage.startsWith("Uncaught "))
                embedderErrorMessage.insert(" (in promise)", 8);

            RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, embedderErrorMessage, m_resourceName, m_lineNumber, m_columnNumber);
            consoleMessage->setScriptArguments(arguments);
            consoleMessage->setCallStack(m_callStack);
            consoleMessage->setScriptId(m_scriptId);
            m_consoleMessageId = consoleMessage->assignMessageId();
            executionContext->addConsoleMessage(consoleMessage.release());
        }

        m_callStack.clear();
    }

    void revoke()
    {
        ExecutionContext* executionContext = m_scriptState->executionContext();
        if (!executionContext)
            return;

        ScriptState::Scope scope(m_scriptState);
        v8::Local<v8::Value> value = m_promise.newLocal(m_scriptState->isolate());
        v8::Local<v8::Value> reason = m_exception.newLocal(m_scriptState->isolate());
        // Either collected or https://crbug.com/450330
        if (value.IsEmpty() || !value->IsPromise())
            return;

        EventTarget* target = executionContext->errorEventTarget();
        if (RuntimeEnabledFeatures::promiseRejectionEventEnabled() && target && !executionContext->shouldSanitizeScriptError(m_resourceName, m_corsStatus)) {
            PromiseRejectionEventInit init;
            init.setPromise(ScriptPromise(m_scriptState, value));
            init.setReason(ScriptValue(m_scriptState, reason));
            RefPtrWillBeRawPtr<PromiseRejectionEvent> event = PromiseRejectionEvent::create(m_scriptState, EventTypeNames::rejectionhandled, init);
            target->dispatchEvent(event);
        }

        if (m_shouldLogToConsole) {
            RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = ConsoleMessage::create(JSMessageSource, RevokedErrorMessageLevel, "Handler added to rejected promise");
            consoleMessage->setRelatedMessageId(m_consoleMessageId);
            executionContext->addConsoleMessage(consoleMessage.release());
        }
    }

    void makePromiseWeak()
    {
        ASSERT(!m_promise.isEmpty() && !m_promise.isWeak());
        m_promise.setWeak(this, &Message::didCollectPromise);
        m_exception.setWeak(this, &Message::didCollectException);
    }

    void makePromiseStrong()
    {
        ASSERT(!m_promise.isEmpty() && m_promise.isWeak());
        m_promise.clearWeak();
        m_exception.clearWeak();
    }

    bool hasHandler()
    {
        ASSERT(!isCollected());
        ScriptState::Scope scope(m_scriptState);
        v8::Local<v8::Value> value = m_promise.newLocal(m_scriptState->isolate());
        return v8::Local<v8::Promise>::Cast(value)->HasHandler();
    }

private:
    Message(ScriptState* scriptState, v8::Local<v8::Promise> promise, v8::Local<v8::Value> exception, const String& errorMessage, const String& resourceName, int scriptId, int lineNumber, int columnNumber, PassRefPtrWillBeRawPtr<ScriptCallStack> callStack, AccessControlStatus corsStatus)
        : m_scriptState(scriptState)
        , m_promise(scriptState->isolate(), promise)
        , m_exception(scriptState->isolate(), exception)
        , m_errorMessage(errorMessage)
        , m_resourceName(resourceName)
        , m_scriptId(scriptId)
        , m_lineNumber(lineNumber)
        , m_columnNumber(columnNumber)
        , m_callStack(callStack)
        , m_consoleMessageId(0)
        , m_collected(false)
        , m_shouldLogToConsole(true)
        , m_corsStatus(corsStatus)
    {
    }

    static void didCollectPromise(const v8::WeakCallbackInfo<Message>& data)
    {
        data.GetParameter()->m_collected = true;
        data.GetParameter()->m_promise.clear();
    }

    static void didCollectException(const v8::WeakCallbackInfo<Message>& data)
    {
        data.GetParameter()->m_exception.clear();
    }

    ScriptState* m_scriptState;
    ScopedPersistent<v8::Promise> m_promise;
    ScopedPersistent<v8::Value> m_exception;
    String m_errorMessage;
    String m_resourceName;
    int m_scriptId;
    int m_lineNumber;
    int m_columnNumber;
    RefPtrWillBeMember<ScriptCallStack> m_callStack;
    unsigned m_consoleMessageId;
    bool m_collected;
    bool m_shouldLogToConsole;
    AccessControlStatus m_corsStatus;
};

RejectedPromises::RejectedPromises()
{
}

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(RejectedPromises);

DEFINE_TRACE(RejectedPromises)
{
    visitor->trace(m_queue);
    visitor->trace(m_reportedAsErrors);
}

void RejectedPromises::rejectedWithNoHandler(ScriptState* scriptState, v8::PromiseRejectMessage data, const String& errorMessage, const String& resourceName, int scriptId, int lineNumber, int columnNumber, PassRefPtrWillBeRawPtr<ScriptCallStack> callStack, AccessControlStatus corsStatus)
{
    m_queue.append(Message::create(scriptState, data.GetPromise(), data.GetValue(), errorMessage, resourceName, scriptId, lineNumber, columnNumber, callStack, corsStatus));
}

void RejectedPromises::handlerAdded(v8::PromiseRejectMessage data)
{
    // First look it up in the pending messages and fast return, it'll be covered by processQueue().
    for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
        if (!(*it)->isCollected() && (*it)->hasPromise(data.GetPromise())) {
            m_queue.remove(it);
            return;
        }
    }

    // Then look it up in the reported errors.
    for (size_t i = 0; i < m_reportedAsErrors.size(); ++i) {
        auto& message = m_reportedAsErrors.at(i);
        if (!message->isCollected() && message->hasPromise(data.GetPromise())) {
            message->makePromiseStrong();
            Platform::current()->currentThread()->scheduler()->timerTaskRunner()->postTask(BLINK_FROM_HERE, new Task(bind(&RejectedPromises::revokeNow, PassRefPtrWillBeRawPtr<RejectedPromises>(this), message.release())));
            m_reportedAsErrors.remove(i);
            return;
        }
    }
}

PassOwnPtrWillBeRawPtr<RejectedPromises::MessageQueue> RejectedPromises::createMessageQueue()
{
    return adoptPtrWillBeNoop(new MessageQueue());
}

void RejectedPromises::dispose()
{
    if (m_queue.isEmpty())
        return;

    OwnPtrWillBeRawPtr<MessageQueue> queue = createMessageQueue();
    queue->swap(m_queue);
    processQueueNow(queue.release());
}

void RejectedPromises::processQueue()
{
    if (m_queue.isEmpty())
        return;

    OwnPtrWillBeRawPtr<MessageQueue> queue = createMessageQueue();
    queue->swap(m_queue);
    Platform::current()->currentThread()->scheduler()->timerTaskRunner()->postTask(BLINK_FROM_HERE, new Task(bind(&RejectedPromises::processQueueNow, PassRefPtrWillBeRawPtr<RejectedPromises>(this), queue.release())));
}

void RejectedPromises::processQueueNow(PassOwnPtrWillBeRawPtr<MessageQueue> queue)
{
    // Remove collected handlers.
    for (size_t i = 0; i < m_reportedAsErrors.size();) {
        if (m_reportedAsErrors.at(i)->isCollected())
            m_reportedAsErrors.remove(i);
        else
            ++i;
    }

    while (!queue->isEmpty()) {
        OwnPtrWillBeRawPtr<Message> message = queue->takeFirst();
        if (message->isCollected())
            continue;
        if (!message->hasHandler()) {
            message->report();
            message->makePromiseWeak();
            m_reportedAsErrors.append(message.release());
            if (m_reportedAsErrors.size() > maxReportedHandlersPendingResolution)
                m_reportedAsErrors.remove(0, maxReportedHandlersPendingResolution / 10);
        }
    }
}

void RejectedPromises::revokeNow(PassOwnPtrWillBeRawPtr<Message> message)
{
    message->revoke();
}

} // namespace blink
