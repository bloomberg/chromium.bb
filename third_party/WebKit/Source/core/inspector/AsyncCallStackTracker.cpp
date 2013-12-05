/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/inspector/AsyncCallStackTracker.h"

namespace WebCore {

#ifdef DEBUG_ASYNC_CALLS_DEBUGGER_SUPPORT
namespace {
unsigned totalAsyncCallStacks = 0;
}
#endif

AsyncCallStackTracker::AsyncCallStack::AsyncCallStack(const ScriptValue& callFrames)
    : m_callFrames(callFrames)
{
#ifdef DEBUG_ASYNC_CALLS_DEBUGGER_SUPPORT
    fprintf(stderr, "AsyncCallStack::AsyncCallStack() %u\n", ++totalAsyncCallStacks);
#endif
}

AsyncCallStackTracker::AsyncCallStack::~AsyncCallStack()
{
#ifdef DEBUG_ASYNC_CALLS_DEBUGGER_SUPPORT
    fprintf(stderr, "AsyncCallStack::~AsyncCallStack() %u\n", --totalAsyncCallStacks);
#endif
}

AsyncCallStackTracker::AsyncCallStackTracker()
    : m_maxAsyncCallStackDepth(0)
{
#ifdef DEBUG_ASYNC_CALLS_DEBUGGER_SUPPORT
    m_maxAsyncCallStackDepth = 4;
#endif
}

void AsyncCallStackTracker::setAsyncCallStackDepth(int depth)
{
    if (depth <= 0) {
        m_maxAsyncCallStackDepth = 0;
        clear();
    } else {
        m_maxAsyncCallStackDepth = depth;
    }
}

const AsyncCallStackTracker::AsyncCallChain* AsyncCallStackTracker::currentAsyncCallChain() const
{
    if (m_currentAsyncCallChain)
        ensureMaxAsyncCallChainDepth(m_currentAsyncCallChain.get(), m_maxAsyncCallStackDepth);
    return m_currentAsyncCallChain.get();
}

void AsyncCallStackTracker::didInstallTimer(int timerId, bool singleShot, const ScriptValue& callFrames)
{
    ASSERT(isEnabled());
    if (!validateCallFrames(callFrames))
        return;
    ASSERT(timerId > 0);
    m_timerCallChains.set(timerId, createAsyncCallChain(callFrames));
    if (!singleShot)
        m_intervalTimerIds.add(timerId);
}

void AsyncCallStackTracker::didRemoveTimer(int timerId)
{
    if (!isEnabled() || timerId <= 0)
        return;
    m_intervalTimerIds.remove(timerId);
    m_timerCallChains.remove(timerId);
}

void AsyncCallStackTracker::willFireTimer(int timerId)
{
    if (!isEnabled())
        return;
    ASSERT(timerId > 0);
    ASSERT(!m_currentAsyncCallChain);
    if (m_intervalTimerIds.contains(timerId))
        m_currentAsyncCallChain = m_timerCallChains.get(timerId);
    else
        m_currentAsyncCallChain = m_timerCallChains.take(timerId);
}

void AsyncCallStackTracker::didRequestAnimationFrame(int callbackId, const ScriptValue& callFrames)
{
    ASSERT(isEnabled());
    if (!validateCallFrames(callFrames))
        return;
    ASSERT(callbackId > 0);
    m_animationFrameCallChains.set(callbackId, createAsyncCallChain(callFrames));
}

void AsyncCallStackTracker::didCancelAnimationFrame(int callbackId)
{
    if (!isEnabled() || callbackId <= 0)
        return;
    m_animationFrameCallChains.remove(callbackId);
}

void AsyncCallStackTracker::willFireAnimationFrame(int callbackId)
{
    if (!isEnabled())
        return;
    ASSERT(callbackId > 0);
    ASSERT(!m_currentAsyncCallChain);
    m_currentAsyncCallChain = m_animationFrameCallChains.take(callbackId);
}

void AsyncCallStackTracker::didFireAsyncCall()
{
    m_currentAsyncCallChain = 0;
}

PassRefPtr<AsyncCallStackTracker::AsyncCallChain> AsyncCallStackTracker::createAsyncCallChain(const ScriptValue& callFrames)
{
    ASSERT(isEnabled());
    RefPtr<AsyncCallChain> chain = adoptRef(m_currentAsyncCallChain ? new AsyncCallStackTracker::AsyncCallChain(*m_currentAsyncCallChain) : new AsyncCallStackTracker::AsyncCallChain());
    ensureMaxAsyncCallChainDepth(chain.get(), m_maxAsyncCallStackDepth - 1);
    chain->m_callStacks.prepend(adoptRef(new AsyncCallStackTracker::AsyncCallStack(callFrames)));
    return chain.release();
}

void AsyncCallStackTracker::ensureMaxAsyncCallChainDepth(AsyncCallChain* chain, unsigned maxDepth)
{
    while (chain->m_callStacks.size() > maxDepth)
        chain->m_callStacks.removeLast();
}

bool AsyncCallStackTracker::validateCallFrames(const ScriptValue& callFrames)
{
    return !callFrames.hasNoValue() && callFrames.isObject();
}

void AsyncCallStackTracker::clear()
{
    m_currentAsyncCallChain = 0;
    m_intervalTimerIds.clear();
    m_timerCallChains.clear();
    m_animationFrameCallChains.clear();
}

} // namespace WebCore
