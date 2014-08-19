// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/inspector/ConsoleMessage.h"

#include "bindings/core/v8/ScriptValue.h"
#include "core/inspector/ScriptArguments.h"

namespace blink {

ConsoleMessage::ConsoleMessage(MessageSource source,
    MessageLevel level,
    const String& message,
    const String& url,
    unsigned lineNumber,
    unsigned columnNumber)
    : m_source(source)
    , m_level(level)
    , m_type(LogMessageType)
    , m_message(message)
    , m_url(url)
    , m_lineNumber(lineNumber)
    , m_columnNumber(columnNumber)
    , m_requestIdentifier(0)
    , m_workerProxy(nullptr)
{
}

ConsoleMessage::~ConsoleMessage()
{
}

MessageType ConsoleMessage::type() const
{
    return m_type;
}

void ConsoleMessage::setType(MessageType type)
{
    m_type = type;
}

const String& ConsoleMessage::url() const
{
    return m_url;
}

void ConsoleMessage::setURL(const String& url)
{
    m_url = url;
}

unsigned ConsoleMessage::lineNumber() const
{
    return m_lineNumber;
}

void ConsoleMessage::setLineNumber(unsigned lineNumber)
{
    m_lineNumber = lineNumber;
}

PassRefPtrWillBeRawPtr<ScriptCallStack> ConsoleMessage::callStack() const
{
    return m_callStack;
}

void ConsoleMessage::setCallStack(PassRefPtrWillBeRawPtr<ScriptCallStack> callStack)
{
    m_callStack = callStack;
}

ScriptState* ConsoleMessage::scriptState() const
{
    if (m_scriptState)
        return m_scriptState->get();
    return nullptr;
}

void ConsoleMessage::setScriptState(ScriptState* scriptState)
{
    if (scriptState)
        m_scriptState = adoptPtr(new ScriptStateProtectingContext(scriptState));
    else
        m_scriptState.clear();
}

PassRefPtrWillBeRawPtr<ScriptArguments> ConsoleMessage::scriptArguments() const
{
    return m_scriptArguments;
}

void ConsoleMessage::setScriptArguments(PassRefPtrWillBeRawPtr<ScriptArguments> scriptArguments)
{
    m_scriptArguments = scriptArguments;
}

unsigned long ConsoleMessage::requestIdentifier() const
{
    return m_requestIdentifier;
}

void ConsoleMessage::setRequestIdentifier(unsigned long requestIdentifier)
{
    m_requestIdentifier = requestIdentifier;
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

unsigned ConsoleMessage::columnNumber() const
{
    return m_columnNumber;
}

void ConsoleMessage::trace(Visitor* visitor)
{
    visitor->trace(m_callStack);
    visitor->trace(m_scriptArguments);
}

} // namespace blink
