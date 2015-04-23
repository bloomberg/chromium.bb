// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "bindings/core/v8/RejectedPromises.h"

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8Binding.h"
#include "core/dom/ExecutionContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ScriptArguments.h"

namespace blink {

class RejectedPromises::Message final : public NoBaseWillBeGarbageCollectedFinalized<RejectedPromises::Message> {
public:
    static PassOwnPtrWillBeRawPtr<Message> create(const ScriptValue& promise, const ScriptValue& exception, const String& errorMessage, const String& resourceName, int scriptId, int lineNumber, int columnNumber, PassRefPtrWillBeRawPtr<ScriptCallStack> callStack)
    {
        return adoptPtrWillBeNoop(new Message(promise, exception, errorMessage, resourceName, scriptId, lineNumber, columnNumber, callStack));
    }

    DEFINE_INLINE_TRACE()
    {
        visitor->trace(m_callStack);
    }

private:
    Message(const ScriptValue& promise, const ScriptValue& exception, const String& errorMessage, const String& resourceName, int scriptId, int lineNumber, int columnNumber, PassRefPtrWillBeRawPtr<ScriptCallStack> callStack)
        : m_promise(promise)
        , m_exception(exception)
        , m_errorMessage(errorMessage)
        , m_resourceName(resourceName)
        , m_scriptId(scriptId)
        , m_lineNumber(lineNumber)
        , m_columnNumber(columnNumber)
        , m_callStack(callStack)
    {
    }

    friend class RejectedPromises;

    const ScriptValue m_promise;
    const ScriptValue m_exception;
    const String m_errorMessage;
    const String m_resourceName;
    const int m_scriptId;
    const int m_lineNumber;
    const int m_columnNumber;
    const RefPtrWillBeMember<ScriptCallStack> m_callStack;
};

RejectedPromises::RejectedPromises()
{
}

DEFINE_EMPTY_DESTRUCTOR_WILL_BE_REMOVED(RejectedPromises);

DEFINE_TRACE(RejectedPromises)
{
    visitor->trace(m_queue);
}

void RejectedPromises::add(ScriptState* scriptState, v8::PromiseRejectMessage data, const String& errorMessage, const String& resourceName, int scriptId, int lineNumber, int columnNumber, PassRefPtrWillBeRawPtr<ScriptCallStack> callStack)
{
    v8::Handle<v8::Promise> promise = data.GetPromise();
    OwnPtrWillBeRawPtr<Message> message = Message::create(ScriptValue(scriptState, promise), ScriptValue(scriptState, data.GetValue()), errorMessage, resourceName, scriptId, lineNumber, columnNumber, callStack);

    m_queue.append(message.release());
}

void RejectedPromises::dispose()
{
    processQueue();
}

void RejectedPromises::processQueue()
{
    while (!m_queue.isEmpty()) {
        OwnPtrWillBeRawPtr<Message> message = m_queue.takeFirst();
        ScriptState* scriptState = message->m_promise.scriptState();
        if (!scriptState->contextIsValid())
            continue;
        // If execution termination has been triggered, quietly bail out.
        if (v8::V8::IsExecutionTerminating(scriptState->isolate()))
            continue;
        ExecutionContext* executionContext = scriptState->executionContext();
        if (!executionContext)
            continue;

        ScriptState::Scope scope(scriptState);

        ASSERT(!message->m_promise.isEmpty());
        v8::Handle<v8::Value> value = message->m_promise.v8Value();
        // https://crbug.com/450330
        if (value.IsEmpty() || !value->IsPromise())
            continue;
        if (v8::Handle<v8::Promise>::Cast(value)->HasHandler())
            continue;

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
        executionContext->addConsoleMessage(consoleMessage.release());
    }
}

} // namespace blink
