// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/ConsoleMessage.h"

#include "bindings/core/v8/ScriptCallStack.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/inspector/ScriptArguments.h"
#include "wtf/CurrentTime.h"
#include "wtf/PassOwnPtr.h"

namespace blink {

unsigned nextMessageId()
{
    struct MessageId {
        MessageId() : value(0) { }
        unsigned value;
    };

    DEFINE_THREAD_SAFE_STATIC_LOCAL(WTF::ThreadSpecific<MessageId>, messageId, new WTF::ThreadSpecific<MessageId>);
    return ++messageId->value;
}

// static
ConsoleMessage* ConsoleMessage::create(MessageSource source, MessageLevel level, const String& message, const String& url, unsigned lineNumber, unsigned columnNumber, PassRefPtr<ScriptCallStack> passCallStack, int scriptId, ScriptArguments* arguments)
{
    RefPtr<ScriptCallStack> callStack = passCallStack;
    if (callStack && !callStack->isEmpty() && (!scriptId || !lineNumber))
        return new ConsoleMessage(source, level, message, callStack->topSourceURL(), callStack->topLineNumber(), callStack->topColumnNumber(), callStack, 0, arguments);
    return new ConsoleMessage(source, level, message, url, lineNumber, columnNumber, callStack, scriptId, arguments);
}

// static
ConsoleMessage* ConsoleMessage::create(MessageSource source, MessageLevel level, const String& message, const String& url, unsigned lineNumber, unsigned columnNumber)
{
    return ConsoleMessage::create(source, level, message, url, lineNumber, columnNumber, nullptr, 0);
}

// static
ConsoleMessage* ConsoleMessage::createWithCallStack(MessageSource source, MessageLevel level, const String& message, const String& url, unsigned lineNumber, unsigned columnNumber)
{
    return ConsoleMessage::create(source, level, message, url, lineNumber, columnNumber, ScriptCallStack::captureForConsole(), 0);
}

// static
ConsoleMessage* ConsoleMessage::create(MessageSource source, MessageLevel level, const String& message)
{
    return ConsoleMessage::createWithCallStack(source, level, message, String(), 0, 0);
}

// static
ConsoleMessage* ConsoleMessage::createForRequest(MessageSource source, MessageLevel level, const String& message, const String& url, unsigned long requestIdentifier)
{
    ConsoleMessage* consoleMessage = ConsoleMessage::createWithCallStack(source, level, message, url, 0, 0);
    consoleMessage->m_requestIdentifier = requestIdentifier;
    return consoleMessage;
}

// static
ConsoleMessage* ConsoleMessage::createForConsoleAPI(MessageLevel level, MessageType type, const String& message, ScriptArguments* arguments)
{
    ConsoleMessage* consoleMessage = ConsoleMessage::create(ConsoleAPIMessageSource, level, message, String(), 0, 0, ScriptCallStack::captureForConsole(), 0, arguments);
    consoleMessage->m_type = type;
    return consoleMessage;
}

ConsoleMessage::ConsoleMessage(MessageSource source,
    MessageLevel level,
    const String& message,
    const String& url,
    unsigned lineNumber,
    unsigned columnNumber,
    PassRefPtr<ScriptCallStack> callStack,
    int scriptId,
    ScriptArguments* arguments)
    : m_source(source)
    , m_level(level)
    , m_type(LogMessageType)
    , m_message(message)
    , m_scriptId(scriptId)
    , m_url(url)
    , m_lineNumber(lineNumber)
    , m_columnNumber(columnNumber)
    , m_callStack(callStack)
    , m_scriptArguments(arguments)
    , m_requestIdentifier(0)
    , m_timestamp(WTF::currentTime())
    , m_messageId(0)
    , m_relatedMessageId(0)
{
}

ConsoleMessage::~ConsoleMessage()
{
}

MessageType ConsoleMessage::type() const
{
    return m_type;
}

int ConsoleMessage::scriptId() const
{
    return m_scriptId;
}

const String& ConsoleMessage::url() const
{
    return m_url;
}

unsigned ConsoleMessage::lineNumber() const
{
    return m_lineNumber;
}

unsigned ConsoleMessage::columnNumber() const
{
    return m_columnNumber;
}

PassRefPtr<ScriptCallStack> ConsoleMessage::callStack() const
{
    return m_callStack;
}

ScriptArguments* ConsoleMessage::scriptArguments() const
{
    return m_scriptArguments;
}

unsigned long ConsoleMessage::requestIdentifier() const
{
    return m_requestIdentifier;
}

double ConsoleMessage::timestamp() const
{
    return m_timestamp;
}

unsigned ConsoleMessage::assignMessageId()
{
    if (!m_messageId)
        m_messageId = nextMessageId();
    return m_messageId;
}

MessageSource ConsoleMessage::source() const
{
    return m_source;
}

MessageLevel ConsoleMessage::level() const
{
    return m_level;
}

const String& ConsoleMessage::message() const
{
    return m_message;
}

void ConsoleMessage::frameWindowDiscarded(LocalDOMWindow* window)
{
    if (!m_scriptArguments)
        return;
    if (m_scriptArguments->getScriptState()->domWindow() != window)
        return;
    if (!m_message)
        m_message = "<message collected>";
    m_scriptArguments.clear();
}

unsigned ConsoleMessage::argumentCount()
{
    if (m_scriptArguments)
        return m_scriptArguments->argumentCount();
    return 0;
}

DEFINE_TRACE(ConsoleMessage)
{
    visitor->trace(m_scriptArguments);
}

} // namespace blink
