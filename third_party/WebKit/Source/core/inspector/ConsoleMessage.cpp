// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/ConsoleMessage.h"

#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/SourceLocation.h"
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
ConsoleMessage* ConsoleMessage::createForRequest(MessageSource source, MessageLevel level, const String& message, const String& url, unsigned long requestIdentifier)
{
    ConsoleMessage* consoleMessage = ConsoleMessage::create(source, level, message, SourceLocation::capture(url, 0, 0));
    consoleMessage->m_requestIdentifier = requestIdentifier;
    return consoleMessage;
}

// static
ConsoleMessage* ConsoleMessage::createForConsoleAPI(MessageLevel level, MessageType type, const String& message, ScriptArguments* arguments)
{
    ConsoleMessage* consoleMessage = ConsoleMessage::create(ConsoleAPIMessageSource, level, message, SourceLocation::capture(), arguments);
    consoleMessage->m_type = type;
    return consoleMessage;
}

// static
ConsoleMessage* ConsoleMessage::create(MessageSource source, MessageLevel level, const String& message, PassOwnPtr<SourceLocation> location, ScriptArguments* arguments)
{
    return new ConsoleMessage(source, level, message, std::move(location), arguments);
}

// static
ConsoleMessage* ConsoleMessage::create(MessageSource source, MessageLevel level, const String& message)
{
    return ConsoleMessage::create(source, level, message, SourceLocation::capture());
}

ConsoleMessage::ConsoleMessage(MessageSource source,
    MessageLevel level,
    const String& message,
    PassOwnPtr<SourceLocation> location,
    ScriptArguments* arguments)
    : m_source(source)
    , m_level(level)
    , m_type(LogMessageType)
    , m_message(message)
    , m_location(std::move(location))
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

SourceLocation* ConsoleMessage::location() const
{
    return m_location.get();
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
