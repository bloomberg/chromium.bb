/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WTF_Functional_h
#define WTF_Functional_h

#include "wtf/Assertions.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/WeakPtr.h"

namespace WTF {

// Functional.h provides a very simple way to bind a function pointer and arguments together into a function object
// that can be stored, copied and invoked, similar to how boost::bind and std::bind in C++11.

// A FunctionWrapper is a class template that can wrap a function pointer or a member function pointer and
// provide a unified interface for calling that function.
template<typename>
class FunctionWrapper;

// Bound static functions:
template<typename R, typename... P>
class FunctionWrapper<R(*)(P...)> {
public:
    typedef R ResultType;

    explicit FunctionWrapper(R(*function)(P...))
        : m_function(function)
    {
    }

    R operator()(P... params)
    {
        return m_function(params...);
    }

private:
    R(*m_function)(P...);
};

// Bound member functions:

template<typename R, typename C, typename... P>
class FunctionWrapper<R(C::*)(P...)> {
public:
    typedef R ResultType;

    explicit FunctionWrapper(R(C::*function)(P...))
        : m_function(function)
    {
    }

    R operator()(C* c, P... params)
    {
        return (c->*m_function)(params...);
    }

    R operator()(const WeakPtr<C>& c, P... params)
    {
        C* obj = c.get();
        if (!obj)
            return R();
        return (obj->*m_function)(params...);
    }

private:
    R(C::*m_function)(P...);
};

template<typename T> struct ParamStorageTraits {
    typedef T StorageType;

    static StorageType wrap(const T& value) { return value; }
    static const T& unwrap(const StorageType& value) { return value; }
};

template<typename T> struct ParamStorageTraits<PassRefPtr<T> > {
    typedef RefPtr<T> StorageType;

    static StorageType wrap(PassRefPtr<T> value) { return value; }
    static T* unwrap(const StorageType& value) { return value.get(); }
};

template<typename T> struct ParamStorageTraits<RefPtr<T> > {
    typedef RefPtr<T> StorageType;

    static StorageType wrap(RefPtr<T> value) { return value.release(); }
    static T* unwrap(const StorageType& value) { return value.get(); }
};

template<typename> class RetainPtr;

template<typename T> struct ParamStorageTraits<RetainPtr<T> > {
    typedef RetainPtr<T> StorageType;

    static StorageType wrap(const RetainPtr<T>& value) { return value; }
    static typename RetainPtr<T>::PtrType unwrap(const StorageType& value) { return value.get(); }
};

class FunctionImplBase : public ThreadSafeRefCounted<FunctionImplBase> {
public:
    virtual ~FunctionImplBase() { }
};

template<typename>
class FunctionImpl;

template<typename R, typename... A>
class FunctionImpl<R(A...)> : public FunctionImplBase {
public:
    virtual R operator()(A... args) = 0;
};

template<int boundArgsCount, typename FunctionWrapper, typename FunctionType>
class PartBoundFunctionImpl;

// Specialization for unbound functions.
template<typename FunctionWrapper, typename R, typename... P>
class PartBoundFunctionImpl<0, FunctionWrapper, R(P...)> final : public FunctionImpl<typename FunctionWrapper::ResultType(P...)> {
public:
    PartBoundFunctionImpl(FunctionWrapper functionWrapper)
        : m_functionWrapper(functionWrapper)
    {
    }

    virtual typename FunctionWrapper::ResultType operator()(P... params) override
    {
        return m_functionWrapper(params...);
    }

private:
    FunctionWrapper m_functionWrapper;
};

template<typename FunctionWrapper, typename R, typename P1, typename... P>
class PartBoundFunctionImpl<1, FunctionWrapper, R(P1, P...)> final : public FunctionImpl<typename FunctionWrapper::ResultType(P...)> {
public:
    PartBoundFunctionImpl(FunctionWrapper functionWrapper, const P1& p1)
        : m_functionWrapper(functionWrapper)
        , m_p1(ParamStorageTraits<P1>::wrap(p1))
    {
    }

    virtual typename FunctionWrapper::ResultType operator()(P... params) override
    {
        return m_functionWrapper(m_p1, params...);
    }

private:
    FunctionWrapper m_functionWrapper;
    typename ParamStorageTraits<P1>::StorageType m_p1;
};

template<typename FunctionWrapper, typename R, typename P1, typename P2, typename... P>
class PartBoundFunctionImpl<2, FunctionWrapper, R(P1, P2, P...)> final : public FunctionImpl<typename FunctionWrapper::ResultType(P...)> {
public:
    PartBoundFunctionImpl(FunctionWrapper functionWrapper, const P1& p1, const P2& p2)
        : m_functionWrapper(functionWrapper)
        , m_p1(ParamStorageTraits<P1>::wrap(p1))
        , m_p2(ParamStorageTraits<P2>::wrap(p2))
    {
    }

    virtual typename FunctionWrapper::ResultType operator()(P... params) override
    {
        return m_functionWrapper(m_p1, m_p2, params...);
    }

private:
    FunctionWrapper m_functionWrapper;
    typename ParamStorageTraits<P1>::StorageType m_p1;
    typename ParamStorageTraits<P2>::StorageType m_p2;
};

template<typename FunctionWrapper, typename R, typename P1, typename P2, typename P3, typename... P>
class PartBoundFunctionImpl<3, FunctionWrapper, R(P1, P2, P3, P...)> final : public FunctionImpl<typename FunctionWrapper::ResultType(P...)> {
public:
    PartBoundFunctionImpl(FunctionWrapper functionWrapper, const P1& p1, const P2& p2, const P3& p3)
        : m_functionWrapper(functionWrapper)
        , m_p1(ParamStorageTraits<P1>::wrap(p1))
        , m_p2(ParamStorageTraits<P2>::wrap(p2))
        , m_p3(ParamStorageTraits<P3>::wrap(p3))
    {
    }

    virtual typename FunctionWrapper::ResultType operator()(P... params) override
    {
        return m_functionWrapper(m_p1, m_p2, m_p3, params...);
    }

private:
    FunctionWrapper m_functionWrapper;
    typename ParamStorageTraits<P1>::StorageType m_p1;
    typename ParamStorageTraits<P2>::StorageType m_p2;
    typename ParamStorageTraits<P3>::StorageType m_p3;
};

template<typename FunctionWrapper, typename R, typename P1, typename P2, typename P3, typename P4, typename... P>
class PartBoundFunctionImpl<4, FunctionWrapper, R(P1, P2, P3, P4, P...)> final : public FunctionImpl<typename FunctionWrapper::ResultType(P...)> {
public:
    PartBoundFunctionImpl(FunctionWrapper functionWrapper, const P1& p1, const P2& p2, const P3& p3, const P4& p4)
        : m_functionWrapper(functionWrapper)
        , m_p1(ParamStorageTraits<P1>::wrap(p1))
        , m_p2(ParamStorageTraits<P2>::wrap(p2))
        , m_p3(ParamStorageTraits<P3>::wrap(p3))
        , m_p4(ParamStorageTraits<P4>::wrap(p4))
    {
    }

    virtual typename FunctionWrapper::ResultType operator()(P... params) override
    {
        return m_functionWrapper(m_p1, m_p2, m_p3, m_p4, params...);
    }

private:
    FunctionWrapper m_functionWrapper;
    typename ParamStorageTraits<P1>::StorageType m_p1;
    typename ParamStorageTraits<P2>::StorageType m_p2;
    typename ParamStorageTraits<P3>::StorageType m_p3;
    typename ParamStorageTraits<P4>::StorageType m_p4;
};

template<typename FunctionWrapper, typename R, typename P1, typename P2, typename P3, typename P4, typename P5, typename... P>
class PartBoundFunctionImpl<5, FunctionWrapper, R(P1, P2, P3, P4, P5, P...)> final : public FunctionImpl<typename FunctionWrapper::ResultType(P...)> {
public:
    PartBoundFunctionImpl(FunctionWrapper functionWrapper, const P1& p1, const P2& p2, const P3& p3, const P4& p4, const P5& p5)
        : m_functionWrapper(functionWrapper)
        , m_p1(ParamStorageTraits<P1>::wrap(p1))
        , m_p2(ParamStorageTraits<P2>::wrap(p2))
        , m_p3(ParamStorageTraits<P3>::wrap(p3))
        , m_p4(ParamStorageTraits<P4>::wrap(p4))
        , m_p5(ParamStorageTraits<P5>::wrap(p5))
    {
    }

    virtual typename FunctionWrapper::ResultType operator()(P... params) override
    {
        return m_functionWrapper(m_p1, m_p2, m_p3, m_p4, m_p5, params...);
    }

private:
    FunctionWrapper m_functionWrapper;
    typename ParamStorageTraits<P1>::StorageType m_p1;
    typename ParamStorageTraits<P2>::StorageType m_p2;
    typename ParamStorageTraits<P3>::StorageType m_p3;
    typename ParamStorageTraits<P4>::StorageType m_p4;
    typename ParamStorageTraits<P5>::StorageType m_p5;
};

template<typename FunctionWrapper, typename FunctionType>
class BoundFunctionImpl;

template<typename FunctionWrapper, typename R>
class BoundFunctionImpl<FunctionWrapper, R()> final : public FunctionImpl<typename FunctionWrapper::ResultType()> {
public:
    explicit BoundFunctionImpl(FunctionWrapper functionWrapper)
        : m_functionWrapper(functionWrapper)
    {
    }

    virtual typename FunctionWrapper::ResultType operator()() override
    {
        return m_functionWrapper();
    }

private:
    FunctionWrapper m_functionWrapper;
};

template<typename FunctionWrapper, typename R, typename P1>
class BoundFunctionImpl<FunctionWrapper, R(P1)> final : public FunctionImpl<typename FunctionWrapper::ResultType()> {
public:
    BoundFunctionImpl(FunctionWrapper functionWrapper, const P1& p1)
        : m_functionWrapper(functionWrapper)
        , m_p1(ParamStorageTraits<P1>::wrap(p1))
    {
    }

    virtual typename FunctionWrapper::ResultType operator()() override
    {
        return m_functionWrapper(ParamStorageTraits<P1>::unwrap(m_p1));
    }

private:
    FunctionWrapper m_functionWrapper;
    typename ParamStorageTraits<P1>::StorageType m_p1;
};

template<typename FunctionWrapper, typename R, typename P1, typename P2>
class BoundFunctionImpl<FunctionWrapper, R(P1, P2)> final : public FunctionImpl<typename FunctionWrapper::ResultType()> {
public:
    BoundFunctionImpl(FunctionWrapper functionWrapper, const P1& p1, const P2& p2)
        : m_functionWrapper(functionWrapper)
        , m_p1(ParamStorageTraits<P1>::wrap(p1))
        , m_p2(ParamStorageTraits<P2>::wrap(p2))
    {
    }

    virtual typename FunctionWrapper::ResultType operator()() override
    {
        return m_functionWrapper(ParamStorageTraits<P1>::unwrap(m_p1), ParamStorageTraits<P2>::unwrap(m_p2));
    }

private:
    FunctionWrapper m_functionWrapper;
    typename ParamStorageTraits<P1>::StorageType m_p1;
    typename ParamStorageTraits<P2>::StorageType m_p2;
};

template<typename FunctionWrapper, typename R, typename P1, typename P2, typename P3>
class BoundFunctionImpl<FunctionWrapper, R(P1, P2, P3)> final : public FunctionImpl<typename FunctionWrapper::ResultType()> {
public:
    BoundFunctionImpl(FunctionWrapper functionWrapper, const P1& p1, const P2& p2, const P3& p3)
        : m_functionWrapper(functionWrapper)
        , m_p1(ParamStorageTraits<P1>::wrap(p1))
        , m_p2(ParamStorageTraits<P2>::wrap(p2))
        , m_p3(ParamStorageTraits<P3>::wrap(p3))
    {
    }

    virtual typename FunctionWrapper::ResultType operator()() override
    {
        return m_functionWrapper(ParamStorageTraits<P1>::unwrap(m_p1), ParamStorageTraits<P2>::unwrap(m_p2), ParamStorageTraits<P3>::unwrap(m_p3));
    }

private:
    FunctionWrapper m_functionWrapper;
    typename ParamStorageTraits<P1>::StorageType m_p1;
    typename ParamStorageTraits<P2>::StorageType m_p2;
    typename ParamStorageTraits<P3>::StorageType m_p3;
};

template<typename FunctionWrapper, typename R, typename P1, typename P2, typename P3, typename P4>
class BoundFunctionImpl<FunctionWrapper, R(P1, P2, P3, P4)> final : public FunctionImpl<typename FunctionWrapper::ResultType()> {
public:
    BoundFunctionImpl(FunctionWrapper functionWrapper, const P1& p1, const P2& p2, const P3& p3, const P4& p4)
        : m_functionWrapper(functionWrapper)
        , m_p1(ParamStorageTraits<P1>::wrap(p1))
        , m_p2(ParamStorageTraits<P2>::wrap(p2))
        , m_p3(ParamStorageTraits<P3>::wrap(p3))
        , m_p4(ParamStorageTraits<P4>::wrap(p4))
    {
    }

    virtual typename FunctionWrapper::ResultType operator()() override
    {
        return m_functionWrapper(ParamStorageTraits<P1>::unwrap(m_p1), ParamStorageTraits<P2>::unwrap(m_p2), ParamStorageTraits<P3>::unwrap(m_p3), ParamStorageTraits<P4>::unwrap(m_p4));
    }

private:
    FunctionWrapper m_functionWrapper;
    typename ParamStorageTraits<P1>::StorageType m_p1;
    typename ParamStorageTraits<P2>::StorageType m_p2;
    typename ParamStorageTraits<P3>::StorageType m_p3;
    typename ParamStorageTraits<P4>::StorageType m_p4;
};

template<typename FunctionWrapper, typename R, typename P1, typename P2, typename P3, typename P4, typename P5>
class BoundFunctionImpl<FunctionWrapper, R(P1, P2, P3, P4, P5)> final : public FunctionImpl<typename FunctionWrapper::ResultType()> {
public:
    BoundFunctionImpl(FunctionWrapper functionWrapper, const P1& p1, const P2& p2, const P3& p3, const P4& p4, const P5& p5)
        : m_functionWrapper(functionWrapper)
        , m_p1(ParamStorageTraits<P1>::wrap(p1))
        , m_p2(ParamStorageTraits<P2>::wrap(p2))
        , m_p3(ParamStorageTraits<P3>::wrap(p3))
        , m_p4(ParamStorageTraits<P4>::wrap(p4))
        , m_p5(ParamStorageTraits<P5>::wrap(p5))
    {
    }

    virtual typename FunctionWrapper::ResultType operator()() override
    {
        return m_functionWrapper(ParamStorageTraits<P1>::unwrap(m_p1), ParamStorageTraits<P2>::unwrap(m_p2), ParamStorageTraits<P3>::unwrap(m_p3), ParamStorageTraits<P4>::unwrap(m_p4), ParamStorageTraits<P5>::unwrap(m_p5));
    }

private:
    FunctionWrapper m_functionWrapper;
    typename ParamStorageTraits<P1>::StorageType m_p1;
    typename ParamStorageTraits<P2>::StorageType m_p2;
    typename ParamStorageTraits<P3>::StorageType m_p3;
    typename ParamStorageTraits<P4>::StorageType m_p4;
    typename ParamStorageTraits<P5>::StorageType m_p5;
};

template<typename FunctionWrapper, typename R, typename P1, typename P2, typename P3, typename P4, typename P5, typename P6>
class BoundFunctionImpl<FunctionWrapper, R(P1, P2, P3, P4, P5, P6)> final : public FunctionImpl<typename FunctionWrapper::ResultType()> {
public:
    BoundFunctionImpl(FunctionWrapper functionWrapper, const P1& p1, const P2& p2, const P3& p3, const P4& p4, const P5& p5, const P6& p6)
        : m_functionWrapper(functionWrapper)
        , m_p1(ParamStorageTraits<P1>::wrap(p1))
        , m_p2(ParamStorageTraits<P2>::wrap(p2))
        , m_p3(ParamStorageTraits<P3>::wrap(p3))
        , m_p4(ParamStorageTraits<P4>::wrap(p4))
        , m_p5(ParamStorageTraits<P5>::wrap(p5))
        , m_p6(ParamStorageTraits<P6>::wrap(p6))
    {
    }

    virtual typename FunctionWrapper::ResultType operator()() override
    {
        return m_functionWrapper(ParamStorageTraits<P1>::unwrap(m_p1), ParamStorageTraits<P2>::unwrap(m_p2), ParamStorageTraits<P3>::unwrap(m_p3), ParamStorageTraits<P4>::unwrap(m_p4), ParamStorageTraits<P5>::unwrap(m_p5), ParamStorageTraits<P6>::unwrap(m_p6));
    }

private:
    FunctionWrapper m_functionWrapper;
    typename ParamStorageTraits<P1>::StorageType m_p1;
    typename ParamStorageTraits<P2>::StorageType m_p2;
    typename ParamStorageTraits<P3>::StorageType m_p3;
    typename ParamStorageTraits<P4>::StorageType m_p4;
    typename ParamStorageTraits<P5>::StorageType m_p5;
    typename ParamStorageTraits<P6>::StorageType m_p6;
};

class FunctionBase {
public:
    bool isNull() const
    {
        return !m_impl;
    }

protected:
    FunctionBase()
    {
    }

    explicit FunctionBase(PassRefPtr<FunctionImplBase> impl)
        : m_impl(impl)
    {
    }

    template<typename FunctionType> FunctionImpl<FunctionType>* impl() const
    {
        return static_cast<FunctionImpl<FunctionType>*>(m_impl.get());
    }

private:
    RefPtr<FunctionImplBase> m_impl;
};

template<typename>
class Function;

template<typename R, typename... A>
class Function<R(A...)> : public FunctionBase {
public:
    Function()
    {
    }

    Function(PassRefPtr<FunctionImpl<R(A...)>> impl)
        : FunctionBase(impl)
    {
    }

    R operator()(A... args) const
    {
        ASSERT(!isNull());
        return impl<R(A...)>()->operator()(args...);
    }
};

template<typename FunctionType, typename... A>
Function<typename FunctionWrapper<FunctionType>::ResultType()> bind(FunctionType function, const A... args)
{
    return Function<typename FunctionWrapper<FunctionType>::ResultType()>(adoptRef(new BoundFunctionImpl<FunctionWrapper<FunctionType>, typename FunctionWrapper<FunctionType>::ResultType (A...)>(FunctionWrapper<FunctionType>(function), args...)));
}

// Partial parameter binding.

template<typename A1, typename FunctionType, typename... A>
Function<typename FunctionWrapper<FunctionType>::ResultType(A1)> bind(FunctionType function, const A&... args)
{
    const int boundArgsCount = sizeof...(A);
    return Function<typename FunctionWrapper<FunctionType>::ResultType(A1)>(adoptRef(new PartBoundFunctionImpl<boundArgsCount, FunctionWrapper<FunctionType>, typename FunctionWrapper<FunctionType>::ResultType (A..., A1)>(FunctionWrapper<FunctionType>(function), args...)));
}

template<typename A1, typename A2, typename FunctionType, typename... A>
Function<typename FunctionWrapper<FunctionType>::ResultType(A1, A2)> bind(FunctionType function, const A&... args)
{
    const int boundArgsCount = sizeof...(A);
    return Function<typename FunctionWrapper<FunctionType>::ResultType(A1, A2)>(adoptRef(new PartBoundFunctionImpl<boundArgsCount, FunctionWrapper<FunctionType>, typename FunctionWrapper<FunctionType>::ResultType (A..., A1, A2)>(FunctionWrapper<FunctionType>(function), args...)));
}

template<typename A1, typename A2, typename A3, typename FunctionType, typename... A>
Function<typename FunctionWrapper<FunctionType>::ResultType(A1, A2, A3)> bind(FunctionType function, const A&... args)
{
    const int boundArgsCount = sizeof...(A);
    return Function<typename FunctionWrapper<FunctionType>::ResultType(A1, A2, A3)>(adoptRef(new PartBoundFunctionImpl<boundArgsCount, FunctionWrapper<FunctionType>, typename FunctionWrapper<FunctionType>::ResultType (A..., A1, A2, A3)>(FunctionWrapper<FunctionType>(function), args...)));
}

template<typename A1, typename A2, typename A3, typename A4, typename FunctionType, typename... A>
Function<typename FunctionWrapper<FunctionType>::ResultType(A1, A2, A3, A4)> bind(FunctionType function, const A&... args)
{
    const int boundArgsCount = sizeof...(A);
    return Function<typename FunctionWrapper<FunctionType>::ResultType(A1, A2, A3, A4)>(adoptRef(new PartBoundFunctionImpl<boundArgsCount, FunctionWrapper<FunctionType>, typename FunctionWrapper<FunctionType>::ResultType (A..., A1, A2, A3, A4)>(FunctionWrapper<FunctionType>(function), args...)));
}

template<typename A1, typename A2, typename A3, typename A4, typename A5, typename FunctionType, typename... A>
Function<typename FunctionWrapper<FunctionType>::ResultType(A1, A2, A3, A4, A5)> bind(FunctionType function, const A&... args)
{
    const int boundArgsCount = sizeof...(A);
    return Function<typename FunctionWrapper<FunctionType>::ResultType(A1, A2, A3, A4, A5)>(adoptRef(new PartBoundFunctionImpl<boundArgsCount, FunctionWrapper<FunctionType>, typename FunctionWrapper<FunctionType>::ResultType (A..., A1, A2, A3, A4, A5)>(FunctionWrapper<FunctionType>(function), args...)));
}

template<typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename FunctionType, typename... A>
Function<typename FunctionWrapper<FunctionType>::ResultType(A1, A2, A3, A4, A5, A6)> bind(FunctionType function, const A&... args)
{
    const int boundArgsCount = sizeof...(A);
    return Function<typename FunctionWrapper<FunctionType>::ResultType(A1, A2, A3, A4, A5, A6)>(adoptRef(new PartBoundFunctionImpl<boundArgsCount, FunctionWrapper<FunctionType>, typename FunctionWrapper<FunctionType>::ResultType (A..., A1, A2, A3, A4, A5, A6)>(FunctionWrapper<FunctionType>(function), args...)));
}

typedef Function<void()> Closure;

}

using WTF::Function;
using WTF::bind;
using WTF::Closure;

#endif // WTF_Functional_h
