// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/inspector/ConsoleMessageStorage.h"

#include "core/frame/LocalDOMWindow.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/InspectorConsoleInstrumentation.h"

namespace blink {

static const unsigned maximumConsoleMessages = 1000;
static const int expireConsoleMessagesStep = 100;

ConsoleMessageStorage::ConsoleMessageStorage(ExecutionContext* context)
    : m_expiredCount(0)
    , m_context(context)
    , m_frame(nullptr)
{
}

ConsoleMessageStorage::ConsoleMessageStorage(LocalFrame* frame)
    : m_expiredCount(0)
    , m_context(nullptr)
    , m_frame(frame)
{
}

void ConsoleMessageStorage::reportMessage(PassRefPtr<ConsoleMessage> prpMessage)
{
    RefPtr<ConsoleMessage> message = prpMessage;
    message->collectCallStack();

    InspectorInstrumentation::addMessageToConsole(executionContext(), message.get());

    m_messages.append(message);
    if (m_messages.size() >= maximumConsoleMessages) {
        m_expiredCount += expireConsoleMessagesStep;
        m_messages.remove(0, expireConsoleMessagesStep);
    }
}

void ConsoleMessageStorage::clear()
{
    m_messages.clear();
    m_expiredCount = 0;
}

Vector<unsigned> ConsoleMessageStorage::argumentCounts() const
{
    Vector<unsigned> result(m_messages.size());
    for (size_t i = 0; i < size(); ++i)
        result[i] = at(i)->argumentCount();
    return result;
}

void ConsoleMessageStorage::frameWindowDiscarded(LocalDOMWindow* window)
{
    for (size_t i = 0; i < size(); ++i)
        at(i)->frameWindowDiscarded(window);
}

size_t ConsoleMessageStorage::size() const
{
    return m_messages.size();
}

PassRefPtr<ConsoleMessage> ConsoleMessageStorage::at(size_t index) const
{
    return m_messages[index];
}

int ConsoleMessageStorage::expiredCount() const
{
    return m_expiredCount;
}

ExecutionContext* ConsoleMessageStorage::executionContext() const
{
    return m_frame ? m_frame->document() : m_context;
}

} // namespace WebCore
