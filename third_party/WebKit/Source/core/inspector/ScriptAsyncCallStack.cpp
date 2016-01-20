// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/ScriptAsyncCallStack.h"

namespace blink {

PassRefPtr<ScriptAsyncCallStack> ScriptAsyncCallStack::create(const String& description, PassRefPtr<ScriptCallStack> callStack, PassRefPtr<ScriptAsyncCallStack> asyncStackTrace)
{
    return adoptRef(new ScriptAsyncCallStack(description, callStack, asyncStackTrace));
}

ScriptAsyncCallStack::ScriptAsyncCallStack(const String& description, PassRefPtr<ScriptCallStack> callStack, PassRefPtr<ScriptAsyncCallStack> asyncStackTrace)
    : m_description(description)
    , m_callStack(callStack)
    , m_asyncStackTrace(asyncStackTrace)
{
    ASSERT(m_callStack);
}

ScriptAsyncCallStack::~ScriptAsyncCallStack()
{
}

PassRefPtr<TypeBuilder::Console::AsyncStackTrace> ScriptAsyncCallStack::buildInspectorObject() const
{
    RefPtr<TypeBuilder::Console::AsyncStackTrace> result = TypeBuilder::Console::AsyncStackTrace::create()
        .setCallFrames(m_callStack->buildInspectorArray())
        .release();
    result->setDescription(m_description);
    if (m_asyncStackTrace)
        result->setAsyncStackTrace(m_asyncStackTrace->buildInspectorObject());
    return result.release();
}

} // namespace blink
