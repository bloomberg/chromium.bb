/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
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

#ifndef ExecutionContextTask_h
#define ExecutionContextTask_h

#include "wtf/FastAllocBase.h"
#include "wtf/Functional.h"
#include "wtf/Noncopyable.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace blink {

class ExecutionContext;

class ExecutionContextTask {
    WTF_MAKE_NONCOPYABLE(ExecutionContextTask);
    WTF_MAKE_FAST_ALLOCATED;
public:
    ExecutionContextTask() { }
    virtual ~ExecutionContextTask() { }
    virtual void performTask(ExecutionContext*) = 0;
    // Certain tasks get marked specially so that they aren't discarded, and are executed, when the context is shutting down its message queue.
    virtual bool isCleanupTask() const { return false; }
    virtual String taskNameForInstrumentation() const { return String(); }
};

class CallClosureTask FINAL : public ExecutionContextTask {
public:
    // Do not use |create| other than in createCrossThreadTask and
    // createSameThreadTask.
    // See http://crbug.com/390851
    static PassOwnPtr<CallClosureTask> create(const Closure& closure)
    {
        return adoptPtr(new CallClosureTask(closure));
    }
    virtual void performTask(ExecutionContext*) OVERRIDE { m_closure(); }

private:
    explicit CallClosureTask(const Closure& closure) : m_closure(closure) { }
    Closure m_closure;
};

// Create tasks passed within a single thread.
// When posting tasks within a thread, use |createSameThreadTask| instead
// of using |bind| directly to state explicitly that there is no need to care
// about thread safety when posting the task.
// When posting tasks across threads, use |createCrossThreadTask|.
template<typename FunctionType>
PassOwnPtr<ExecutionContextTask> createSameThreadTask(
    FunctionType function)
{
    return CallClosureTask::create(bind(function));
}

template<typename FunctionType, typename P1>
PassOwnPtr<ExecutionContextTask> createSameThreadTask(
    FunctionType function,
    const P1& parameter1)
{
    return CallClosureTask::create(bind(function,
        parameter1));
}

template<typename FunctionType, typename P1, typename P2>
PassOwnPtr<ExecutionContextTask> createSameThreadTask(
    FunctionType function,
    const P1& parameter1, const P2& parameter2)
{
    return CallClosureTask::create(bind(function,
        parameter1,
        parameter2));
}

template<typename FunctionType, typename P1, typename P2, typename P3>
PassOwnPtr<ExecutionContextTask> createSameThreadTask(
    FunctionType function,
    const P1& parameter1, const P2& parameter2, const P3& parameter3)
{
    return CallClosureTask::create(bind(function,
        parameter1,
        parameter2,
        parameter3));
}

template<typename FunctionType, typename P1, typename P2, typename P3, typename P4>
PassOwnPtr<ExecutionContextTask> createSameThreadTask(
    FunctionType function,
    const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4)
{
    return CallClosureTask::create(bind(function,
        parameter1,
        parameter2,
        parameter3,
        parameter4));
}

template<typename FunctionType, typename P1, typename P2, typename P3, typename P4, typename P5>
PassOwnPtr<ExecutionContextTask> createSameThreadTask(
    FunctionType function,
    const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5)
{
    return CallClosureTask::create(bind(function,
        parameter1,
        parameter2,
        parameter3,
        parameter4,
        parameter5));
}

template<typename FunctionType, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
PassOwnPtr<ExecutionContextTask> createSameThreadTask(
    FunctionType function,
    const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5, const P6& parameter6)
{
    return CallClosureTask::create(bind(function,
        parameter1,
        parameter2,
        parameter3,
        parameter4,
        parameter5,
        parameter6));
}

} // namespace

#endif
