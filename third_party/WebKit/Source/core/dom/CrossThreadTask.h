/*
 * Copyright (C) 2009-2010 Google Inc. All rights reserved.
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

#ifndef CrossThreadTask_h
#define CrossThreadTask_h

#include "core/dom/ExecutionContext.h"
#include "core/dom/ExecutionContextTask.h"
#include "platform/CrossThreadCopier.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/TypeTraits.h"

namespace blink {

// createCrossThreadTask(...) is similar to but safer than
// CallClosureTask::create(bind(...)) for cross-thread task posting.
// postTask(CallClosureTask::create(bind(...))) is not thread-safe
// due to temporary objects, see http://crbug.com/390851 for details.
//
// Example: func1(42, str2) will be called, where |str2| is a deep copy of
//          |str| (created by str.isolatedCopy()).
//     void func1(int, const String&);
//     createCrossThreadTask(func1, 42, str);
//
// Don't (if you pass the closure across threads):
//     bind(func1, 42, str);
//     bind(func1, 42, str.isolatedCopy());
//
// createCrossThreadTask(func, ...) can be used for functions that take
// ExecutionContext* arguments.
// Note: ExecutionContext* argument must be the last argument of functions.
//
// Example: func2(42, str2, context) will be called,
//          where |context| is supplied by the target thread.
//     void func2(int, const String&, ExecutionContext*);
//     createCrossThreadTask(func2, 42, str);
//
// createCrossThreadTask copies its arguments into Closure
// by CrossThreadCopier, rather than copy constructors.
// This means it creates deep copy of each argument if necessary, and you
// don't have to create deep copies manually by calling e.g. isolatedCopy().
//
// To pass things that cannot be copied by CrossThreadCopier
// (e.g. pointers), use AllowCrossThreadAccess() explicitly.
//
// If the first argument of createCrossThreadTask
// is a pointer to a member function in class C,
// then the second argument of createCrossThreadTask
// is a raw pointer (C*) or a weak pointer (const WeakPtr<C>&) to C.
// createCrossThreadTask does not use CrossThreadCopier for the pointer,
// assuming the user of createCrossThreadTask knows that the pointer
// can be accessed from the target thread.

class CallClosureWithExecutionContextTask final : public ExecutionContextTask {
public:
    static PassOwnPtr<CallClosureWithExecutionContextTask> create(PassOwnPtr<Function<void(ExecutionContext*)>> closure)
    {
        return adoptPtr(new CallClosureWithExecutionContextTask(closure));
    }
    virtual void performTask(ExecutionContext* context) override { (*m_closure)(context); }

private:
    explicit CallClosureWithExecutionContextTask(PassOwnPtr<Function<void(ExecutionContext*)>> closure) : m_closure(closure) { }
    OwnPtr<Function<void(ExecutionContext*)>> m_closure;
};

// Templates for functions (with ExecutionContext* argument).
inline PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (*function)(ExecutionContext*))
{
    return CallClosureWithExecutionContextTask::create(bind<ExecutionContext*>(function));
}

template<typename P1, typename MP1>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (*function)(MP1, ExecutionContext*),
    const P1& parameter1)
{
    return CallClosureWithExecutionContextTask::create(bind<ExecutionContext*>(function,
        CrossThreadCopier<P1>::copy(parameter1)));
}

template<typename P1, typename MP1, typename P2, typename MP2>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (*function)(MP1, MP2, ExecutionContext*),
    const P1& parameter1, const P2& parameter2)
{
    return CallClosureWithExecutionContextTask::create(bind<ExecutionContext*>(function,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2)));
}

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (*function)(MP1, MP2, MP3, ExecutionContext*),
    const P1& parameter1, const P2& parameter2, const P3& parameter3)
{
    return CallClosureWithExecutionContextTask::create(bind<ExecutionContext*>(function,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3)));
}

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (*function)(MP1, MP2, MP3, MP4, ExecutionContext*),
    const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4)
{
    return CallClosureWithExecutionContextTask::create(bind<ExecutionContext*>(function,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3),
        CrossThreadCopier<P4>::copy(parameter4)));
}

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (*function)(MP1, MP2, MP3, MP4, MP5, ExecutionContext*),
    const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5)
{
    return CallClosureWithExecutionContextTask::create(bind<ExecutionContext*>(function,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3),
        CrossThreadCopier<P4>::copy(parameter4),
        CrossThreadCopier<P5>::copy(parameter5)));
}

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5, typename P6, typename MP6>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (*function)(MP1, MP2, MP3, MP4, MP5, MP6, ExecutionContext*),
    const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5, const P6& parameter6)
{
    return CallClosureWithExecutionContextTask::create(bind<ExecutionContext*>(function,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3),
        CrossThreadCopier<P4>::copy(parameter4),
        CrossThreadCopier<P5>::copy(parameter5),
        CrossThreadCopier<P6>::copy(parameter6)));
}

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5, typename P6, typename MP6, typename P7, typename MP7>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (*function)(MP1, MP2, MP3, MP4, MP5, MP6, MP7, ExecutionContext*),
    const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5, const P6& parameter6, const P7& parameter7)
{
    return CallClosureWithExecutionContextTask::create(bind<ExecutionContext*>(function,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3),
        CrossThreadCopier<P4>::copy(parameter4),
        CrossThreadCopier<P5>::copy(parameter5),
        CrossThreadCopier<P6>::copy(parameter6),
        CrossThreadCopier<P7>::copy(parameter7)));
}

template<typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5, typename P6, typename MP6, typename P7, typename MP7, typename P8, typename MP8>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (*function)(MP1, MP2, MP3, MP4, MP5, MP6, MP7, MP8, ExecutionContext*),
    const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5, const P6& parameter6, const P7& parameter7, const P8& parameter8)
{
    return CallClosureWithExecutionContextTask::create(bind<ExecutionContext*>(function,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3),
        CrossThreadCopier<P4>::copy(parameter4),
        CrossThreadCopier<P5>::copy(parameter5),
        CrossThreadCopier<P6>::copy(parameter6),
        CrossThreadCopier<P7>::copy(parameter7),
        CrossThreadCopier<P8>::copy(parameter8)));
}

// Templates for member functions of class C (with ExecutionContext* argument)
// + raw pointer (C*).
template<typename C>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(ExecutionContext*),
    C* p)
{
    return CallClosureWithExecutionContextTask::create(bind<ExecutionContext*>(function, p));
}

template<typename C, typename P1, typename MP1>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(MP1, ExecutionContext*),
    C* p,
    const P1& parameter1)
{
    return CallClosureWithExecutionContextTask::create(bind<ExecutionContext*>(function, p,
        CrossThreadCopier<P1>::copy(parameter1)));
}

template<typename C, typename P1, typename MP1, typename P2, typename MP2>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(MP1, MP2, ExecutionContext*),
    C* p,
    const P1& parameter1, const P2& parameter2)
{
    return CallClosureWithExecutionContextTask::create(bind<ExecutionContext*>(function, p,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2)));
}

template<typename C, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(MP1, MP2, MP3, ExecutionContext*),
    C* p,
    const P1& parameter1, const P2& parameter2, const P3& parameter3)
{
    return CallClosureWithExecutionContextTask::create(bind<ExecutionContext*>(function, p,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3)));
}

template<typename C, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(MP1, MP2, MP3, MP4, ExecutionContext*),
    C* p,
    const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4)
{
    return CallClosureWithExecutionContextTask::create(bind<ExecutionContext*>(function, p,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3),
        CrossThreadCopier<P4>::copy(parameter4)));
}

template<typename C, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(MP1, MP2, MP3, MP4, MP5, ExecutionContext*),
    C* p,
    const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5)
{
    return CallClosureWithExecutionContextTask::create(bind<ExecutionContext*>(function, p,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3),
        CrossThreadCopier<P4>::copy(parameter4),
        CrossThreadCopier<P5>::copy(parameter5)));
}

template<typename C, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5, typename P6, typename MP6>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(MP1, MP2, MP3, MP4, MP5, MP6, ExecutionContext*),
    C* p,
    const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5, const P6& parameter6)
{
    return CallClosureWithExecutionContextTask::create(bind<ExecutionContext*>(function, p,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3),
        CrossThreadCopier<P4>::copy(parameter4),
        CrossThreadCopier<P5>::copy(parameter5),
        CrossThreadCopier<P6>::copy(parameter6)));
}

template<typename C, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5, typename P6, typename MP6, typename P7, typename MP7>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(MP1, MP2, MP3, MP4, MP5, MP6, MP7, ExecutionContext*),
    C* p,
    const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5, const P6& parameter6, const P7& parameter7)
{
    return CallClosureWithExecutionContextTask::create(bind<ExecutionContext*>(function, p,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3),
        CrossThreadCopier<P4>::copy(parameter4),
        CrossThreadCopier<P5>::copy(parameter5),
        CrossThreadCopier<P6>::copy(parameter6),
        CrossThreadCopier<P7>::copy(parameter7)));
}

template<typename C, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5, typename P6, typename MP6, typename P7, typename MP7, typename P8, typename MP8>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(MP1, MP2, MP3, MP4, MP5, MP6, MP7, MP8, ExecutionContext*),
    C* p,
    const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5, const P6& parameter6, const P7& parameter7, const P8& parameter8)
{
    return CallClosureWithExecutionContextTask::create(bind<ExecutionContext*>(function, p,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3),
        CrossThreadCopier<P4>::copy(parameter4),
        CrossThreadCopier<P5>::copy(parameter5),
        CrossThreadCopier<P6>::copy(parameter6),
        CrossThreadCopier<P7>::copy(parameter7),
        CrossThreadCopier<P8>::copy(parameter8)));
}

// Templates for member function of class C + raw pointer (C*)
// which do not use CrossThreadCopier for the raw pointer (a1)
template<typename C>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(),
    C* p)
{
    return CallClosureTask::create(bind(function,
        p));
}

template<typename C, typename P1, typename MP1>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(MP1),
    C* p, const P1& parameter1)
{
    return CallClosureTask::create(bind(function,
        p,
        CrossThreadCopier<P1>::copy(parameter1)));
}

template<typename C, typename P1, typename MP1, typename P2, typename MP2>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(MP1, MP2),
    C* p, const P1& parameter1, const P2& parameter2)
{
    return CallClosureTask::create(bind(function,
        p,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2)));
}

template<typename C, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(MP1, MP2, MP3),
    C* p, const P1& parameter1, const P2& parameter2, const P3& parameter3)
{
    return CallClosureTask::create(bind(function,
        p,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3)));
}

template<typename C, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(MP1, MP2, MP3, MP4),
    C* p, const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4)
{
    return CallClosureTask::create(bind(function,
        p,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3),
        CrossThreadCopier<P4>::copy(parameter4)));
}

template<typename C, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(MP1, MP2, MP3, MP4, MP5),
    C* p, const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5)
{
    return CallClosureTask::create(bind(function,
        p,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3),
        CrossThreadCopier<P4>::copy(parameter4),
        CrossThreadCopier<P5>::copy(parameter5)));
}

// Templates for member function of class C + weak pointer (const WeakPtr<C>&)
// which do not use CrossThreadCopier for the weak pointer (a1)
template<typename C>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(),
    const WeakPtr<C>& p)
{
    return CallClosureTask::create(bind(function,
        p));
}

template<typename C, typename P1, typename MP1>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(MP1),
    const WeakPtr<C>& p, const P1& parameter1)
{
    return CallClosureTask::create(bind(function,
        p,
        CrossThreadCopier<P1>::copy(parameter1)));
}

template<typename C, typename P1, typename MP1, typename P2, typename MP2>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(MP1, MP2),
    const WeakPtr<C>& p, const P1& parameter1, const P2& parameter2)
{
    return CallClosureTask::create(bind(function,
        p,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2)));
}

template<typename C, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(MP1, MP2, MP3),
    const WeakPtr<C>& p, const P1& parameter1, const P2& parameter2, const P3& parameter3)
{
    return CallClosureTask::create(bind(function,
        p,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3)));
}

template<typename C, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(MP1, MP2, MP3, MP4),
    const WeakPtr<C>& p, const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4)
{
    return CallClosureTask::create(bind(function,
        p,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3),
        CrossThreadCopier<P4>::copy(parameter4)));
}

template<typename C, typename P1, typename MP1, typename P2, typename MP2, typename P3, typename MP3, typename P4, typename MP4, typename P5, typename MP5>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    void (C::*function)(MP1, MP2, MP3, MP4, MP5),
    const WeakPtr<C>& p, const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5)
{
    return CallClosureTask::create(bind(function,
        p,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3),
        CrossThreadCopier<P4>::copy(parameter4),
        CrossThreadCopier<P5>::copy(parameter5)));
}

// Other cases; use CrossThreadCopier for all arguments
template<typename FunctionType>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    FunctionType function)
{
    return CallClosureTask::create(bind(function));
}

template<typename FunctionType, typename P1>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    FunctionType function,
    const P1& parameter1)
{
    return CallClosureTask::create(bind(function,
        CrossThreadCopier<P1>::copy(parameter1)));
}

template<typename FunctionType, typename P1, typename P2>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    FunctionType function,
    const P1& parameter1, const P2& parameter2)
{
    return CallClosureTask::create(bind(function,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2)));
}

template<typename FunctionType, typename P1, typename P2, typename P3>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    FunctionType function,
    const P1& parameter1, const P2& parameter2, const P3& parameter3)
{
    return CallClosureTask::create(bind(function,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3)));
}

template<typename FunctionType, typename P1, typename P2, typename P3, typename P4>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    FunctionType function,
    const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4)
{
    return CallClosureTask::create(bind(function,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3),
        CrossThreadCopier<P4>::copy(parameter4)));
}

template<typename FunctionType, typename P1, typename P2, typename P3, typename P4, typename P5>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    FunctionType function,
    const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5)
{
    return CallClosureTask::create(bind(function,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3),
        CrossThreadCopier<P4>::copy(parameter4),
        CrossThreadCopier<P5>::copy(parameter5)));
}

template<typename FunctionType, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
PassOwnPtr<ExecutionContextTask> createCrossThreadTask(
    FunctionType function,
    const P1& parameter1, const P2& parameter2, const P3& parameter3, const P4& parameter4, const P5& parameter5, const P6& parameter6)
{
    return CallClosureTask::create(bind(function,
        CrossThreadCopier<P1>::copy(parameter1),
        CrossThreadCopier<P2>::copy(parameter2),
        CrossThreadCopier<P3>::copy(parameter3),
        CrossThreadCopier<P4>::copy(parameter4),
        CrossThreadCopier<P5>::copy(parameter5),
        CrossThreadCopier<P6>::copy(parameter6)));
}

} // namespace blink

#endif // CrossThreadTask_h
