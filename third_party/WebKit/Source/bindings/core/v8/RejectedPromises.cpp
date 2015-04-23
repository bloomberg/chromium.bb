// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/RejectedPromises.h"

#include "bindings/core/v8/ScopedPersistent.h"
#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/ExecutionContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ScriptArguments.h"

namespace blink {

static const unsigned maxReportedHandlersPendingResolution = 1000;

class RejectedPromises::Message final : public NoBaseWillBeGarbageCollectedFinalized<RejectedPromises::Message> {
public:
    static PassOwnPtrWillBeRawPtr<Message> create(ScriptState* scriptState, v8::Handle<v8::Promise> promise, const ScriptValue& exception, const String& errorMessage, const String& resourceName, int scriptId, int lineNumber, int columnNumber, PassRefPtrWillBeRawPtr<ScriptCallStack> callStack)
    {
        return adoptPtrWillBeNoop(new Message(scriptState, promise, exception, errorMessage, resourceName, scriptId, lineNumber, columnNumber, callStack));
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_callStack);
    }

    v8::Handle<v8::Value> promise()
    {
        return m_promise.newLocal(m_scriptState->isolate());
    }

private:
    friend class RejectedPromises;

    Message(ScriptState* scriptState, v8::Handle<v8::Promise> promise, const ScriptValue& exception, const String& errorMessage, const String& resourceName, int scriptId, int lineNumber, int columnNumber, PassRefPtrWillBeRawPtr<ScriptCallStack> callStack)
        : m_scriptState(scriptState)
        , m_promise(scriptState->isolate(), promise)
        , m_exception(exception)
        , m_errorMessage(errorMessage)
        , m_resourceName(resourceName)
        , m_scriptId(scriptId)
        , m_lineNumber(lineNumber)
        , m_columnNumber(columnNumber)
        , m_callStack(callStack)
        , m_consoleMessageId(0)
        , m_collected(false)
    {
        m_promise.setWeak(this, &Message::didCollectPromise);
    }

    static void didCollectPromise(const v8::WeakCallbackInfo<Message>& data)
    {
        data.GetParameter()->m_collected = true;
        data.GetParameter()->m_promise.clear();
    }

    friend class RejectedPromises;

    ScriptState* m_scriptState;
    ScopedPersistent<v8::Promise> m_promise;
    const ScriptValue m_exception;
    const String m_errorMessage;
    const String m_resourceName;
    const int m_scriptId;
    const int m_lineNumber;
    const int m_columnNumber;
    const RefPtrWillBeMember<ScriptCallStack> m_callStack;
    unsigned m_consoleMessageId;
    bool m_collected;
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

void RejectedPromises::rejectedWithNoHandler(ScriptState* scriptState, v8::PromiseRejectMessage data, const String& errorMessage, const String& resourceName, int scriptId, int lineNumber, int columnNumber, PassRefPtrWillBeRawPtr<ScriptCallStack> callStack)
{
    OwnPtrWillBeRawPtr<Message> message = Message::create(scriptState, data.GetPromise(), ScriptValue(scriptState, data.GetValue()), errorMessage, resourceName, scriptId, lineNumber, columnNumber, callStack);
    m_queue.append(message.release());
}

void RejectedPromises::handlerAdded(v8::PromiseRejectMessage data)
{
    // First look it up in the pending messages and fast return, it'll be covered by processQueue().
    for (auto it = m_queue.begin(); it != m_queue.end(); ++it) {
        if ((*it)->m_collected)
            continue;
        ScriptState::Scope scope((*it)->m_scriptState);
        if ((*it)->promise() == data.GetPromise()) {
            m_queue.remove(it);
            return;
        }
    }

    // Then look it up in the reported errors.
    for (auto it = m_reportedAsErrors.begin(); it != m_reportedAsErrors.end(); ++it) {
        if ((*it)->m_collected)
            continue;
        ScriptState* scriptState = (*it)->m_scriptState;
        ScriptState::Scope scope(scriptState);
        v8::Handle<v8::Value> promise = (*it)->promise();
        if (promise != data.GetPromise())
            continue;

        ExecutionContext* executionContext = scriptState->executionContext();
        if (!executionContext)
            continue;

        RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = ConsoleMessage::create(JSMessageSource, RevokedErrorMessageLevel, "Handler added to rejected promise");
        consoleMessage->setRelatedMessageId((*it)->m_consoleMessageId);
        executionContext->addConsoleMessage(consoleMessage.release());
        m_reportedAsErrors.remove(it);
        break;
    }
}

void RejectedPromises::dispose()
{
    processQueue();
}

void RejectedPromises::processQueue()
{
    // Remove collected handlers.
    for (auto it = m_reportedAsErrors.begin(); it != m_reportedAsErrors.end(); ++it) {
        if ((*it)->m_collected)
            m_reportedAsErrors.remove(it);
    }

    while (!m_queue.isEmpty()) {
        OwnPtrWillBeRawPtr<Message> message = m_queue.takeFirst();
        if (message->m_collected)
            continue;
        ScriptState* scriptState = message->m_scriptState;
        if (!scriptState->contextIsValid())
            continue;
        // If execution termination has been triggered, quietly bail out.
        if (v8::V8::IsExecutionTerminating(scriptState->isolate()))
            continue;
        ExecutionContext* executionContext = scriptState->executionContext();
        if (!executionContext)
            continue;

        ScriptState::Scope scope(scriptState);

        v8::Handle<v8::Value> value = message->promise();
        // Either collected or https://crbug.com/450330
        if (value.IsEmpty() || !value->IsPromise())
            continue;
        ASSERT(!v8::Handle<v8::Promise>::Cast(value)->HasHandler());

        const String errorMessage = "Uncaught (in promise)";
        Vector<ScriptValue> args;
        args.append(ScriptValue(scriptState, v8String(scriptState->isolate(), errorMessage)));
        args.append(message->m_exception);
        RefPtrWillBeRawPtr<ScriptArguments> arguments = ScriptArguments::create(scriptState, args);

        String embedderErrorMessage = message->m_errorMessage;
        if (embedderErrorMessage.isEmpty())
            embedderErrorMessage = errorMessage;
        else if (embedderErrorMessage.startsWith("Uncaught "))
            embedderErrorMessage.insert(" (in promise)", 8);

        RefPtrWillBeRawPtr<ConsoleMessage> consoleMessage = ConsoleMessage::create(JSMessageSource, ErrorMessageLevel, embedderErrorMessage, message->m_resourceName, message->m_lineNumber, message->m_columnNumber);
        consoleMessage->setScriptArguments(arguments);
        consoleMessage->setCallStack(message->m_callStack);
        consoleMessage->setScriptId(message->m_scriptId);
        message->m_consoleMessageId = consoleMessage->assignMessageId();
        executionContext->addConsoleMessage(consoleMessage.release());

        m_reportedAsErrors.append(message.release());
        if (m_reportedAsErrors.size() > maxReportedHandlersPendingResolution)
            m_reportedAsErrors.removeFirst();
    }
}

} // namespace blink
