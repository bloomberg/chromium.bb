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

#include "base/tuple.h"
#include "wtf/Allocator.h"
#include "wtf/Assertions.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/RefPtr.h"
#include "wtf/ThreadSafeRefCounted.h"
#include "wtf/WeakPtr.h"
#include <tuple>

namespace WTF {

// Functional.h provides a very simple way to bind a function pointer and arguments together into a function object
// that can be stored, copied and invoked, similar to how boost::bind and std::bind in C++11.

// Use threadSafeBind() or createCrossThreadTask() if the function/task is
// called on a (potentially) different thread from the current thread.

// A FunctionWrapper is a class template that can wrap a function pointer or a member function pointer and
// provide a unified interface for calling that function.
template<typename>
class FunctionWrapper;

// Bound static functions:
template<typename R, typename... Params>
class FunctionWrapper<R(*)(Params...)> {
    DISALLOW_NEW();
public:
    typedef R ResultType;

    explicit FunctionWrapper(R(*function)(Params...))
        : m_function(function)
    {
    }

    R operator()(Params... params)
    {
        return m_function(params...);
    }

private:
    R(*m_function)(Params...);
};

// Bound member functions:

template<typename R, typename C, typename... Params>
class FunctionWrapper<R(C::*)(Params...)> {
    DISALLOW_NEW();
public:
    typedef R ResultType;

    explicit FunctionWrapper(R(C::*function)(Params...))
        : m_function(function)
    {
    }

    R operator()(C* c, Params... params)
    {
        return (c->*m_function)(params...);
    }

    R operator()(PassOwnPtr<C> c, Params... params)
    {
        return (c.get()->*m_function)(params...);
    }

    R operator()(const WeakPtr<C>& c, Params... params)
    {
        C* obj = c.get();
        if (!obj)
            return R();
        return (obj->*m_function)(params...);
    }

private:
    R(C::*m_function)(Params...);
};

template<typename T> struct ParamStorageTraits {
    typedef T StorageType;

    static StorageType wrap(const T& value) { return value; }
    static const T& unwrap(const StorageType& value) { return value; }
};

template<typename T> struct ParamStorageTraits<PassRefPtr<T>> {
    typedef RefPtr<T> StorageType;

    static StorageType wrap(PassRefPtr<T> value) { return value; }
    static T* unwrap(const StorageType& value) { return value.get(); }
};

template<typename T> struct ParamStorageTraits<RefPtr<T>> {
    typedef RefPtr<T> StorageType;

    static StorageType wrap(RefPtr<T> value) { return value.release(); }
    static T* unwrap(const StorageType& value) { return value.get(); }
};

template<typename> class RetainPtr;

template<typename T> struct ParamStorageTraits<RetainPtr<T>> {
    typedef RetainPtr<T> StorageType;

    static StorageType wrap(const RetainPtr<T>& value) { return value; }
    static typename RetainPtr<T>::PtrType unwrap(const StorageType& value) { return value.get(); }
};

template<typename>
class Function;

template<typename R, typename... Args>
class Function<R(Args...)> {
    USING_FAST_MALLOC(Function);
    WTF_MAKE_NONCOPYABLE(Function);
public:
    virtual ~Function() { }
    virtual R operator()(Args... args) = 0;
protected:
    Function() = default;
};

template <typename BoundParametersTuple, typename FunctionWrapper, typename... UnboundParameters>
class PartBoundFunctionImpl;

template <typename... BoundParameters, typename FunctionWrapper, typename... UnboundParameters>
class PartBoundFunctionImpl<std::tuple<BoundParameters...>, FunctionWrapper, UnboundParameters...> final : public Function<typename FunctionWrapper::ResultType(UnboundParameters...)> {
public:
    explicit PartBoundFunctionImpl(FunctionWrapper functionWrapper, const BoundParameters&... bound)
        : m_functionWrapper(functionWrapper)
        , m_bound(ParamStorageTraits<BoundParameters>::wrap(bound)...)
    {
    }

    typename FunctionWrapper::ResultType operator()(UnboundParameters... unbound) override
    {
        // What we really want to do is to call m_functionWrapper(m_bound..., unbound...), but to do that we need to
        // pass a list of indices to a worker function template.
        return callInternal(unbound..., base::MakeIndexSequence<sizeof...(BoundParameters)>());
    }

private:
    template <std::size_t... boundIndices>
    typename FunctionWrapper::ResultType callInternal(UnboundParameters... unbound, const base::IndexSequence<boundIndices...>&)
    {
        // Get each element in m_bound, unwrap them, and call the function with the desired arguments.
        return m_functionWrapper(ParamStorageTraits<BoundParameters>::unwrap(std::get<boundIndices>(m_bound))..., unbound...);
    }

    FunctionWrapper m_functionWrapper;
    std::tuple<typename ParamStorageTraits<BoundParameters>::StorageType...> m_bound;
};


template<typename... UnboundParameters, typename FunctionType, typename... BoundParameters>
PassOwnPtr<Function<typename FunctionWrapper<FunctionType>::ResultType(UnboundParameters...)>> bind(FunctionType function, const BoundParameters&... boundParameters)
{
    // Bound parameters' types are wrapped with std::tuple so we can pass two template parameter packs (bound
    // parameters and unbound) to PartBoundFunctionImpl. Note that a tuple of this type isn't actually created;
    // std::tuple<> is just for carrying the bound parameters' types. Any other class template taking a type parameter
    // pack can be used instead of std::tuple. std::tuple is used just because it's most convenient for this purpose.
    using BoundFunctionType = PartBoundFunctionImpl<std::tuple<BoundParameters...>, FunctionWrapper<FunctionType>, UnboundParameters...>;
    return adoptPtr(new BoundFunctionType(FunctionWrapper<FunctionType>(function), boundParameters...));
}

typedef Function<void()> Closure;

} // namespace WTF

using WTF::Function;
using WTF::bind;
using WTF::Closure;

#endif // WTF_Functional_h
