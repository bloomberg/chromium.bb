/*
 * Copyright (C) 2014 Google Inc. All rights reserved.
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

#include "bindings/v8/ScriptCallStackFactory.h"
#include "core/inspector/PromiseTracker.h"
#include "wtf/CurrentTime.h"

namespace WebCore {

PromiseTracker::PromiseData::PromiseData(const ScriptObject& promise, const ScriptObject& parentPromise, const ScriptValue& result, V8PromiseCustom::PromiseState state, double timestamp)
    : m_promise(promise)
    , m_parentPromise(parentPromise)
    , m_result(result)
    , m_state(state)
    , m_timeOnCreate(timestamp)
    , m_timeOnSettle(-1)
    , m_callStackOnCreate(createScriptCallStack(ScriptCallStack::maxCallStackSizeToCapture, true))
{
}

void PromiseTracker::PromiseData::didSettlePromise(double timestamp)
{
    m_timeOnSettle = timestamp;
    m_callStackOnSettle = createScriptCallStack(ScriptCallStack::maxCallStackSizeToCapture, true);
}

void PromiseTracker::enable()
{
    m_isEnabled = true;
}

void PromiseTracker::disable()
{
    m_isEnabled = false;
    clear();
}

void PromiseTracker::clear()
{
    m_promiseDataMap.clear();
}

void PromiseTracker::didCreatePromise(const ScriptObject& promise)
{
    ASSERT(isEnabled());

    double timestamp = currentTimeMS();
    m_promiseDataMap.set(promise, adoptRef(new PromiseData(promise, ScriptObject(), ScriptValue(), V8PromiseCustom::Pending, timestamp)));
}

void PromiseTracker::didUpdatePromiseParent(const ScriptObject& promise, const ScriptObject& parentPromise)
{
    ASSERT(isEnabled());

    RefPtr<PromiseData> data = m_promiseDataMap.get(promise);
    ASSERT(data != NULL && data->m_promise == promise);
    data->m_parentPromise = parentPromise;
}

void PromiseTracker::didUpdatePromiseState(const ScriptObject& promise, V8PromiseCustom::PromiseState state, const ScriptValue& result)
{
    ASSERT(isEnabled());

    double timestamp = currentTimeMS();
    RefPtr<PromiseData> data = m_promiseDataMap.get(promise);
    ASSERT(data != NULL && data->m_promise == promise);
    data->m_state = state;
    data->m_result = result;
    if (state == V8PromiseCustom::Fulfilled || state == V8PromiseCustom::Rejected)
        data->didSettlePromise(timestamp);
}

} // namespace WebCore
