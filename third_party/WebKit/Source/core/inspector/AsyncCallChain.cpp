// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/inspector/AsyncCallChain.h"

#include "wtf/text/WTFString.h"

namespace blink {

AsyncCallStack::AsyncCallStack(const String& description, v8::Local<v8::Object> callFrames)
    : m_description(description)
    , m_callFrames(callFrames->GetIsolate(), callFrames)
{
}

AsyncCallStack::~AsyncCallStack()
{
}

PassRefPtr<AsyncCallChain> AsyncCallChain::create(v8::Local<v8::Context> creationContext, PassRefPtr<AsyncCallStack> stack, AsyncCallChain* prevChain, unsigned asyncCallChainMaxLength)
{
    return adoptRef(new AsyncCallChain(creationContext, stack, prevChain, asyncCallChainMaxLength));
}

AsyncCallChain::AsyncCallChain(v8::Local<v8::Context> creationContext, PassRefPtr<AsyncCallStack> stack, AsyncCallChain* prevChain, unsigned asyncCallChainMaxLength)
{
    m_creationContext.Reset(creationContext->GetIsolate(), creationContext);
    if (stack)
        m_callStacks.append(stack);
    if (prevChain) {
        const AsyncCallStackVector& other = prevChain->m_callStacks;
        for (size_t i = 0; i < other.size() && m_callStacks.size() < asyncCallChainMaxLength; i++)
            m_callStacks.append(other[i]);
    }
}

AsyncCallChain::~AsyncCallChain()
{
}

} // namespace blink
