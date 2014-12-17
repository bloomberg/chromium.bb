// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/inspector/AsyncCallChain.h"

#include "wtf/text/WTFString.h"

namespace blink {

void AsyncCallChain::trace(Visitor* visitor)
{
    visitor->trace(m_callStacks);
}

AsyncCallStack::AsyncCallStack(const String& description, const ScriptValue& callFrames)
    : m_description(description)
    , m_callFrames(callFrames)
{
}

AsyncCallStack::~AsyncCallStack()
{
}

AsyncCallChain::~AsyncCallChain()
{
}

void AsyncCallChain::ensureMaxAsyncCallChainDepth(unsigned maxDepth)
{
    while (m_callStacks.size() > maxDepth)
        m_callStacks.removeLast();
}

} // namespace blink
