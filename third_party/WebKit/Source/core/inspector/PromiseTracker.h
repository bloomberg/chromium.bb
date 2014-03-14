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

#ifndef PromiseTracker_h
#define PromiseTracker_h

#include "bindings/v8/custom/V8PromiseCustom.h"
#include "bindings/v8/ScriptObjectTraits.h"
#include "core/inspector/ScriptCallStack.h"
#include "wtf/HashMap.h"
#include "wtf/Noncopyable.h"
#include "wtf/RefPtr.h"

namespace WebCore {

class ExecutionContext;

class PromiseTracker {
    WTF_MAKE_NONCOPYABLE(PromiseTracker);
public:
    PromiseTracker() : m_isEnabled(false) { }

    bool isEnabled() const { return m_isEnabled; }
    void enable();
    void disable();

    void clear();

    void didCreatePromise(const ScriptObject& promise);
    void didUpdatePromiseParent(const ScriptObject& promise, const ScriptObject& parentPromise);
    void didUpdatePromiseState(const ScriptObject& promise, V8PromiseCustom::PromiseState, const ScriptValue& result);

private:
    class PromiseData : public RefCounted<PromiseData> {
    public:
        PromiseData(const ScriptObject& promise, const ScriptObject& parentPromise, const ScriptValue& result, V8PromiseCustom::PromiseState state, double timestamp);

        void didSettlePromise(double timestamp);

        ScriptObject m_promise;
        ScriptObject m_parentPromise;
        ScriptValue m_result;
        V8PromiseCustom::PromiseState m_state;

        // Time in milliseconds.
        double m_timeOnCreate;
        double m_timeOnSettle;

        RefPtr<ScriptCallStack> m_callStackOnCreate;
        RefPtr<ScriptCallStack> m_callStackOnSettle;
    };

    bool m_isEnabled;
    typedef HashMap<ScriptObject, RefPtr<PromiseData>, ScriptObjectHash, ScriptObjectHashTraits> PromiseDataMap;
    PromiseDataMap m_promiseDataMap;
};

} // namespace WebCore

#endif // !defined(PromiseTracker_h)
