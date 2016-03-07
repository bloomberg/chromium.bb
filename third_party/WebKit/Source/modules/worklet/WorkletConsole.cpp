// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/worklet/WorkletConsole.h"

#include "core/inspector/ConsoleMessage.h"
#include "modules/worklet/WorkletGlobalScope.h"


namespace blink {

WorkletConsole::WorkletConsole(WorkletGlobalScope* scope)
    : m_scope(scope)
{
}

WorkletConsole::~WorkletConsole()
{
}

void WorkletConsole::reportMessageToConsole(PassRefPtrWillBeRawPtr<ConsoleMessage> consoleMessage)
{
    m_scope->addConsoleMessage(consoleMessage);
}

ExecutionContext* WorkletConsole::context()
{
    if (!m_scope)
        return nullptr;
    return m_scope;
}

DEFINE_TRACE(WorkletConsole)
{
    visitor->trace(m_scope);
    ConsoleBase::trace(visitor);
}

} // namespace blink
